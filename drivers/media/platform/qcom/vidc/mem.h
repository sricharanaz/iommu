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

#ifndef __VIDC_SMEM_H__
#define __VIDC_SMEM_H__

#include <linux/dma-attrs.h>

enum smem_cache_ops {
	SMEM_CACHE_CLEAN,
	SMEM_CACHE_INVALIDATE,
	SMEM_CACHE_CLEAN_INVALIDATE,
};

struct device;
struct sg_table;

struct smem {
	size_t size;
	void *kvaddr;
	dma_addr_t da;
	struct dma_attrs attrs;
	struct device *iommu_dev;
	struct sg_table *sgt;
};

struct smem *smem_alloc(struct device *dev, size_t size, u32 align,
			int map_kernel);
void smem_free(struct smem *mem);

int smem_dma_sync_cache(struct smem *mem, enum smem_cache_ops);

#endif /* __VIDC_SMEM_H__ */
