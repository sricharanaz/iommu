/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/phy/phy.h>

#define QSERDES_COM_BG_TIMER			0x00c
#define QSERDES_COM_SSC_EN_CENTER		0x010
#define QSERDES_COM_SSC_ADJ_PER1		0x014
#define QSERDES_COM_SSC_ADJ_PER2		0x018
#define QSERDES_COM_SSC_PER1			0x01c
#define QSERDES_COM_SSC_PER2			0x020
#define QSERDES_COM_SSC_STEP_SIZE1		0x024
#define QSERDES_COM_SSC_STEP_SIZE2		0x028
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN		0x034
#define QSERDES_COM_CLK_ENABLE1			0x038
#define QSERDES_COM_SYS_CLK_CTRL		0x03c
#define QSERDES_COM_SYSCLK_BUF_ENABLE		0x040
#define QSERDES_COM_PLL_IVCO			0x048
#define QSERDES_COM_LOCK_CMP1_MODE0		0x04c
#define QSERDES_COM_LOCK_CMP2_MODE0		0x050
#define QSERDES_COM_LOCK_CMP3_MODE0		0x054
#define QSERDES_COM_BG_TRIM			0x070
#define QSERDES_COM_CLK_EP_DIV			0x074
#define QSERDES_COM_CP_CTRL_MODE0		0x078
#define QSERDES_COM_PLL_RCTRL_MODE0		0x084
#define QSERDES_COM_PLL_CCTRL_MODE0		0x090
#define QSERDES_COM_SYSCLK_EN_SEL		0x0ac
#define QSERDES_COM_RESETSM_CNTRL		0x0b4
#define QSERDES_COM_RESTRIM_CTRL		0x0bc
#define QSERDES_COM_RESCODE_DIV_NUM		0x0c4
#define QSERDES_COM_LOCK_CMP_EN			0x0c8
#define QSERDES_COM_DEC_START_MODE0		0x0d0
#define QSERDES_COM_DIV_FRAC_START1_MODE0	0x0dc
#define QSERDES_COM_DIV_FRAC_START2_MODE0	0x0e0
#define QSERDES_COM_DIV_FRAC_START3_MODE0	0x0e4
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE0	0x108
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE0	0x10c
#define QSERDES_COM_VCO_TUNE_CTRL		0x124
#define QSERDES_COM_VCO_TUNE_MAP		0x128
#define QSERDES_COM_VCO_TUNE1_MODE0		0x12c
#define QSERDES_COM_VCO_TUNE2_MODE0		0x130
#define QSERDES_COM_VCO_TUNE_TIMER1		0x144
#define QSERDES_COM_VCO_TUNE_TIMER2		0x148
#define QSERDES_COM_BG_CTRL			0x170
#define QSERDES_COM_CLK_SELECT			0x174
#define QSERDES_COM_HSCLK_SEL			0x178
#define QSERDES_COM_CORECLK_DIV			0x184
#define QSERDES_COM_CORE_CLK_EN			0x18c
#define QSERDES_COM_C_READY_STATUS		0x190
#define QSERDES_COM_CMN_CONFIG			0x194
#define QSERDES_COM_SVS_MODE_CLK_SEL		0x19c

