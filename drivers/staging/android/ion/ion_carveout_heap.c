/*
 * drivers/staging/android/ion/ion_carveout_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion.h"
#include "ion_priv.h"
#include <linux/msm_ion.h>
#include <asm/cacheflush.h>
#include <linux/iommu.h>

struct ion_carveout_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
};

ion_phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc(carveout_heap->pool, size);

	if (!offset)
		return ION_CARVEOUT_ALLOCATE_FAIL;

	return offset;
}

void ion_carveout_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(carveout_heap->pool, addr, size);
}

static int ion_carveout_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  ion_phys_addr_t *addr, size_t *len)
{
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	*addr = paddr;
	*len = buffer->size;
	return 0;
}

static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	struct sg_table *table;
	ion_phys_addr_t paddr;
	int ret;

	if (align > PAGE_SIZE)
		return -EINVAL;

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr = ion_carveout_allocate(heap, size, align);
	if (paddr == ION_CARVEOUT_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->priv_virt = table;

	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_carveout_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	ion_phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	ion_heap_buffer_zero(buffer);

	if (ion_buffer_cached(buffer))
		dma_sync_sg_for_device(NULL, table->sgl, table->nents,
							DMA_BIDIRECTIONAL);

	ion_carveout_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct sg_table *ion_carveout_heap_map_dma(struct ion_heap *heap,
						  struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

static void ion_carveout_heap_unmap_dma(struct ion_heap *heap,
					struct ion_buffer *buffer)
{
}

int ion_carveout_cache_ops(struct ion_heap *heap, struct ion_buffer *buffer,
                        void *vaddr, unsigned int offset, unsigned int length,
                        unsigned int cmd)
{
        struct ion_carveout_heap *carveout_heap =
             container_of(heap, struct  ion_carveout_heap, heap);
        unsigned int size_to_vmap, total_size;
        int i, j;
        void *ptr = NULL;
        ion_phys_addr_t buff_phys = buffer->priv_phys;

        if (!vaddr) {
                /*
                 * Split the vmalloc space into smaller regions in
                 * order to clean and/or invalidate the cache.
                 */
                size_to_vmap = ((VMALLOC_END - VMALLOC_START)/8);
                total_size = buffer->size;

                for (i = 0; i < total_size; i += size_to_vmap) {
                        size_to_vmap = min(size_to_vmap, total_size - i);
                        for (j = 0; j < 10 && size_to_vmap; ++j) {
                                ptr = ioremap(buff_phys, size_to_vmap);
                                if (ptr) {
                                        switch (cmd) {
                                        case ION_IOC_CLEAN_CACHES:
                                                dmac_map_area(ptr,
                                                        size_to_vmap, DMA_TO_DEVICE);
                                                break;
                                        case ION_IOC_INV_CACHES:
                                                dmac_unmap_area(ptr,
                                                        size_to_vmap, DMA_FROM_DEVICE);
                                                break;
                                        case ION_IOC_CLEAN_INV_CACHES:
                                                dmac_flush_range(ptr,
                                                        ptr + size_to_vmap);
                                                break;
                                        default:
                                                return -EINVAL;
                                        }
                                        buff_phys += size_to_vmap;
                                        break;
                                } else {
                                        size_to_vmap >>= 1;
                                }
                        }
                        if (!ptr) {
                                pr_err("Couldn't io-remap the memory\n");
                                return -EINVAL;
                        }
                        iounmap(ptr);
                }
        } else {
                switch (cmd) {
                case ION_IOC_CLEAN_CACHES:
                        dmac_map_area(ptr, size_to_vmap, DMA_TO_DEVICE);
                        break;
                case ION_IOC_INV_CACHES:
			dmac_unmap_area(ptr, size_to_vmap, DMA_FROM_DEVICE);
                        break;
                case ION_IOC_CLEAN_INV_CACHES:
			dmac_flush_range(ptr, ptr + size_to_vmap);
                        break;
                default:
                        return -EINVAL;
                }
        }

        return 0;
}

int ion_carveout_heap_map_iommu(struct ion_buffer *buffer,
                              struct ion_iommu_map *data,
                              struct dma_iommu_mapping *mapping,
                              unsigned long align,
                              unsigned long iova_length,
                              unsigned long flags)
{
	int ret = 0;
	struct scatterlist *sglist = 0;
	int prot = IOMMU_WRITE | IOMMU_READ;
	prot |= ION_IS_CACHED(flags) ? IOMMU_CACHE : 0;

	data->mapped_size = iova_length;

	if (ret)
		goto out;

	sglist = kmalloc(sizeof(*sglist), GFP_KERNEL);
	if (!sglist)
		goto out1;

	sg_init_table(sglist, 1);
	sglist->length = buffer->size;
	sglist->offset = 0;
	sglist->dma_address = buffer->priv_phys;

	data->iova_addr = alloc_iova(mapping, iova_length);

	ret = default_iommu_map_sg(mapping->domain, data->iova_addr, sglist,
			      1, prot);

	printk("\n done diommu");
	if (ret != buffer->size) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, data->iova_addr, mapping->domain);
		goto out1;
	}

	kfree(sglist);
	printk("\n done ciommu");
	return ret;

	iommu_unmap(mapping->domain, data->iova_addr, buffer->size);
out1:
	kfree(sglist);
out:
	return ret;
}

void ion_carveout_heap_unmap_iommu(struct ion_iommu_map *data)
{
	iommu_unmap(data->mapping->domain, data->iova_addr, data->mapped_size);
	return;
}

static struct ion_heap_ops carveout_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
	.phys = ion_carveout_heap_phys,
	.map_dma = ion_carveout_heap_map_dma,
	.unmap_dma = ion_carveout_heap_unmap_dma,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.cache_op = ion_carveout_cache_ops,
        .map_iommu = ion_carveout_heap_map_iommu,
        .unmap_iommu = ion_carveout_heap_unmap_iommu,
};

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_carveout_heap *carveout_heap;
	int ret;

	struct page *page;
	size_t size;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	ion_pages_sync_for_device(NULL, page, size, DMA_BIDIRECTIONAL);

	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);

	carveout_heap = kzalloc(sizeof(struct ion_carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		return ERR_PTR(-ENOMEM);

	carveout_heap->pool = gen_pool_create(12, -1);
	if (!carveout_heap->pool) {
		kfree(carveout_heap);
		return ERR_PTR(-ENOMEM);
	}
	carveout_heap->base = heap_data->base;
	gen_pool_add(carveout_heap->pool, carveout_heap->base, heap_data->size,
		     -1);
	carveout_heap->heap.ops = &carveout_heap_ops;
	carveout_heap->heap.type = ION_HEAP_TYPE_CARVEOUT;
	carveout_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	return &carveout_heap->heap;
}

void ion_carveout_heap_destroy(struct ion_heap *heap)
{
	struct ion_carveout_heap *carveout_heap =
	     container_of(heap, struct  ion_carveout_heap, heap);

	gen_pool_destroy(carveout_heap->pool);
	kfree(carveout_heap);
	carveout_heap = NULL;
}
