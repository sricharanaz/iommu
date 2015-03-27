/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/dma-iommu.h>
#include <linux/dma-attrs.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/iommu.h>
//#include <linux/msm_iommu_domains.h>
#ifdef CONFIG_ION
#include <linux/msm_ion.h>
#endif
#include <linux/slab.h>
#include <linux/types.h>
#include <media/msm_vidc.h>
#include <media/videobuf2-dma-sg.h>
#include <media/videobuf2-dma-contig.h>
#include "msm_vidc_debug.h"
#include "msm_vidc_resources.h"

#define NEW_SMMU

struct smem_client {
	int mem_type;
	void *clnt;
	struct msm_vidc_platform_resources *res;
};


static int get_device_address(struct smem_client *smem_client,
		struct dma_buf *buf, unsigned long align,
		dma_addr_t *iova, unsigned long *buffer_size,
		unsigned long flags, enum hal_buffer buffer_type,
		struct dma_mapping_info *mapping_info)
{
	int rc = 0;
	struct dma_buf_attachment *attach;
	struct sg_table *table = NULL;
	struct context_bank_info *cb = NULL;
	phys_addr_t phys, orig_phys;

	if (!iova || !buffer_size || !buf || !smem_client || !mapping_info) {
		dprintk(VIDC_ERR, "Invalid params: %p, %p, %p, %p\n",
				smem_client, buf, iova, buffer_size);
		return -EINVAL;
	}


	cb = msm_smem_get_context_bank(smem_client, flags & SMEM_SECURE,
			buffer_type);
	if (!cb) {
		dprintk(VIDC_ERR,
				"%s: Failed to get context bank device\n",
				__func__);
		rc = -EIO;
		goto mem_map_failed;
	}

	/* Prepare a dma buf for dma on the given device */
	attach = dma_buf_attach(buf, cb->dev);
	if (IS_ERR_OR_NULL(attach)) {
		rc = PTR_ERR(attach) ?: -ENOMEM;
		dprintk(VIDC_ERR, "Failed to attach dmabuf\n");
		goto mem_buf_attach_failed;
	}

	/* Get the scatterlist for the given attachment */
	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(table)) {
		rc = PTR_ERR(table) ?: -ENOMEM;
		dprintk(VIDC_ERR, "Failed to map table\n");
		goto mem_map_table_failed;
	}

	/* debug trace's need to be updated later */
	trace_msm_smem_buffer_iommu_op_start("MAP", 0, 0,
			align, *iova, *buffer_size);

	/* Map a scatterlist into an SMMU */
	rc = dma_map_sg(cb->dev, table->sgl, table->nents,
			DMA_BIDIRECTIONAL);
	if (!rc) {
		dprintk(VIDC_ERR, "dma_map_sg failed! (%d != %d)\n",
				rc, table->nents);
		goto mem_map_sg_failed;
	}
	if (table->sgl) {
		dprintk(VIDC_DBG,
				"%s: DMA buf: %p, device: %p, attach: %p, table: %p, table sgl: %p, rc: %d, dma_address: %pa\n",
				__func__, buf, cb->dev, attach,
				table, table->sgl, rc,
				&table->sgl->dma_address);

		*iova = table->sgl->dma_address;
		*buffer_size = table->sgl->dma_length;
	} else {
		dprintk(VIDC_ERR, "sgl is NULL\n");
		rc = -ENOMEM;
		goto mem_map_sg_failed;
	}

	/* Translation check for debugging */
	orig_phys = sg_phys(table->sgl);

	phys = iommu_iova_to_phys(cb->mapping->domain, *iova);
	if (phys != orig_phys) {
		dprintk(VIDC_ERR,
				"%s iova_to_phys failed!!! mapped: %pa, got: %pa\n",
				__func__, &orig_phys, &phys);
		rc = -EIO;
		goto mem_iova_to_phys_failed;
	}

	mapping_info->dev = cb->dev;
	mapping_info->mapping = cb->mapping;
	mapping_info->table = table;
	mapping_info->attach = attach;
	mapping_info->buf = buf;

	trace_msm_smem_buffer_iommu_op_end("MAP", 0, 0,
			align, *iova, *buffer_size);

	dprintk(VIDC_DBG, "mapped dma buf %p to %pa\n", buf, iova);
	return 0;