#define PCIE_N_SW_RESET(n)			(PCS_PORT(n) + 0x00)
#define PCIE_N_POWER_DOWN_CONTROL(n)		(PCS_PORT(n) + 0x04)
#define PCIE_N_START_CONTROL(n)			(PCS_PORT(n) + 0x08)
#define PCIE_N_TXDEEMPH_M6DB_V0(n)		(PCS_PORT(n) + 0x24)
#define PCIE_N_TXDEEMPH_M3P5DB_V0(n)		(PCS_PORT(n) + 0x28)
#define PCIE_N_ENDPOINT_REFCLK_DRIVE(n)		(PCS_PORT(n) + 0x54)
#define PCIE_N_RX_IDLE_DTCT_CNTRL(n)		(PCS_PORT(n) + 0x58)
#define PCIE_N_POWER_STATE_CONFIG1(n)		(PCS_PORT(n) + 0x60)
#define PCIE_N_POWER_STATE_CONFIG4(n)		(PCS_PORT(n) + 0x6c)
#define PCIE_N_PWRUP_RESET_DLY_TIME_AUXCLK(n)	(PCS_PORT(n) + 0xa0)
#define PCIE_N_LP_WAKEUP_DLY_TIME_AUXCLK(n)	(PCS_PORT(n) + 0xa4)
#define PCIE_N_PLL_LOCK_CHK_DLY_TIME(n)		(PCS_PORT(n) + 0xa8)
#define PCIE_N_PCS_STATUS(n)			(PCS_PORT(n) + 0x174)
#define PCIE_N_LP_WAKEUP_DLY_TIME_AUXCLK_MSB(n)	(PCS_PORT(n) + 0x1a8)
#define PCIE_N_OSC_DTCT_ACTIONS(n)		(PCS_PORT(n) + 0x1ac)
#define PCIE_N_SIGDET_CNTRL(n)			(PCS_PORT(n) + 0x1b0)
#define PCIE_N_L1SS_WAKEUP_DLY_TIME_AUXCLK_LSB(n)	(PCS_PORT(n) + 0x1dc)
#define PCIE_N_L1SS_WAKEUP_DLY_TIME_AUXCLK_MSB(n)	(PCS_PORT(n) + 0x1e0)

#define PCIE_COM_SW_RESET		0x400
#define PCIE_COM_POWER_DOWN_CONTROL	0x404
#define PCIE_COM_START_CONTROL		0x408
#define PCIE_COM_PCS_READY_STATUS	0x448

#define PCIE_LANE_TX_BASE 0x1000
#define PCIE_LANE_RX_BASE 0x1200
#define PCIE_LANE_PCS_BASE 0x1400

#define TX(n) (PCIE_LANE_TX_BASE + n * 0x1000)
#define RX(n) (PCIE_LANE_RX_BASE + n * 0x1000)
#define PCS_PORT(n) (PCIE_LANE_PCS_BASE + n * 0x1000)

#define QSERDES_TX_N_RES_CODE_LANE_OFFSET(n)	(TX(n) + 0x4c)
#define QSERDES_TX_N_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN(n) (TX(n) + 0x68)
#define QSERDES_TX_N_LANE_MODE(n)		(TX(n) + 0x94)
#define QSERDES_TX_N_RCV_DETECT_LVL_2(n)	(TX(n) + 0xac)

#define QSERDES_RX_N_UCDR_SO_GAIN_HALF(n)	(RX(n) + 0x010)
#define QSERDES_RX_N_UCDR_SO_GAIN(n)		(RX(n) + 0x01c)
#define QSERDES_RX_N_UCDR_SO_SATURATION_AND_ENABLE(n) (RX(n) + 0x048)
#define QSERDES_RX_N_RX_EQU_ADAPTOR_CNTRL2(n)	(RX(n) + 0x0d8)
#define QSERDES_RX_N_RX_EQU_ADAPTOR_CNTRL3(n)	(RX(n) + 0x0dc)
#define QSERDES_RX_N_RX_EQU_ADAPTOR_CNTRL4(n)	(RX(n) + 0x0e0)
#define QSERDES_RX_N_SIGDET_ENABLES(n)		(RX(n) + 0x110)
#define QSERDES_RX_N_SIGDET_DEGLITCH_CNTRL(n)	(RX(n) + 0x11c)
#define QSERDES_RX_N_SIGDET_LVL(n)		(RX(n) + 0x118)
#define QSERDES_RX_N_RX_BAND(n)			(RX(n) + 0x120)

#define REFCLK_STABILIZATION_DELAY_US_MIN	1000
#define REFCLK_STABILIZATION_DELAY_US_MAX	1005
#define PHY_READY_TIMEOUT_COUNT			10
#define POWER_DOWN_DELAY_US_MIN			10
#define POWER_DOWN_DELAY_US_MAX			11

struct phy_msm8996_priv;

struct phy_msm8996_desc {
	struct phy	*phy;
	unsigned int	index;
	struct	reset_control *phy_rstc;
	struct phy_msm8996_priv *priv;
};

