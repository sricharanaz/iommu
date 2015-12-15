/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/regmap.h>
#include "tsens.h"

#define STATUS_OFFSET	0x10a0
#define LAST_TEMP_MASK	0xfff

static int get_temp_8996(struct tsens_device *tmdev, int id, int *temp)
{
	struct tsens_sensor *s = &tmdev->sensor[id];
	u32 code;
	unsigned int sensor_addr;
	int last_temp = 0, ret;

	sensor_addr = STATUS_OFFSET + s->hw_id * 4;
	ret = regmap_read(tmdev->map, sensor_addr, &code);
	if (ret)
		return ret;
	last_temp = code & LAST_TEMP_MASK;

	*temp = last_temp;

	return 0;
}

const struct tsens_ops ops_8996 = {
	.init		= init_common,
	.get_temp	= get_temp_8996,
};