mem_iova_to_phys_failed:
	dma_unmap_sg(cb->dev, table->sgl, table->nents, DMA_BIDIRECTIONAL);
mem_map_sg_failed:
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
mem_map_table_failed:
	dma_buf_detach(buf, attach);
mem_buf_attach_failed:
	dma_buf_put(buf);
mem_map_failed:
	return rc;
}

static void put_device_address(struct smem_client *smem_client,
	u32 flags, struct dma_mapping_info *mapping_info,
	enum hal_buffer buffer_type)
{
	if (!smem_client || !mapping_info) {
		dprintk(VIDC_WARN, "Invalid params: %p, %p\n",
				smem_client, mapping_info);
		return;
	}

	if (!mapping_info->dev || !mapping_info->table ||
		!mapping_info->buf || !mapping_info->attach) {
			dprintk(VIDC_WARN, "Invalid params:\n");
			return;
	}

	if (is_iommu_present(smem_client->res)) {
		dprintk(VIDC_DBG,
			"Calling dma_unmap_sg - device: %p, address: %pa, buf: %p, table: %p, attach: %p\n",
			mapping_info->dev,
			&mapping_info->table->sgl->dma_address,
			mapping_info->buf, mapping_info->table,
			mapping_info->attach);

		trace_msm_smem_buffer_iommu_op_start("UNMAP", 0, 0, 0, 0, 0);
		dma_unmap_sg(mapping_info->dev, mapping_info->table->sgl,
			mapping_info->table->nents, DMA_BIDIRECTIONAL);
		dma_buf_unmap_attachment(mapping_info->attach,
			mapping_info->table, DMA_BIDIRECTIONAL);
		dma_buf_detach(mapping_info->buf, mapping_info->attach);
		dma_buf_put(mapping_info->buf);
		trace_msm_smem_buffer_iommu_op_end("UNMAP", 0, 0, 0, 0, 0);
	}
}

#ifdef CONFIG_ION
static int ion_get_device_address(struct smem_client *smem_client,
		struct ion_handle *hndl, unsigned long align,
		ion_phys_addr_t *iova, unsigned long *buffer_size,
		unsigned long flags, enum hal_buffer buffer_type,
		struct dma_mapping_info *mapping_info)
{
	
	clnt = smem_client->clnt;
	if (!clnt) {
		dprintk(VIDC_ERR, "Invalid client\n");
		return -EINVAL;
	}
	if (is_iommu_present(smem_client->res)) {
		/* Convert an Ion handle to a dma buf */
		buf = ion_share_dma_buf(clnt, hndl);
		if (IS_ERR_OR_NULL(buf)) {
			rc = PTR_ERR(buf) ?: -ENOMEM;
			dprintk(VIDC_ERR, "Share ION buf to DMA failed\n");
			goto mem_map_failed;
		}
	} else {
		dprintk(VIDC_DBG, "Using physical memory address\n");
		rc = ion_phys(clnt, hndl, iova, (size_t *)buffer_size);
		if (rc) {
			dprintk(VIDC_ERR, "ion memory map failed - %d\n", rc);
			goto mem_map_failed;
		}
	}
}

static int ion_user_to_kernel(struct smem_client *client, int fd, u32 offset,
		struct msm_smem *mem, enum hal_buffer buffer_type)
{
	struct ion_handle *hndl;
	ion_phys_addr_t iova = 0;
	unsigned long buffer_size = 0;
	int rc = 0;
	unsigned long align = SZ_4K;