struct phy_msm8996_priv {
	void __iomem *base;
	struct clk *cfg_clk;
	struct clk *aux_clk;
	struct clk *ref_clk;
	struct clk *ref_clk_src;
	struct	reset_control *phy_rstc, *phycom_rstc;
	struct device *dev;
	unsigned int	nphys;
	int		init_count;
	struct mutex	phy_mutex;
	struct phy_msm8996_desc	**phys;
};

static struct phy *phy_msm8996_pcie_phy_xlate(struct device *dev,
					     struct of_phandle_args *args)
{
	struct phy_msm8996_priv *priv = dev_get_drvdata(dev);
	int i;

	if (WARN_ON(args->args[0] >= priv->nphys))
		return ERR_PTR(-ENODEV);

	for (i = 0; i < priv->nphys; i++) {
		if (priv->phys[i]->index == args->args[0])
			break;
	}

	if (i == priv->nphys)
		return ERR_PTR(-ENODEV);

	return priv->phys[i]->phy;
}

static int pcie_phy_is_ready(struct phy *phy)
{
	struct phy_msm8996_desc *phydesc = phy_get_drvdata(phy);
	struct phy_msm8996_priv *priv = phydesc->priv;
	void __iomem *base = priv->base;
	int retries = 0;

	do {
		if ((readl_relaxed(base + PCIE_COM_PCS_READY_STATUS) & 0x1))
			return 0;
		retries++;
		usleep_range(REFCLK_STABILIZATION_DELAY_US_MIN,
					 REFCLK_STABILIZATION_DELAY_US_MAX);
	} while (retries < PHY_READY_TIMEOUT_COUNT);

	dev_err(priv->dev, "PHY Failed to come up\n");

	return -EBUSY;
}

static int qcom_msm8996_phy_common_power_off(struct phy *phy)
{
	struct phy_msm8996_desc *phydesc = phy_get_drvdata(phy);
	struct phy_msm8996_priv *priv = phydesc->priv;
	void __iomem *base = priv->base;

	mutex_lock(&priv->phy_mutex);
	if (--priv->init_count) {
		mutex_unlock(&priv->phy_mutex);
		return 0;
	}

	writel_relaxed(0x01, base + PCIE_COM_SW_RESET);
	writel_relaxed(0x0, base + PCIE_COM_POWER_DOWN_CONTROL);

	reset_control_assert(priv->phy_rstc);
	reset_control_assert(priv->phycom_rstc);
	clk_disable_unprepare(priv->cfg_clk);
	clk_disable_unprepare(priv->aux_clk);
	clk_disable_unprepare(priv->ref_clk);
	clk_disable_unprepare(priv->ref_clk_src);

	mutex_unlock(&priv->phy_mutex);

	return 0;
}

