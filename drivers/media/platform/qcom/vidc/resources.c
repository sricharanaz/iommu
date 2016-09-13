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
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "core.h"
#include "resources.h"

static const struct freq_tbl freq_table_8916[] = {
//	{ 352800, 228570000 },	/* 1920x1088 @ 30 + 1280x720 @ 30 */
//	{ 244800, 160000000 },	/* 1920x1088 @ 30 */
//	{ 108000, 100000000 },	/* 1280x720 @ 30 */

	{ 1944000, 490000000 },	/* 4k UHD @ 60 */
	{  972000, 320000000 },	/* 4k UHD @ 30 */
	{  489600, 150000000 },	/* 1080p @ 60 */
	{  244800,  75000000 },	/* 1080p @ 30 */
};

static const struct reg_val reg_preset_8916[] = {
	{ 0x80010, 0xffffffff },
	{ 0x80018, 0x00001556 },
	{ 0x8001C, 0x00001556 },
};

static struct clock_info clks_8916[] = {
	{ .name = "core_clk", },
	{ .name = "core0_clk" },
	{ .name = "core1_clk" },
	{ .name = "rpm_mmaxi_clk", },
	{ .name = "rpm_mmaxi_a_clk", },
	{ .name = "mmagic_ahb_clk", },
	{ .name = "smmu_ahb_clk", },
	{ .name = "smmu_axi_clk", },
	{ .name = "mmagic_video_axi_clk" },
	{ .name = "iface_clk", },
	{ .name = "bus_clk", },
	{ .name = "mmagic_video_noc_cfg_ahb_clk", },
	{ .name = "maxi_clk", },
	{ .name = "mmss_mmagic_maxi_clk", },
};

static const u32 max_load_8916 = 2563200; /* 720p@30 + 1080p@30 */

static int get_clks(struct device *dev, struct vidc_resources *res)
{
	struct clock_info *clks = res->clks;
	unsigned int i;

	for (i = 0; i < res->clks_num; i++) {
		clks[i].clk = devm_clk_get(dev, clks[i].name);
		if (IS_ERR(clks[i].clk)) {
			dev_err(dev, "get_clks failed for %s", clks[i].name);
			return PTR_ERR(clks[i].clk);
		}
	}

	return 0;
}

int enable_clocks(struct vidc_core *core)
{
	struct vidc_resources *res = &(core->res);
        struct clock_info *clks = res->clks;
	unsigned int i;
	int ret;

	for (i = 0; i < res->clks_num; i++) {
		ret = clk_prepare_enable(clks[i].clk);
		if (ret) {
			dev_err(core->dev, "enable_clocks failed for %s", clks[i].name);
			goto err;
		}
	}

	return 0;
err:
	while (--i)
		clk_disable_unprepare(clks[i].clk);

	return ret;
}

void disable_clocks(struct vidc_core *core)
{
        struct vidc_resources *res = &(core->res);
	struct clock_info *clks = res->clks;
	unsigned int i;

	return;

	for (i = 0; i < res->clks_num; i++) {
		if (i == 8)
			continue;
		clk_disable_unprepare(clks[i].clk);
	}
}

int get_platform_resources(struct vidc_core *core)
{
	struct vidc_resources *res = &core->res;
	struct device *dev = core->dev;
	struct device_node *np = dev->of_node;
	const char *hfi_name = NULL, *propname;
	int ret;

	res->freq_tbl = freq_table_8916;
	res->freq_tbl_size = ARRAY_SIZE(freq_table_8916);
	res->reg_tbl = reg_preset_8916;
	res->reg_tbl_size = ARRAY_SIZE(reg_preset_8916);
	res->clks = clks_8916;
	res->clks_num = ARRAY_SIZE(clks_8916);
	res->max_load = max_load_8916;

	ret = of_property_read_string(np, "qcom,hfi", &hfi_name);
	if (ret) {
		dev_err(dev, "reading hfi type failed\n");
		return ret;
	}

	if (!strcasecmp(hfi_name, "venus"))
		core->hfi_type = VIDC_VENUS;
	else if (!strcasecmp(hfi_name, "q6"))
		core->hfi_type = VIDC_Q6;
	else
		return -EINVAL;

	propname = "qcom,hfi-version";
	ret = of_property_read_string(np, propname, &res->hfi_version);
	if (ret)
		dev_dbg(dev, "legacy HFI packetization\n");

	ret = get_clks(dev, res);
	if (ret) {
		dev_err(dev, "load clock table failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

void put_platform_resources(struct vidc_core *core)
{
}
