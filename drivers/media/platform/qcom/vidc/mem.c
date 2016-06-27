/*
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 Linaro Ltd.
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

#include <linux/device.h>
#include <linux/dma-attrs.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "mem.h"

static int alloc_dma_mem(struct device *dev, size_t size, u32 align,
			 struct smem *mem, int map_kernel)
{
	int ret;

	if (align > 1) {
		align = ALIGN(align, SZ_4K);
		size = ALIGN(size, align);
	}

	size = ALIGN(size, SZ_4K);

	mem->size = size;
	mem->kvaddr = NULL;

	mem->iommu_dev = dev;

	printk("\n alloc_dma_mem 1");
	if (IS_ERR(mem->iommu_dev))
		return PTR_ERR(mem->iommu_dev);

	printk("\n alloc_dma_mem 2");
	init_dma_attrs(&mem->attrs);

	if (!map_kernel)
		dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &mem->attrs);

	mem->kvaddr = dma_alloc_attrs(mem->iommu_dev, size, &mem->da,
				      GFP_KERNEL, &mem->attrs);
	if (!mem->kvaddr)
		return -ENOMEM;

	printk("\n alloc_dma_mem 3");
	mem->sgt = kmalloc(sizeof(*mem->sgt), GFP_KERNEL);
	if (!mem->sgt) {
		ret = -ENOMEM;
		goto error_sgt;
	}

	printk("\n alloc_dma_mem 4");
	ret = dma_get_sgtable_attrs(mem->iommu_dev, mem->sgt, mem->kvaddr,
				    mem->da, mem->size, &mem->attrs);
	if (ret)
		goto error;

	printk("\n alloc_dma_mem 5");
	return 0;
error:
	kfree(mem->sgt);
error_sgt:
	dma_free_attrs(mem->iommu_dev, mem->size, mem->kvaddr,
		       mem->da, &mem->attrs);
	return ret;
}

static void free_dma_mem(struct smem *mem)
{
	dma_free_attrs(mem->iommu_dev, mem->size, mem->kvaddr,
		       mem->da, &mem->attrs);
	sg_free_table(mem->sgt);
	kfree(mem->sgt);
}

int smem_dma_sync_cache(struct smem *mem, enum smem_cache_ops cache_op)
{
	if (!mem)
		return -EINVAL;

	if (cache_op == SMEM_CACHE_CLEAN) {
		dma_sync_sg_for_device(mem->iommu_dev, mem->sgt->sgl,
				       mem->sgt->nents, DMA_TO_DEVICE);
	} else if (cache_op == SMEM_CACHE_INVALIDATE) {
		dma_sync_sg_for_cpu(mem->iommu_dev, mem->sgt->sgl,
				    mem->sgt->nents, DMA_FROM_DEVICE);
	} else {
		dma_sync_sg_for_device(mem->iommu_dev, mem->sgt->sgl,
				       mem->sgt->nents, DMA_TO_DEVICE);
		dma_sync_sg_for_cpu(mem->iommu_dev, mem->sgt->sgl,
				    mem->sgt->nents, DMA_FROM_DEVICE);
	}

	return 0;
}

struct smem *smem_alloc(struct device *dev, size_t size, u32 align,
			int map_kernel)
{
	struct smem *mem;
	int ret;

	printk("\n v1.1.1.1");
	if (!size)
		return ERR_PTR(-EINVAL);

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return ERR_PTR(-ENOMEM);

	printk("\n v1.1.1.2");
	ret = alloc_dma_mem(dev, size, align, mem, map_kernel);
	if (ret) {
		kfree(mem);
		return ERR_PTR(ret);
	}

	printk("\n v1.1.1.3");
	return mem;
}

void smem_free(struct smem *mem)
{
	if (!mem)
		return;

	free_dma_mem(mem);
	kfree(mem);
};