	hndl = ion_import_dma_buf(client->clnt, fd);
	dprintk(VIDC_DBG, "%s ion handle: %p\n", __func__, hndl);
	if (IS_ERR_OR_NULL(hndl)) {
		dprintk(VIDC_ERR, "Failed to get handle: %p, %d, %d, %p\n",
				client, fd, offset, hndl);
		rc = -ENOMEM;
		goto fail_import_fd;
	}
	mem->kvaddr = NULL;
	rc = ion_handle_get_flags(client->clnt, hndl, &mem->flags);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get ion flags: %d\n", rc);
		goto fail_device_address;
	}

	mem->buffer_type = buffer_type;
	if (mem->flags & SMEM_SECURE)
		align = ALIGN(align, SZ_1M);

	rc = get_device_address(client, hndl, align, &iova, &buffer_size,
				mem->flags, buffer_type, &mem->mapping_info);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get device address: %d\n", rc);
		goto fail_device_address;
	}

	mem->mem_type = client->mem_type;
	mem->smem_priv = hndl;
	mem->device_addr = iova;
	mem->size = buffer_size;
	if ((u32)mem->device_addr != iova) {
		dprintk(VIDC_ERR, "iova(%pa) truncated to %#x",
			&iova, (u32)mem->device_addr);
		goto fail_device_address;
	}
	dprintk(VIDC_DBG,
		"%s: ion_handle = %p, fd = %d, device_addr = %pa, size = %zx, kvaddr = %p, buffer_type = %d, flags = %#lx\n",
		__func__, mem->smem_priv, fd, &mem->device_addr, mem->size,
		mem->kvaddr, mem->buffer_type, mem->flags);
	return rc;
fail_device_address:
	ion_free(client->clnt, hndl);
fail_import_fd:
	return rc;
}

static int alloc_ion_mem(struct smem_client *client, size_t size, u32 align,
	u32 flags, enum hal_buffer buffer_type, struct msm_smem *mem,
	int map_kernel)
{
	struct ion_handle *hndl;
	ion_phys_addr_t iova = 0;
	unsigned long buffer_size = 0;
	unsigned long heap_mask = 0;
	int rc = 0;

	align = ALIGN(align, SZ_4K);
	size = ALIGN(size, SZ_4K);

	if (flags & SMEM_SECURE) {
		size = ALIGN(size, SZ_1M);
		align = ALIGN(align, SZ_1M);
		flags |= ION_FLAG_ALLOW_NON_CONTIG;
	}

	if (is_iommu_present(client->res)) {
		heap_mask = ION_HEAP(ION_IOMMU_HEAP_ID);
	} else {
		dprintk(VIDC_DBG,
			"allocate shared memory from adsp heap size %zx align %d\n",
			size, align);
		heap_mask = ION_HEAP(ION_ADSP_HEAP_ID);
	}

	if (flags & SMEM_SECURE)
		heap_mask = ION_HEAP(ION_CP_MM_HEAP_ID);

	trace_msm_smem_buffer_ion_op_start("ALLOC", (u32)buffer_type,
		heap_mask, size, align, flags, map_kernel);
	hndl = ion_alloc(client->clnt, size, align, heap_mask, flags);
	if (IS_ERR_OR_NULL(hndl)) {
		dprintk(VIDC_ERR,
		"Failed to allocate shared memory = %p, %zx, %d, %#x\n",
		client, size, align, flags);
		rc = -ENOMEM;
		goto fail_shared_mem_alloc;
	}
	trace_msm_smem_buffer_ion_op_end("ALLOC", (u32)buffer_type,
		heap_mask, size, align, flags, map_kernel);
	mem->mem_type = client->mem_type;
	mem->smem_priv = hndl;
	mem->flags = flags;
	mem->buffer_type = buffer_type;
	if (map_kernel) {
		mem->kvaddr = ion_map_kernel(client->clnt, hndl);
		if (IS_ERR_OR_NULL(mem->kvaddr)) {
			dprintk(VIDC_ERR,
				"Failed to map shared mem in kernel\n");
			rc = -EIO;
			goto fail_map;
		}
	} else {
		mem->kvaddr = NULL;
	}

	rc = get_device_address(client, hndl, align, &iova, &buffer_size,
				flags, buffer_type, &mem->mapping_info);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get device address: %d\n",
			rc);
		goto fail_device_address;
	}
	mem->device_addr = iova;
	if ((u32)mem->device_addr != iova) {
		dprintk(VIDC_ERR, "iova(%pa) truncated to %#x",
			&iova, (u32)mem->device_addr);
		goto fail_device_address;
	}
	mem->size = size;
	dprintk(VIDC_DBG,
		"%s: ion_handle = %p, device_addr = %pa, size = %#zx, kvaddr = %p, buffer_type = %#x, flags = %#lx\n",
		__func__, mem->smem_priv, &mem->device_addr,
		mem->size, mem->kvaddr, mem->buffer_type, mem->flags);
	return rc;
