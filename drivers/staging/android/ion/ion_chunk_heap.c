/*
 * drivers/staging/android/ion/ion_chunk_heap.c
 *
 * Copyright (C) 2012 Google, Inc.
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

struct ion_chunk_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
	unsigned long chunk_size;
	unsigned long size;
	unsigned long allocated;
};

static int ion_chunk_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	struct ion_chunk_heap *chunk_heap =
		container_of(heap, struct ion_chunk_heap, heap);
	struct sg_table *table;
	struct scatterlist *sg;
	int ret, i;
	unsigned long num_chunks;
	unsigned long allocated_size;

	if (align > chunk_heap->chunk_size)
		return -EINVAL;

	allocated_size = ALIGN(size, chunk_heap->chunk_size);
	num_chunks = allocated_size / chunk_heap->chunk_size;

	if (allocated_size > chunk_heap->size - chunk_heap->allocated)
		return -ENOMEM;

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, num_chunks, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ret;
	}

	sg = table->sgl;
	for (i = 0; i < num_chunks; i++) {
		unsigned long paddr = gen_pool_alloc(chunk_heap->pool,
						     chunk_heap->chunk_size);
		if (!paddr)
			goto err;
		sg_set_page(sg, pfn_to_page(PFN_DOWN(paddr)),
				chunk_heap->chunk_size, 0);
		sg = sg_next(sg);
	}

	buffer->priv_virt = table;
	chunk_heap->allocated += allocated_size;
	return 0;
err:
	sg = table->sgl;
	for (i -= 1; i >= 0; i--) {
		gen_pool_free(chunk_heap->pool, page_to_phys(sg_page(sg)),
			      sg->length);
		sg = sg_next(sg);
	}
	sg_free_table(table);
	kfree(table);
	return -ENOMEM;
}

static void ion_chunk_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_chunk_heap *chunk_heap =
		container_of(heap, struct ion_chunk_heap, heap);
	struct sg_table *table = buffer->priv_virt;
	struct scatterlist *sg;
	int i;
	unsigned long allocated_size;

	allocated_size = ALIGN(buffer->size, chunk_heap->chunk_size);

	ion_heap_buffer_zero(buffer);

	if (ion_buffer_cached(buffer))
		dma_sync_sg_for_device(NULL, table->sgl, table->nents,
							DMA_BIDIRECTIONAL);

	for_each_sg(table->sgl, sg, table->nents, i) {
		gen_pool_free(chunk_heap->pool, page_to_phys(sg_page(sg)),
			      sg->length);
	}
	chunk_heap->allocated -= allocated_size;
	sg_free_table(table);
	kfree(table);
}

static struct sg_table *ion_chunk_heap_map_dma(struct ion_heap *heap,
					       struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

static void ion_chunk_heap_unmap_dma(struct ion_heap *heap,
				     struct ion_buffer *buffer)
{
}

int ion_cp_cache_ops(struct ion_heap *heap, struct ion_buffer *buffer,
			void *vaddr, unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	void (*outer_cache_op)(phys_addr_t, phys_addr_t) = NULL;
	struct ion_chunk_heap *cp_heap =
		container_of(heap, struct  ion_chunk_heap, heap);
	unsigned int size_to_vmap, total_size;
	int i, j;
	void *ptr = NULL;
	ion_phys_addr_t buff_phys = buffer->priv_phys;

	if (!vaddr) {
		/*
		 * Split the vmalloc space into smaller regions in
		 * order to clean and/or invalidate the cache.
		 */
		size_to_vmap = (VMALLOC_END - VMALLOC_START)/8;
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

static int ion_cp_heap_map_iommu(struct ion_buffer *buffer,
				struct ion_iommu_map *data,
				unsigned int domain_num,
				unsigned int partition_num,
				unsigned long align,
				unsigned long iova_length,
				unsigned long flags)
{
	struct iommu_domain *domain;
	int ret = 0;
	unsigned long extra;
	struct ion_chunk_heap *cp_heap =
		container_of(buffer->heap, struct ion_chunk_heap, heap);
	int prot = IOMMU_WRITE | IOMMU_READ;
	prot |= ION_IS_CACHED(flags) ? IOMMU_CACHE : 0;

	data->mapped_size = iova_length;

	extra = iova_length - buffer->size;

	ret = default_iommu_map_sg(domain, data->iova_addr, buffer->sg_table->sgl,
			      buffer->size, prot);
	if (ret) {
		pr_err("%s: could not map %lx in domain %p\n",
			__func__, data->iova_addr, domain);
		goto out1;
	}

	return ret;

out2:
	iommu_unmap(domain, data->iova_addr, buffer->size);
out1:
out:
	return ret;
}

static void ion_cp_heap_unmap_iommu(struct ion_iommu_map *data)
{
	unsigned int domain_num;
	unsigned int partition_num;
	struct iommu_domain *domain;
	struct ion_chunk_heap *cp_heap =
		container_of(data->buffer->heap, struct ion_chunk_heap, heap);

	iommu_unmap(domain, data->iova_addr, data->mapped_size);

	return;
}

static struct ion_heap_ops chunk_heap_ops = {
	.allocate = ion_chunk_heap_allocate,
	.free = ion_chunk_heap_free,
	.map_dma = ion_chunk_heap_map_dma,
	.unmap_dma = ion_chunk_heap_unmap_dma,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.cache_op = ion_cp_cache_ops,
	.map_iommu = ion_cp_heap_map_iommu,
	.unmap_iommu = ion_cp_heap_unmap_iommu,
};

struct ion_heap *ion_chunk_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_chunk_heap *chunk_heap;
	int ret;
	struct page *page;
	size_t size;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	ion_pages_sync_for_device(NULL, page, size, DMA_BIDIRECTIONAL);

	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);

	chunk_heap = kzalloc(sizeof(struct ion_chunk_heap), GFP_KERNEL);
	if (!chunk_heap)
		return ERR_PTR(-ENOMEM);

	chunk_heap->chunk_size = (unsigned long)heap_data->priv;
	chunk_heap->pool = gen_pool_create(get_order(chunk_heap->chunk_size) +
					   PAGE_SHIFT, -1);
	if (!chunk_heap->pool) {
		ret = -ENOMEM;
		goto error_gen_pool_create;
	}
	chunk_heap->base = heap_data->base;
	chunk_heap->size = heap_data->size;
	chunk_heap->allocated = 0;

	gen_pool_add(chunk_heap->pool, chunk_heap->base, heap_data->size, -1);
	chunk_heap->heap.ops = &chunk_heap_ops;
	chunk_heap->heap.type = ION_HEAP_TYPE_CHUNK;
	chunk_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	pr_debug("%s: base %lu size %zu align %ld\n", __func__,
		chunk_heap->base, heap_data->size, heap_data->align);

	return &chunk_heap->heap;

error_gen_pool_create:
	kfree(chunk_heap);
	return ERR_PTR(ret);
}

void ion_chunk_heap_destroy(struct ion_heap *heap)
{
	struct ion_chunk_heap *chunk_heap =
	     container_of(heap, struct  ion_chunk_heap, heap);

	gen_pool_destroy(chunk_heap->pool);
	kfree(chunk_heap);
	chunk_heap = NULL;
}
