/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#ifndef __VIDC_VDEC_H__
#define __VIDC_VDEC_H__

struct vidc_core;
struct video_device;
struct vidc_inst;

int vdec_init(struct vidc_core *core, struct video_device *dec);
void vdec_deinit(struct vidc_core *core, struct video_device *dec);
int vdec_open(struct vidc_inst *inst);
void vdec_close(struct vidc_inst *inst);

#endif
