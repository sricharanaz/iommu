/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#include <linux/export.h>
#include <linux/err.h>
#include <linux/msm_ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/rwsem.h>
#include <linux/uaccess.h>
#include "../ion_priv.h"
#include "ion_cp_common.h"
#include <linux/dma-mapping.h>
#include <asm/dma-iommu.h>
#include <linux/of_device.h>
#include <linux/mod_devicetable.h>
#include <asm/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_reserved_mem.h>

#define ION_COMPAT_STR	"qcom,msm-ion"

static struct ion_device *idev;
static int num_heaps;
static struct ion_heap **heaps;

struct ion_heap_desc {
	unsigned int id;
	enum ion_heap_type type;
	const char *name;
	unsigned int permission_type;
};

struct ion_client *msm_ion_client_create(unsigned int heap_mask,
                                       const char *name)
{
       return ion_client_create(idev, name);
}
EXPORT_SYMBOL(msm_ion_client_create);

int msm_ion_secure_heap(int heap_id)
{
       return 0;
}
EXPORT_SYMBOL(msm_ion_secure_heap);

int msm_ion_unsecure_heap(int heap_id)
{
       return 0;
}
EXPORT_SYMBOL(msm_ion_unsecure_heap);

int msm_ion_secure_heap_2_0(int heap_id, enum cp_mem_usage usage)
{
       return 0;
}
EXPORT_SYMBOL(msm_ion_secure_heap_2_0);

int msm_ion_unsecure_heap_2_0(int heap_id, enum cp_mem_usage usage)
{
       return 0;
}
EXPORT_SYMBOL(msm_ion_unsecure_heap_2_0);

int msm_ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *vaddr, unsigned long len, unsigned int cmd)
{
	return ion_do_cache_op(client, handle, vaddr, 0, len, cmd);
}
EXPORT_SYMBOL(msm_ion_do_cache_op);

static unsigned long msm_ion_get_base(unsigned long size, unsigned int align)
{
        dma_addr_t addr;
	void *cpu_addr;

        DEFINE_DMA_ATTRS(attrs);

        dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &attrs);
        cpu_addr = dma_alloc_attrs(NULL, size,
                        &(addr), 0, &attrs);
	if (!cpu_addr)
		pr_err("Invalid address from dma_alloc");

        return addr;

}

static void msm_ion_allocate(struct ion_platform_heap *heap)
{

	if (!heap->base) {
		unsigned int align = 0;
		switch ((int) heap->type) {
			case ION_HEAP_TYPE_CHUNK:
				heap->base = msm_ion_get_base(heap->size,
							      align);
				if (!heap->base)
					pr_err("%s: could not get memory for heap %s "
					   "(id %x)\n", __func__, heap->name, heap->id);
			default:
				break;
		}
	}
}

static void free_pdata(const struct ion_platform_data *pdata)
{
	unsigned int i;
	for (i = 0; i < pdata->nr; ++i)
		kfree(pdata->heaps[i].extra_data);
	kfree(pdata);
}

static int check_vaddr_bounds(unsigned long start, unsigned long end)
{
	struct mm_struct *mm = current->active_mm;
	struct vm_area_struct *vma;
	int ret = 1;

	if (end < start)
		goto out;

	down_read(&mm->mmap_sem);
	vma = find_vma(mm, start);
	if (vma && vma->vm_start < end) {
		if (start < vma->vm_start)
			goto out_up;
		if (end > vma->vm_end)
			goto out_up;
		ret = 0;
	}

out_up:
	up_read(&mm->mmap_sem);
out:
	return ret;
}

