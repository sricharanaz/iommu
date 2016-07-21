/*
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
#ifndef __VIDC_RESOURCES_H__
#define __VIDC_RESOURCES_H__

struct vidc_core;
struct clk;

struct freq_tbl {
	unsigned int load;
	unsigned long freq;
};

struct reg_val {
	u32 reg;
	u32 value;
};

struct clock_info {
	const char *name;
	struct clk *clk;
};

struct vidc_resources {
	void __iomem *base;
	unsigned int irq;
	const struct freq_tbl *freq_tbl;
	unsigned int freq_tbl_size;
	const struct reg_val *reg_tbl;
	unsigned int reg_tbl_size;
	struct clock_info *clks;
	unsigned int clks_num;
	const char *hfi_version;
	u32 max_load;
};

int get_platform_resources(struct vidc_core *);
void put_platform_resources(struct vidc_core *);

int enable_clocks(struct vidc_core *);
void disable_clocks(struct vidc_core *);
extern void venus_enable_clock_config(struct vidc_core *core);

#endif