fail_device_address:
	if (mem->kvaddr)
		ion_unmap_kernel(client->clnt, hndl);
fail_map:
	ion_free(client->clnt, hndl);
fail_shared_mem_alloc:
	return rc;
}

static void free_ion_mem(struct smem_client *client, struct msm_smem *mem)
{
	dprintk(VIDC_DBG,
		"%s: ion_handle = %p, device_addr = %pa, size = %#zx, kvaddr = %p, buffer_type = %#x\n",
		__func__, mem->smem_priv, &mem->device_addr,
		mem->size, mem->kvaddr, mem->buffer_type);

	if (mem->device_addr)
		put_device_address(client, mem->smem_priv, mem->flags,
			&mem->mapping_info, mem->buffer_type);

	if (mem->kvaddr)
		ion_unmap_kernel(client->clnt, mem->smem_priv);
	if (mem->smem_priv) {
		trace_msm_smem_buffer_ion_op_start("FREE",
				(u32)mem->buffer_type, -1, mem->size, -1,
				mem->flags, -1);
		dprintk(VIDC_DBG,
			"%s: Freeing handle %p, client: %p\n",
			__func__, mem->smem_priv, client->clnt);
		ion_free(client->clnt, mem->smem_priv);
		trace_msm_smem_buffer_ion_op_end("FREE", (u32)mem->buffer_type,
			-1, mem->size, -1, mem->flags, -1);
	}
}

static void *ion_new_client(void)
{
	struct ion_client *client = NULL;
	client = msm_ion_client_create("video_client");
	if (!client)
		dprintk(VIDC_ERR, "Failed to create smem client\n");
	return client;
};

static void ion_delete_client(struct smem_client *client)
{
	ion_client_destroy(client->clnt);
}

static int ion_cache_operations(struct smem_client *client,
	struct msm_smem *mem, enum smem_cache_ops cache_op)
{
	unsigned long ionflag = 0;
	int rc = 0;
	int msm_cache_ops = 0;
	if (!mem || !client) {
		dprintk(VIDC_ERR, "Invalid params: %p, %p\n",
			mem, client);
		return -EINVAL;
	}
	rc = ion_handle_get_flags(client->clnt,	mem->smem_priv,
		&ionflag);
	if (rc) {
		dprintk(VIDC_ERR,
			"ion_handle_get_flags failed: %d\n", rc);
		goto cache_op_failed;
	}
	if (ION_IS_CACHED(ionflag)) {
		switch (cache_op) {
		case SMEM_CACHE_CLEAN:
			msm_cache_ops = ION_IOC_CLEAN_CACHES;
			break;
		case SMEM_CACHE_INVALIDATE:
			msm_cache_ops = ION_IOC_INV_CACHES;
			break;
		case SMEM_CACHE_CLEAN_INVALIDATE:
			msm_cache_ops = ION_IOC_CLEAN_INV_CACHES;
			break;
		default:
			dprintk(VIDC_ERR, "cache operation not supported\n");
			rc = -EINVAL;
			goto cache_op_failed;
		}
		rc = msm_ion_do_cache_op(client->clnt,
				(struct ion_handle *)mem->smem_priv,
				0, (unsigned long)mem->size,
				msm_cache_ops);
		if (rc) {
			dprintk(VIDC_ERR,
					"cache operation failed %d\n", rc);
			goto cache_op_failed;
		}
	}
cache_op_failed:
	return rc;
}
#endif