static long msm_ion_custom_ioctl(struct ion_client *client,
				unsigned int cmd,
				unsigned long arg)
{
	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
	case ION_IOC_INV_CACHES:
	case ION_IOC_CLEAN_INV_CACHES:
	{
		struct ion_flush_data data;
		unsigned long start, end;
		struct ion_handle *handle = NULL;
		int ret;

		if (copy_from_user(&data, (void __user *)arg,
					sizeof(struct ion_flush_data)))
			return -EFAULT;

		start = (unsigned long) data.vaddr;
		end = (unsigned long) data.vaddr + data.length;

		if (start && check_vaddr_bounds(start, end)) {
			pr_err("%s: virtual address %p is out of bounds\n",
				__func__, data.vaddr);
			return -EINVAL;
		}

		if (!data.handle) {
			handle = ion_import_dma_buf(client, data.fd);
			if (IS_ERR(handle)) {
				pr_info("%s: Could not import handle: %d\n",
					__func__, (int)handle);
				return -EINVAL;
			}
		}

		ret = ion_do_cache_op(client,
				data.handle ? data.handle : handle,
				data.vaddr, data.offset, data.length,
				cmd);

		if (!data.handle)
			ion_free(client, handle);

		if (ret < 0)
			return ret;
		break;

	}
	case ION_IOC_GET_FLAGS:
	{
		struct ion_flag_data data;
		int ret;
		if (copy_from_user(&data, (void __user *)arg,
					sizeof(struct ion_flag_data)))
			return -EFAULT;

		ret = ion_handle_get_flags(client, data.handle, &data.flags);
		if (ret < 0)
			return ret;
		if (copy_to_user((void __user *)arg, &data,
					sizeof(struct ion_flag_data)))
			return -EFAULT;
		break;
	}
	default:
		return -ENOTTY;
	}
	return 0;
}

static int extend_iommu_mapping(struct dma_iommu_mapping *mapping)
{
        int next_bitmap;

        if (mapping->nr_bitmaps > mapping->extensions)
                return -EINVAL;

        next_bitmap = mapping->nr_bitmaps;
        mapping->bitmaps[next_bitmap] = kzalloc(mapping->bitmap_size,
                                                GFP_ATOMIC);
        if (!mapping->bitmaps[next_bitmap])
                return -ENOMEM;

        mapping->nr_bitmaps++;

        return 0;
}

#define CONFIG_ARM_DMA_IOMMU_ALIGNMENT 8

dma_addr_t alloc_iova(struct dma_iommu_mapping *mapping,
                                      size_t size)
{
        unsigned int order = get_order(size);
        unsigned int align = 0;
        unsigned int count, start;
        size_t mapping_size = mapping->bits << PAGE_SHIFT;
        unsigned long flags;
        dma_addr_t iova;
        int i;

        if (order > CONFIG_ARM_DMA_IOMMU_ALIGNMENT)
                order = CONFIG_ARM_DMA_IOMMU_ALIGNMENT;

        count = PAGE_ALIGN(size) >> PAGE_SHIFT;
        align = (1 << order) - 1;

        spin_lock_irqsave(&mapping->lock, flags);
        for (i = 0; i < mapping->nr_bitmaps; i++) {
                start = bitmap_find_next_zero_area(mapping->bitmaps[i],
                                mapping->bits, 0, count, align);

                if (start > mapping->bits)
                        continue;

                bitmap_set(mapping->bitmaps[i], start, count);
                break;
        }

        /*
         * No unused range found. Try to extend the existing mapping
         * and perform a second attempt to reserve an IO virtual
         * address range of size bytes.
         */
        if (i == mapping->nr_bitmaps) {
                if (extend_iommu_mapping(mapping)) {
                        spin_unlock_irqrestore(&mapping->lock, flags);
                        return DMA_ERROR_CODE;
                }

                start = bitmap_find_next_zero_area(mapping->bitmaps[i],
                                mapping->bits, 0, count, align);

                if (start > mapping->bits) {
                        spin_unlock_irqrestore(&mapping->lock, flags);
                        return DMA_ERROR_CODE;
                }
                bitmap_set(mapping->bitmaps[i], start, count);
        }
        spin_unlock_irqrestore(&mapping->lock, flags);

        iova = mapping->base + (mapping_size * i);
        iova += start << PAGE_SHIFT;

        return iova;
}

static int msm_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(heaps[i]);

	ion_device_destroy(idev);
	kfree(heaps);
	return 0;
}