static int qcom_msm8996_phy_common_power_on(struct phy *phy)
{
	struct phy_msm8996_desc *phydesc = phy_get_drvdata(phy);
	struct phy_msm8996_priv *priv = phydesc->priv;
	void __iomem *base = priv->base;
	int ret;

	mutex_lock(&priv->phy_mutex);
	if (priv->init_count++) {
		mutex_unlock(&priv->phy_mutex);
		return 0;
	}

	clk_prepare_enable(priv->cfg_clk);
	clk_prepare_enable(priv->aux_clk);
	clk_prepare_enable(priv->ref_clk);
	clk_prepare_enable(priv->ref_clk_src);

	reset_control_deassert(priv->phy_rstc);
	reset_control_deassert(priv->phycom_rstc);

	writel_relaxed(0x01, base + PCIE_COM_POWER_DOWN_CONTROL);
	writel_relaxed(0x1c, base + QSERDES_COM_BIAS_EN_CLKBUFLR_EN);
	writel_relaxed(0x10, base + QSERDES_COM_CLK_ENABLE1);
	writel_relaxed(0x33, base + QSERDES_COM_CLK_SELECT);
	writel_relaxed(0x06, base + QSERDES_COM_CMN_CONFIG);
	writel_relaxed(0x42, base + QSERDES_COM_LOCK_CMP_EN);
	writel_relaxed(0x00, base + QSERDES_COM_VCO_TUNE_MAP);
	writel_relaxed(0xff, base + QSERDES_COM_VCO_TUNE_TIMER1);
	writel_relaxed(0x1f, base + QSERDES_COM_VCO_TUNE_TIMER2);
	writel_relaxed(0x01, base + QSERDES_COM_HSCLK_SEL);
	writel_relaxed(0x01, base + QSERDES_COM_SVS_MODE_CLK_SEL);
	writel_relaxed(0x00, base + QSERDES_COM_CORE_CLK_EN);
	writel_relaxed(0x0a, base + QSERDES_COM_CORECLK_DIV);
	writel_relaxed(0x09, base + QSERDES_COM_BG_TIMER);
	writel_relaxed(0x82, base + QSERDES_COM_DEC_START_MODE0);
	writel_relaxed(0x03, base + QSERDES_COM_DIV_FRAC_START3_MODE0);
	writel_relaxed(0x55, base + QSERDES_COM_DIV_FRAC_START2_MODE0);
	writel_relaxed(0x55, base + QSERDES_COM_DIV_FRAC_START1_MODE0);
	writel_relaxed(0x00, base + QSERDES_COM_LOCK_CMP3_MODE0);
	writel_relaxed(0x1a, base + QSERDES_COM_LOCK_CMP2_MODE0);
	writel_relaxed(0x0a, base + QSERDES_COM_LOCK_CMP1_MODE0);
	writel_relaxed(0x33, base + QSERDES_COM_CLK_SELECT);
	writel_relaxed(0x02, base + QSERDES_COM_SYS_CLK_CTRL);
	writel_relaxed(0x1f, base + QSERDES_COM_SYSCLK_BUF_ENABLE);
	writel_relaxed(0x04, base + QSERDES_COM_SYSCLK_EN_SEL);
	writel_relaxed(0x0b, base + QSERDES_COM_CP_CTRL_MODE0);
	writel_relaxed(0x16, base + QSERDES_COM_PLL_RCTRL_MODE0);
	writel_relaxed(0x28, base + QSERDES_COM_PLL_CCTRL_MODE0);
	writel_relaxed(0x00, base + QSERDES_COM_INTEGLOOP_GAIN1_MODE0);
	writel_relaxed(0x80, base + QSERDES_COM_INTEGLOOP_GAIN0_MODE0);
	writel_relaxed(0x01, base + QSERDES_COM_SSC_EN_CENTER);
	writel_relaxed(0x31, base + QSERDES_COM_SSC_PER1);
	writel_relaxed(0x01, base + QSERDES_COM_SSC_PER2);
	writel_relaxed(0x02, base + QSERDES_COM_SSC_ADJ_PER1);
	writel_relaxed(0x00, base + QSERDES_COM_SSC_ADJ_PER2);
	writel_relaxed(0x2f, base + QSERDES_COM_SSC_STEP_SIZE1);
	writel_relaxed(0x19, base + QSERDES_COM_SSC_STEP_SIZE2);
	writel_relaxed(0x15, base + QSERDES_COM_RESCODE_DIV_NUM);
	writel_relaxed(0x0f, base + QSERDES_COM_BG_TRIM);
	writel_relaxed(0x0f, base + QSERDES_COM_PLL_IVCO);
	writel_relaxed(0x19, base + QSERDES_COM_CLK_EP_DIV);
	writel_relaxed(0x10, base + QSERDES_COM_CLK_ENABLE1);
	writel_relaxed(0x00, base + QSERDES_COM_HSCLK_SEL);
	writel_relaxed(0x40, base + QSERDES_COM_RESCODE_DIV_NUM);
	writel_relaxed(0x00, base + PCIE_COM_SW_RESET);
	writel_relaxed(0x03, base + PCIE_COM_START_CONTROL);

	ret = pcie_phy_is_ready(phy);

	mutex_unlock(&priv->phy_mutex);

	return ret;
}