static int alloc_dma_mem(struct smem_client *client, size_t size, u32 align,
	u32 flags, enum hal_buffer buffer_type, struct msm_smem *mem,
	int map_kernel)
{
	int rc = 0;
	enum dma_data_direction dma_dir;
	unsigned long buffer_size = 0;
	dma_addr_t iova = 0;

	align = ALIGN(align, SZ_4K);
	size = ALIGN(size, SZ_4K);
	size = PAGE_ALIGN(size);

	dprintk(VIDC_DBG, "alloc_dma_mem type %d, size %zu\n", buffer_type, size);

	switch (buffer_type) {
	case HAL_BUFFER_INPUT:
		dma_dir = DMA_TO_DEVICE;
		break;
	case HAL_BUFFER_OUTPUT:
		dma_dir = DMA_FROM_DEVICE;
		break;
	default:
		dma_dir = DMA_FROM_DEVICE;
		break;
	}

	mem->mem_type = client->mem_type;
	mem->flags = flags;
	mem->buffer_type = buffer_type;
	mem->kvaddr = NULL;
	mem->size = size;
#ifndef NEW_SMMU
	rc = msm_smem_get_domain_partition(client, flags, buffer_type,
			&domain, &partition);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get domain and partition: %d\n",
				rc);
		return -EINVAL;
	}
	mem->smem_priv = vb2_dma_sg_memops.alloc(client->clnt, size, 0);
	if (mem->smem_priv == NULL) {
		dprintk(VIDC_ERR, "VB2 SG memops alloc failed %zu", size);
		return -EINVAL;
	}
	//Get dma_buf from sg_table
	mem->mapping_info.table = vb2_dma_sg_memops.cookie(mem->smem_priv);
	mem->mapping_info.buf = vb2_dma_sg_memops.get_dmabuf(mem->smem_priv);
	if (IS_ERR(mem->mapping_info.buf)) {
		rc = PTR_ERR(mem->mapping_info.buf);
		goto err_put;
	}
	rc = msm_map_dma_buf(mem->mapping_info.buf, mem->mapping_info.table, domain, partition, align, 0,
				&mem->device_addr, &buffer_size, 0, 0);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get device address: %d\n",
			rc);
		goto err_put;
	}
	if (map_kernel) {
		dprintk(VIDC_DBG, "alloc_dma_mem, map_kernel \n");
		dma_buf_begin_cpu_access(mem->mapping_info.buf, 0, size, dma_dir);
		mem->kvaddr = dma_buf_vmap(mem->mapping_info.buf);
		if (!mem->kvaddr) {
			dprintk(VIDC_ERR,
				"Failed to map shared mem in kernel\n");
			rc = -EIO;
			goto err_kunmap;
		}
		dprintk(VIDC_DBG, "alloc_dma_mem, map_kernel %p\n", mem->kvaddr);
	}

