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
#ifndef __VENUS_HFI_H__
#define __VENUS_HFI_H__

struct hfi_device;
struct vidc_resources;

void venus_hfi_destroy(struct hfi_device *hfi);
int venus_hfi_create(struct hfi_device *hfi, struct vidc_resources *res);

#endif
