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

#include <linux/clk.h>

#include "core.h"
#include "load.h"

static u32 get_inst_load(struct vidc_inst *inst)
{
	int mbs;
	u32 w = inst->width;
	u32 h = inst->height;

	if (!inst->hfi_inst || !(inst->hfi_inst->state >= INST_INIT &&
				 inst->hfi_inst->state < INST_STOP))
		return 0;

	mbs = (ALIGN(w, 16) / 16) * (ALIGN(h, 16) / 16);

	return mbs * inst->fps;
}

static u32 get_load(struct vidc_core *core, enum session_type type)
{
	struct vidc_inst *inst = NULL;
	u32 mbs_per_sec = 0;

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (inst->session_type != type)
			continue;

		mbs_per_sec += get_inst_load(inst);
	}
	mutex_unlock(&core->lock);

	return mbs_per_sec;
}

static int scale_clocks_load(struct vidc_core *core, u32 mbs_per_sec)
{
	const struct freq_tbl *table = core->res.freq_tbl;
	int num_rows = core->res.freq_tbl_size;
	struct clk *clk = core->res.clks[0].clk;
	struct device *dev = core->dev;
	unsigned long freq = table[0].freq;
	int ret, i;

	if (!mbs_per_sec && num_rows > 1) {
		freq = table[num_rows - 1].freq;
		goto set_freq;
	}

	for (i = 0; i < num_rows; i++) {
		if (mbs_per_sec > table[i].load)
			break;
		freq = table[i].freq;
	}

set_freq:

	ret = clk_set_rate(clk, freq);
	if (ret) {
		dev_err(dev, "failed to set clock rate %lu (%d)\n", freq, ret);
		return ret;
	}

	return 0;
}

int vidc_scale_clocks(struct vidc_core *core)
{
	struct device *dev = core->dev;
	u32 mbs_per_sec;
	int ret;

	mbs_per_sec = get_load(core, VIDC_ENCODER) +
		      get_load(core, VIDC_DECODER);

	if (mbs_per_sec > core->res.max_load) {
		dev_warn(dev, "HW is overloaded, needed: %d max: %d\n",
			 mbs_per_sec, core->res.max_load);
		return -EBUSY;
	}

	ret = scale_clocks_load(core, mbs_per_sec);
	if (ret)
		dev_warn(dev, "failed to scale clocks, performance might be impacted\n");

	return 0;
}