#else

	mem->smem_priv = vb2_dma_contig_memops.alloc(client->clnt, size, dma_dir, 0);
	if (mem->smem_priv == NULL) {
		dprintk(VIDC_ERR, "VB2 contig memops alloc failed %zu\n", size);
		return -EINVAL;
	}
	mem->device_addr = (dma_addr_t) vb2_dma_contig_memops.cookie(mem->smem_priv);
	if (map_kernel) {
		dprintk(VIDC_DBG, "alloc_dma_mem, map_kernel\n");
		mem->kvaddr = vb2_dma_contig_memops.vaddr(mem->smem_priv);
		if (!mem->kvaddr) {
			dprintk(VIDC_WARN, "Failed to map dma buf %pad\n", &mem->device_addr);
		}
	}

	rc = get_device_address(client, vb2_dma_contig_memops.get_dmabuf(mem->smem_priv, O_CLOEXEC),
			align, &iova, &buffer_size, flags, buffer_type, &mem->mapping_info);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get device address: %d\n",
				rc);
		goto fail_device_address;
	}
	mem->device_addr = iova;
	if ((u32)mem->device_addr != iova) {
		dprintk(VIDC_ERR, "iova(%pa) truncated to %#x",
				&iova, (u32)mem->device_addr);
		goto fail_device_address;
	}
	mem->size = size;
	dprintk(VIDC_DBG,
			"%s: ion_handle = %p, device_addr = %pa, size = %#zx, kvaddr = %p, buffer_type = %#x, flags = %#lx\n",
			__func__, mem->smem_priv, &mem->device_addr,
			mem->size, mem->kvaddr, mem->buffer_type, mem->flags);

#endif

	dprintk(VIDC_DBG,
		"%s: sg_handle = 0x%p, device_addr = 0x%x, size = %zu, kvaddr = 0x%p, buffer_type = %d\n",
		__func__, mem->smem_priv, (u32)mem->device_addr,
		mem->size, mem->kvaddr, mem->buffer_type);
	return rc;
#ifndef NEW_SMMU
err_kunmap:
	dma_buf_end_cpu_access(mem->mapping_info.buf, 0, size, DMA_FROM_DEVICE);
err_put:
	vb2_dma_sg_memops.put(mem->smem_priv);
#else
fail_device_address:
#endif
	return rc;

}

static void free_dma_mem(struct smem_client *client, struct msm_smem *mem)
{
#ifndef NEW_SMMU
	int domain, partition, rc;

	dprintk(VIDC_DBG,
		"%s: mem priv = 0x%p, device_addr = 0x%pa, size = 0x%zx, kvaddr = 0x%p, buffer_type = 0x%x\n",
		__func__, mem->smem_priv, &mem->device_addr,
		mem->size, mem->kvaddr, mem->buffer_type);
	rc = msm_smem_get_domain_partition((void *)client, mem->flags,
			mem->buffer_type, &domain, &partition);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to get domain, partition: %d\n", rc);
		return;
	}

	if (mem->device_addr)
		msm_unmap_dma_buf(mem->mapping_info.table, domain, partition);
	if (mem->smem_priv) {
		vb2_dma_sg_memops.put(mem->smem_priv);
	}
	if (mem->kvaddr) {
		dma_buf_end_cpu_access(mem->mapping_info.buf, 0, mem->size, DMA_FROM_DEVICE);
		dma_buf_vunmap(mem->mapping_info.buf, mem->kvaddr);
	}
#else
	if (mem->smem_priv) {
		vb2_dma_contig_memops.put(mem->smem_priv);
	}
#endif
}

struct msm_smem *msm_smem_user_to_kernel(void *clt, int fd, u32 offset,
		enum hal_buffer buffer_type)
{
	struct smem_client *client = clt;
	int rc = 0;
	struct msm_smem *mem;
	if (fd < 0) {
		dprintk(VIDC_ERR, "Invalid fd: %d\n", fd);
		return NULL;
	}
	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		dprintk(VIDC_ERR, "Failed to allocte shared mem\n");
		return NULL;
	}
	switch (client->mem_type) {
#ifdef CONFIG_ION
	case SMEM_ION:
		rc = ion_user_to_kernel(clt, fd, offset, mem, buffer_type);
		break;
#endif
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		rc = -EINVAL;
		break;
	}
	if (rc) {
		dprintk(VIDC_ERR, "Failed to allocate shared memory\n");
		kfree(mem);
		mem = NULL;
	}
	return mem;
}