struct ion_platform_heap apq8064_heaps[] = {
                {
                        .id     = ION_SYSTEM_HEAP_ID,
                        .type   = ION_HEAP_TYPE_SYSTEM,
                        .name   = ION_VMALLOC_HEAP_NAME,
                },
                {
                        .id     = ION_CP_MM_HEAP_ID,
                        .type   = ION_HEAP_TYPE_CP,
                        .name   = ION_MM_HEAP_NAME,
                        .size   = SZ_4M,
                },
                {
                        .id     = ION_MM_FIRMWARE_HEAP_ID,
                        .type   = ION_HEAP_TYPE_CARVEOUT,
                        .name   = ION_MM_FIRMWARE_HEAP_NAME,
                        .size   = 1966080,
                },
                {
                        .id     = ION_CP_MFC_HEAP_ID,
                        .type   = ION_HEAP_TYPE_CP,
                        .name   = ION_MFC_HEAP_NAME,
                        .size   = 270336,
                },
};

static struct ion_platform_data apq8064_ion_pdata = {
        .nr = 4,
        .heaps = apq8064_heaps,
};

static const struct of_device_id ion_data_match[] = {
	{ .compatible = "ion-msm", .data = &apq8064_ion_pdata },
	{},
};

static int msm_ion_probe(struct platform_device *pdev)
{
        struct ion_platform_data *pdata;
        unsigned int pdata_needs_to_be_freed;
        int err = -1;
        int i;
        const struct of_device_id *pd;

        pd = of_match_device(ion_data_match, &pdev->dev);
        pdata = pd->data;
        pdata_needs_to_be_freed = 0;

        num_heaps = pdata->nr;

        heaps = kcalloc(pdata->nr, sizeof(struct ion_heap *), GFP_KERNEL);

        if (!heaps) {
                err = -ENOMEM;
                goto out;
        }

        idev = ion_device_create(msm_ion_custom_ioctl);
        if (IS_ERR_OR_NULL(idev)) {
                err = PTR_ERR(idev);
                goto freeheaps;
        }

        /* create the heaps as specified in the board file */
        for (i = 0; i < num_heaps; i++) {
                struct ion_platform_heap *heap_data = &pdata->heaps[i];
                msm_ion_allocate(heap_data);

		if (heap_data->type == ION_HEAP_TYPE_CARVEOUT) {
			of_reserved_mem_device_init(&pdev->dev);
			dma_alloc_coherent(&pdev->dev, heap_data->size, &heap_data->base, GFP_KERNEL);
			pr_err("heap->data_base %x\n", heap_data->base);
		}

		pr_err("msm_ion %x\n", &pdev->dev);

                heaps[i] = ion_heap_create(heap_data);
                if (IS_ERR_OR_NULL(heaps[i])) {
                        heaps[i] = 0;
                        continue;
                } else {
                        if (heap_data->size)
                                pr_info("ION heap %s created at %lx "
                                        "with size %x\n", heap_data->name,
                                                          heap_data->base,
                                                          heap_data->size);
                        else
                                pr_info("ION heap %s created\n",
                                                          heap_data->name);
                }

                ion_device_add_heap(idev, heaps[i]);
        }

        if (pdata_needs_to_be_freed)
                free_pdata(pdata);

        platform_set_drvdata(pdev, idev);
        return 0;

freeheaps:
        kfree(heaps);
        if (pdata_needs_to_be_freed)
                free_pdata(pdata);
out:
        return err;
}

static struct platform_driver msm_ion_driver = {
        .probe = msm_ion_probe,
        .remove = msm_ion_remove,
        .driver = {
                .name = "ion-msm-driver",
                .of_match_table = ion_data_match,
        },
};

static int __init msm_ion_init(void)
{
	return platform_driver_register(&msm_ion_driver);
}

static void __exit msm_ion_exit(void)
{
	platform_driver_unregister(&msm_ion_driver);
}

subsys_initcall(msm_ion_init);
module_exit(msm_ion_exit);