static int qcom_msm8996_pcie_phy_power_on(struct phy *phy)
{
	struct phy_msm8996_desc *phydesc = phy_get_drvdata(phy);
	struct phy_msm8996_priv *priv = phydesc->priv;
	void __iomem *base = priv->base;
	int id = phydesc->index;
	int err;

	err = qcom_msm8996_phy_common_power_on(phy);
	if (err) {
		dev_err(priv->dev, "PCIE phy power on failed\n");
		return err;
	}

	reset_control_deassert(phydesc->phy_rstc);

	writel_relaxed(0x45, base +
		       QSERDES_TX_N_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN(id));
	writel_relaxed(0x06, base + QSERDES_TX_N_LANE_MODE(id));
	writel_relaxed(0x1c, base + QSERDES_RX_N_SIGDET_ENABLES(id));
	writel_relaxed(0x17, base + QSERDES_RX_N_SIGDET_LVL(id));
	writel_relaxed(0x01, base + QSERDES_RX_N_RX_EQU_ADAPTOR_CNTRL2(id));
	writel_relaxed(0x00, base + QSERDES_RX_N_RX_EQU_ADAPTOR_CNTRL3(id));
	writel_relaxed(0xdb, base + QSERDES_RX_N_RX_EQU_ADAPTOR_CNTRL4(id));
	writel_relaxed(0x18, base + QSERDES_RX_N_RX_BAND(id));
	writel_relaxed(0x04, base + QSERDES_RX_N_UCDR_SO_GAIN(id));
	writel_relaxed(0x04, base + QSERDES_RX_N_UCDR_SO_GAIN_HALF(id));
	writel_relaxed(0x4c, base + PCIE_N_RX_IDLE_DTCT_CNTRL(id));
	writel_relaxed(0x00, base + PCIE_N_PWRUP_RESET_DLY_TIME_AUXCLK(id));
	writel_relaxed(0x01, base + PCIE_N_LP_WAKEUP_DLY_TIME_AUXCLK(id));
	writel_relaxed(0x05, base + PCIE_N_PLL_LOCK_CHK_DLY_TIME(id));
	writel_relaxed(0x4b, base +
		       QSERDES_RX_N_UCDR_SO_SATURATION_AND_ENABLE(id));
	writel_relaxed(0x14, base + QSERDES_RX_N_SIGDET_DEGLITCH_CNTRL(id));
	writel_relaxed(0x05, base + PCIE_N_ENDPOINT_REFCLK_DRIVE(id));
	writel_relaxed(0x02, base + PCIE_N_POWER_DOWN_CONTROL(id));
	writel_relaxed(0x00, base + PCIE_N_POWER_STATE_CONFIG4(id));
	writel_relaxed(0xa3, base + PCIE_N_POWER_STATE_CONFIG1(id));
	writel_relaxed(0x19, base + QSERDES_RX_N_SIGDET_LVL(id));
	writel_relaxed(0x0e, base + PCIE_N_TXDEEMPH_M3P5DB_V0(id));
	writel_relaxed(0x03, base + PCIE_N_POWER_DOWN_CONTROL(id));

	usleep_range(POWER_DOWN_DELAY_US_MIN, POWER_DOWN_DELAY_US_MAX);

	writel_relaxed(0x00, base + PCIE_N_SW_RESET(id));
	writel_relaxed(0x0a, base + PCIE_N_START_CONTROL(id));

	return 0;
}

static int qcom_msm8996_pcie_phy_power_off(struct phy *phy)
{
	struct phy_msm8996_desc *phydesc = phy_get_drvdata(phy);
	struct phy_msm8996_priv *priv = phydesc->priv;
	void __iomem *base = priv->base;
	int id = phydesc->index;
	int err;

	writel_relaxed(0x01, base + PCIE_N_SW_RESET(id));
	writel_relaxed(0x0, base + PCIE_N_POWER_DOWN_CONTROL(id));

	err = qcom_msm8996_phy_common_power_off(phy);
	if (err < 0)
		return err;

	reset_control_assert(phydesc->phy_rstc);

	return err;
}