int msm_smem_cache_operations(void *clt, struct msm_smem *mem,
		enum smem_cache_ops cache_op)
{
	struct smem_client *client = clt;
	int rc = 0;
	if (!client) {
		dprintk(VIDC_ERR, "Invalid params: %p\n",
			client);
		return -EINVAL;
	}
	switch (client->mem_type) {
#ifdef CONFIG_ION
	case SMEM_ION:
		rc = ion_cache_operations(client, mem, cache_op);
		if (rc)
			dprintk(VIDC_ERR,
			"Failed cache operations: %d\n", rc);
		break;
#endif
	case SMEM_DMA:
		dprintk(VIDC_ERR,
			"Ignore dma cache operations: %d\n", rc);
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		break;
	}
	return rc;
}

void *msm_smem_new_client(enum smem_type mtype,
		void *platform_resources)
{
	struct smem_client *client = NULL;
	void *clnt = NULL;
	struct msm_vidc_platform_resources *res = platform_resources;
	switch (mtype) {
#ifdef CONFIG_ION
	case SMEM_ION:
		clnt = ion_new_client();
		break;
#endif
	case SMEM_DMA:
#ifdef NEW_SMMU
		clnt = vb2_dma_contig_init_ctx(&(res->pdev->dev));
#else
		clnt = vb2_dma_sg_init_ctx(&(res->pdev->dev));
#endif
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		break;
	}
	if (clnt) {
		client = kzalloc(sizeof(*client), GFP_KERNEL);
		if (client) {
			client->mem_type = mtype;
			client->clnt = clnt;
			client->res = res;
		}
	} else {
		dprintk(VIDC_ERR, "Failed to create new client: mtype = %d\n",
			mtype);
	}
	return client;
}

struct msm_smem *msm_smem_alloc(void *clt, size_t size, u32 align, u32 flags,
		enum hal_buffer buffer_type, int map_kernel)
{
	struct smem_client *client;
	int rc = 0;
	struct msm_smem *mem;
	client = clt;
	if (!client) {
		dprintk(VIDC_ERR, "Invalid  client passed\n");
		return NULL;
	}
	if (!size) {
		dprintk(VIDC_ERR, "No need to allocate memory of size: %zx\n",
			size);
		return NULL;
	}
	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		dprintk(VIDC_ERR, "Failed to allocate shared mem\n");
		return NULL;
	}
	switch (client->mem_type) {
#ifdef CONFIG_ION
	case SMEM_ION:
		rc = alloc_ion_mem(client, size, align, flags, buffer_type,
					mem, map_kernel);
		break;
#endif
	case SMEM_DMA:
		rc = alloc_dma_mem(client, size, align, flags, buffer_type,
					mem, map_kernel);
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		rc = -EINVAL;
		break;
	}
	if (rc) {
		dprintk(VIDC_ERR, "Failed to allocate shared memory\n");
		kfree(mem);
		mem = NULL;
	}
	return mem;
}

void msm_smem_free(void *clt, struct msm_smem *mem)
{
	struct smem_client *client = clt;
	if (!client || !mem) {
		dprintk(VIDC_ERR, "Invalid  client/handle passed\n");
		return;
	}
	switch (client->mem_type) {
#ifdef CONFIG_ION
	case SMEM_ION:
		free_ion_mem(client, mem);
		break;
#endif
	case SMEM_DMA:
		free_dma_mem(client, mem);
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		break;
	}
	kfree(mem);
};

void msm_smem_delete_client(void *clt)
{
	struct smem_client *client = clt;
	if (!client) {
		dprintk(VIDC_ERR, "Invalid  client passed\n");
		return;
	}
	switch (client->mem_type) {
#ifdef CONFIG_ION
	case SMEM_ION:
		ion_delete_client(client);
		break;
#endif
	case SMEM_DMA:
#ifdef NEW_SMMU
		vb2_dma_contig_cleanup_ctx(client->clnt);
#else
		vb2_dma_sg_cleanup_ctx(client->clnt);
#endif
		break;
	default:
		dprintk(VIDC_ERR, "Mem type not supported\n");
		break;
	}
	kfree(client);
}

