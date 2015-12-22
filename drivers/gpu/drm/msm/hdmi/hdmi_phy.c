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
 */

#include <linux/of_device.h>

#include "hdmi.h"

static int hdmi_phy_regulator_init(struct hdmi_phy *phy)
{
	struct hdmi_phy_cfg *cfg = phy->cfg;
	struct device *dev = &phy->pdev->dev;
	int i, ret;

	phy->regs = devm_kzalloc(dev, sizeof(phy->regs[0]) * cfg->num_regs,
			GFP_KERNEL);
	if (!phy->regs)
		return -ENOMEM;

	for (i = 0; i < cfg->num_regs; i++) {
		struct regulator *reg;

		reg = devm_regulator_get(dev, cfg->reg_names[i]);
		if (IS_ERR(reg)) {
			ret = PTR_ERR(reg);
			dev_err(dev,"failed to get phy regulator: %s (%d)\n",
				cfg->reg_names[i], ret);
			return ret;
		}

		phy->regs[i] = reg;
	}

	return 0;
}

static int hdmi_phy_clk_init(struct hdmi_phy *phy)
{
	struct hdmi_phy_cfg *cfg = phy->cfg;
	struct device *dev = &phy->pdev->dev;
	int i, ret;

	phy->clks = devm_kzalloc(dev, sizeof(phy->clks[0]) * cfg->num_clks,
			GFP_KERNEL);
	if (!phy->clks)
		return -ENOMEM;

	for (i = 0; i < cfg->num_clks; i++) {
		struct clk *clk;

		clk = devm_clk_get(dev, cfg->clk_names[i]);
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			dev_err(dev, "failed to get phy clock: %s (%d)\n",
				cfg->clk_names[i], ret);
			return ret;
		}

		phy->clks[i] = clk;
	}

	return 0;
}

int hdmi_phy_resource_enable(struct hdmi_phy *phy)
{
	struct hdmi_phy_cfg *cfg = phy->cfg;
	struct device *dev = &phy->pdev->dev;
	int i, ret = 0;

	pm_runtime_get_sync(dev);

	for (i = 0; i < cfg->num_regs; i++) {
		ret = regulator_enable(phy->regs[i]);
		if (ret)
			dev_err(dev, "failed to enable phy regulator: %s (%d)\n",
				cfg->reg_names[i], ret);
	}

	for (i = 0; i < cfg->num_clks; i++) {
		ret = clk_prepare_enable(phy->clks[i]);
		if (ret)
			dev_err(dev, "failed to enable clock: %s (%d)\n",
				cfg->clk_names[i], ret);
	}

	return ret;
}

void hdmi_phy_resource_disable(struct hdmi_phy *phy)
{
	struct hdmi_phy_cfg *cfg = phy->cfg;
	struct device *dev = &phy->pdev->dev;
	int i;

	for (i = cfg->num_clks - 1; i >= 0; i--)
		clk_disable_unprepare(phy->clks[i]);

	for (i = cfg->num_regs - 1; i >= 0; i--)
		regulator_disable(phy->regs[i]);

	pm_runtime_put_sync(dev);
}

void hdmi_phy_powerup(struct hdmi_phy *phy, unsigned long int pixclock)
{
	if (!phy || !phy->cfg->powerup)
		return;

	phy->cfg->powerup(phy, pixclock);
}

void hdmi_phy_powerdown(struct hdmi_phy *phy)
{
	if (!phy || !phy->cfg->powerdown)
		return;

	phy->cfg->powerdown(phy);
}

static int hdmi_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hdmi_phy *phy;
	int ret;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENODEV;

	phy->cfg = (struct hdmi_phy_cfg *) of_device_get_match_data(dev);
	if (!phy->cfg)
		return -ENODEV;

	phy->mmio = msm_ioremap(pdev, "hdmi_phy", "HDMI_PHY");
	if (IS_ERR(phy->mmio)) {
		dev_err(dev, "%s: failed to map phy base\n", __func__);
		return -ENOMEM;
	}

	phy->pdev = pdev;

	ret = hdmi_phy_regulator_init(phy);
	if (ret)
		return ret;

	ret = hdmi_phy_clk_init(phy);
	if (ret)
		return ret;

	pm_runtime_enable(&pdev->dev);

	platform_set_drvdata(pdev, phy);

	return 0;
}

static int hdmi_phy_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id hdmi_phy_dt_match[] = {
	{ .compatible = "qcom,hdmi-phy-8x60",
	  .data = &hdmi_phy_8x60_cfg },
	{ .compatible = "qcom,hdmi-phy-8960",
	  .data = &hdmi_phy_8960_cfg },
	{ .compatible = "qcom,hdmi-phy-8x74",
	  .data = &hdmi_phy_8x74_cfg },
	{}
};

static struct platform_driver hdmi_phy_platform_driver = {
	.probe      = hdmi_phy_probe,
	.remove     = hdmi_phy_remove,
	.driver     = {
		.name   = "msm_hdmi_phy",
		.of_match_table = hdmi_phy_dt_match,
	},
};

void __init hdmi_phy_driver_register(void)
{
	platform_driver_register(&hdmi_phy_platform_driver);
}

void __exit hdmi_phy_driver_unregister(void)
{
	platform_driver_unregister(&hdmi_phy_platform_driver);
}