static const struct phy_ops qcom_msm8996_pcie_phy_ops = {
	.power_on	= qcom_msm8996_pcie_phy_power_on,
	.power_off	= qcom_msm8996_pcie_phy_power_off,
	.owner		= THIS_MODULE,
};


static int qcom_msm8996_pcie_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *child;
	struct phy *phy;
	struct phy_provider *phy_provider;
	struct phy_msm8996_priv *priv;
	struct resource *res;
	int ret;
	u32 phy_id;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	priv->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!priv->base)
		return -ENOMEM;

	priv->cfg_clk = devm_clk_get(dev, "cfg");
	if (IS_ERR(priv->cfg_clk))
		return PTR_ERR(priv->cfg_clk);

	priv->aux_clk = devm_clk_get(dev, "aux");
	if (IS_ERR(priv->aux_clk))
		return PTR_ERR(priv->aux_clk);

	priv->ref_clk = devm_clk_get(dev, "ref_clk");
	if (IS_ERR(priv->ref_clk))
		return PTR_ERR(priv->ref_clk);

	priv->ref_clk_src = devm_clk_get(dev, "ref_clk_src");
	if (IS_ERR(priv->ref_clk_src))
		return PTR_ERR(priv->ref_clk_src);

	priv->nphys = of_get_child_count(dev->of_node);
	if (priv->nphys == 0)
		return -ENODEV;

	priv->phys = devm_kcalloc(dev, priv->nphys, sizeof(*priv->phys),
				  GFP_KERNEL);
	if (!priv->phys)
		return -ENOMEM;

	priv->phy_rstc = reset_control_get(dev, "phy");
	if (IS_ERR(priv->phy_rstc))
		return PTR_ERR(priv->phy_rstc);

	priv->phycom_rstc = reset_control_get(dev, "common");
	if (IS_ERR(priv->phycom_rstc))
		return PTR_ERR(priv->phycom_rstc);

	mutex_init(&priv->phy_mutex);

	dev_set_drvdata(dev, priv);

	for_each_available_child_of_node(dev->of_node, child) {
		struct phy_msm8996_desc *phy_desc;

		if (of_property_read_u32(child, "reg", &phy_id)) {
			dev_err(dev, "missing reg property in node %s\n",
				child->name);
			ret = -EINVAL;
			goto put_child;
		}

		phy_desc = devm_kzalloc(dev, sizeof(*phy_desc), GFP_KERNEL);
		if (!phy_desc) {
			ret = -ENOMEM;
			goto put_child;
		}

		phy = devm_phy_create(dev, NULL, &qcom_msm8996_pcie_phy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create PHY %d\n", phy_id);
			ret = PTR_ERR(phy);
			goto put_child;
		}

		phy_desc->phy_rstc = of_reset_control_get(child, "phy");
		if (IS_ERR(phy_desc->phy_rstc)) {
			ret = PTR_ERR(phy_desc->phy_rstc);
			goto put_child;
		}

		phy_desc->phy = phy;
		phy_desc->priv = priv;
		phy_desc->index = phy_id;
		phy_set_drvdata(phy, phy_desc);
		priv->phys[phy_id] = phy_desc;
	}

	phy_provider =
		devm_of_phy_provider_register(dev, phy_msm8996_pcie_phy_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);

put_child:
	of_node_put(child);
	return ret;
}

static const struct of_device_id qcom_msm8996_pcie_phy_of_match[] = {
	{ .compatible = "qcom,msm8996-pcie-phy" },
	{ },
};
MODULE_DEVICE_TABLE(of, qcom_msm8996_pcie_phy_of_match);

static struct platform_driver qcom_msm8996_pcie_phy_driver = {
	.probe	= qcom_msm8996_pcie_phy_probe,
	.driver = {
		.name	= "qcom-msm8996-pcie-phy",
		.of_match_table	= qcom_msm8996_pcie_phy_of_match,
	}
};
module_platform_driver(qcom_msm8996_pcie_phy_driver);

MODULE_AUTHOR("Srinivas Kandagatla <srinivas.kandagatla@st.com>");
MODULE_DESCRIPTION("QCOM MSM899 PCIE PHY driver");
MODULE_LICENSE("GPL v2");