struct context_bank_info *msm_smem_get_context_bank(void *clt,
			bool is_secure, enum hal_buffer buffer_type)
{
	struct smem_client *client = clt;
	struct context_bank_info *cb = NULL, *match = NULL;

	if (!clt) {
		dprintk(VIDC_ERR, "%s - invalid params\n", __func__);
		return NULL;
	}

	list_for_each_entry(cb, &client->res->context_banks, list) {
		if (cb->is_secure == is_secure &&
				cb->buffer_type & buffer_type) {
			match = cb;
			dprintk(VIDC_DBG,
				"context bank found for device: %p mapping: %p\n",
				match->dev, match->mapping);
			break;
		}
	}

	return match;
}

void *msm_smem_get_alloc_ctx(void *mem_client)
{
	struct smem_client *client = mem_client;
	return client->clnt;
}


struct msm_smem* msm_smem_map_dma_buf(void* smem_client, struct sg_table* sgt,
			struct dma_buf* dbuf, enum hal_buffer buffer_type)
{
	struct smem_client *client = smem_client;
	int rc = 0;
	struct msm_smem *mem;
	dma_addr_t iova = 0;
	unsigned long buffer_size = 0;
	u32 flags = 0;
	struct dma_buf_attachment *attach;
	struct context_bank_info *cb = NULL;

	dprintk(VIDC_DBG, "msm_smem_map_dma_buf type %d\n", buffer_type);

	if (client == NULL || sgt == NULL || dbuf == NULL) {
		dprintk(VIDC_ERR, "%s: Invalid  params \n", __func__);
		return NULL;
	}

	if (is_iommu_present(client->res)) {
		cb = msm_smem_get_context_bank(smem_client, flags & SMEM_SECURE,
				buffer_type);
		if (!cb) {
			dprintk(VIDC_ERR,
					"%s: Failed to get context bank device\n",
					__func__);
			return NULL;
		}
	}

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		dprintk(VIDC_ERR, "Failed to allocte shared mem\n");
		return NULL;
	}

	/* Prepare a dma buf for dma on the given device */
	attach = dma_buf_attach(dbuf, cb->dev);
	if (IS_ERR_OR_NULL(attach)) {
		rc = PTR_ERR(attach) ?: -ENOMEM;
		dprintk(VIDC_ERR, "Failed to attach dmabuf\n");
		goto mem_buf_attach_failed;
	}

	/* Map a scatterlist into an SMMU */
	rc = dma_map_sg(cb->dev, sgt->sgl, sgt->nents,
			DMA_BIDIRECTIONAL);
	if (!rc) {
		dprintk(VIDC_ERR, "dma_map_sg failed! (%d != %d)\n",
				rc, sgt->nents);
		goto mem_map_sg_failed;
	}
	if (sgt->sgl) {
		dprintk(VIDC_DBG,
				"%s: DMA buf: %p, device: %p, attach: %p, table: %p, table sgl: %p, rc: %d, dma_address: %pa\n",
				__func__, dbuf, cb->dev, attach,
				sgt, sgt->sgl, rc,
				&sgt->sgl->dma_address);

	} else {
		dprintk(VIDC_ERR, "sgl is NULL\n");
		rc = -ENOMEM;
		goto mem_map_sg_failed;
	}
	mem->mem_type = client->mem_type;
	mem->device_addr = iova;
	mem->smem_priv = NULL;
	mem->size = buffer_size;
	mem->mapping_info.table = sgt;
	mem->mapping_info.buf = dbuf;
	mem->kvaddr = NULL;
	mem->buffer_type = buffer_type;

	return mem;
mem_map_sg_failed:
	dma_buf_detach(dbuf, attach);
mem_buf_attach_failed:
	return ERR_PTR(rc);
}
