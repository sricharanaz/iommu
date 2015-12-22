/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __HDMI_CONNECTOR_H__
#define __HDMI_CONNECTOR_H__

#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/hdmi.h>
#include <linux/clk-provider.h>

#include "msm_drv.h"
#include "hdmi.xml.h"

enum hdmi_gpio {
	HDMI_GPIO_DDC_CLK,
	HDMI_GPIO_DDC_DATA,
	HDMI_GPIO_TX_HPD,
	HDMI_GPIO_MUX_EN,
	HDMI_GPIO_MUX_SEL,
	HDMI_GPIO_MUX_LPM,
	HDMI_GPIO_MAX,
};

struct hdmi_phy;
struct hdmi_pll;
struct hdmi_platform_config;

struct hdmi_audio {
	bool enabled;
	struct hdmi_audio_infoframe infoframe;
	int rate;
};

struct hdmi_hdcp_ctrl;

struct hdmi {
	struct drm_device *dev;
	struct platform_device *pdev;

	const struct hdmi_platform_config *config;

	/* audio state: */
	struct hdmi_audio audio;

	/* video state: */
	bool power_on;
	unsigned long int pixclock;

	void __iomem *mmio;
	void __iomem *qfprom_mmio;
	phys_addr_t mmio_phy_addr;

	struct regulator **hpd_regs;
	struct regulator **pwr_regs;
	struct clk **hpd_clks;
	struct clk **pwr_clks;

	struct hdmi_phy *phy;
	struct i2c_adapter *i2c;
	struct drm_connector *connector;
	struct drm_bridge *bridge;

	/* the encoder we are hooked to (outside of hdmi block) */
	struct drm_encoder *encoder;

	bool hdmi_mode;               /* are we in hdmi mode? */

	int irq;
	struct workqueue_struct *workq;

	struct hdmi_hdcp_ctrl *hdcp_ctrl;

	/*
	* spinlock to protect registers shared by different execution
	* REG_HDMI_CTRL
	* REG_HDMI_DDC_ARBITRATION
	* REG_HDMI_HDCP_INT_CTRL
	* REG_HDMI_HPD_CTRL
	*/
	spinlock_t reg_lock;
};

/* platform config data (ie. from DT, or pdata) */
struct hdmi_platform_config {
	struct hdmi_phy *(*phy_init)(struct hdmi *hdmi);
	const char *mmio_name;
	const char *qfprom_mmio_name;

	/* regulators that need to be on for hpd: */
	const char **hpd_reg_names;
	int hpd_reg_cnt;

	/* regulators that need to be on for screen pwr: */
	const char **pwr_reg_names;
	int pwr_reg_cnt;

	/* clks that need to be on for hpd: */
	const char **hpd_clk_names;
	const long unsigned *hpd_freq;
	int hpd_clk_cnt;

	/* clks that need to be on for screen pwr (ie pixel clk): */
	const char **pwr_clk_names;
	int pwr_clk_cnt;

	/* gpio's: */
	int gpio[HDMI_GPIO_MAX];
};

void hdmi_set_mode(struct hdmi *hdmi, bool power_on);

static inline void hdmi_write(struct hdmi *hdmi, u32 reg, u32 data)
{
	msm_writel(data, hdmi->mmio + reg);
}

static inline u32 hdmi_read(struct hdmi *hdmi, u32 reg)
{
	return msm_readl(hdmi->mmio + reg);
}

static inline u32 hdmi_qfprom_read(struct hdmi *hdmi, u32 reg)
{
	return msm_readl(hdmi->qfprom_mmio + reg);
}

/*
 * hdmi phy:
 */
struct hdmi_phy_funcs {
	void (*destroy)(struct hdmi_phy *phy);
	void (*powerup)(struct hdmi_phy *phy, unsigned long int pixclock);
	void (*powerdown)(struct hdmi_phy *phy);
};

enum hdmi_phy_type {
	MSM_HDMI_PHY_8x60,
	MSM_HDMI_PHY_8960,
	MSM_HDMI_PHY_8x74,
	MSM_HDMI_PHY_MAX,
};

struct hdmi_phy_cfg {
	enum hdmi_phy_type type;
	void (*powerup)(struct hdmi_phy *phy, unsigned long int pixclock);
	void (*powerdown)(struct hdmi_phy *phy);
	const char * const *reg_names;
	int num_regs;
	const char * const *clk_names;
	int num_clks;
};

extern const struct hdmi_phy_cfg hdmi_phy_8x60_cfg;
extern const struct hdmi_phy_cfg hdmi_phy_8960_cfg;
extern const struct hdmi_phy_cfg hdmi_phy_8x74_cfg;

struct hdmi_phy {
	struct platform_device *pdev;
	void __iomem *mmio;
	struct hdmi_phy_cfg *cfg;
	struct hdmi_pll *pll;
	const struct hdmi_phy_funcs *funcs;
	struct regulator **regs;
	struct clk **clks;
};

struct hdmi_phy *hdmi_phy_8960_init(struct hdmi *hdmi);
struct hdmi_phy *hdmi_phy_8x60_init(struct hdmi *hdmi);
struct hdmi_phy *hdmi_phy_8x74_init(struct hdmi *hdmi);

static inline void hdmi_phy_write(struct hdmi_phy *phy, u32 reg, u32 data)
{
	msm_writel(data, phy->mmio + reg);
}

static inline u32 hdmi_phy_read(struct hdmi_phy *phy, u32 reg)
{
	return msm_readl(phy->mmio + reg);
}

int hdmi_phy_resource_enable(struct hdmi_phy *phy);
void hdmi_phy_resource_disable(struct hdmi_phy *phy);
void hdmi_phy_powerup(struct hdmi_phy *phy, unsigned long int pixclock);
void hdmi_phy_powerdown(struct hdmi_phy *phy);
void __init hdmi_phy_driver_register(void);
void __exit hdmi_phy_driver_unregister(void);

 /*
 * hdmi pll:
 */
struct hdmi_pll {
	enum hdmi_phy_type type;
	struct clk_hw clk_hw;
};

#define hw_clk_to_pll(x) container_of(x, struct hdmi_pll, clk_hw)

#ifdef CONFIG_COMMON_CLK
struct hdmi_pll *hdmi_pll_8960_init(struct platform_device *pdev);
#else
struct hdmi_pll *hdmi_pll_8960_init(struct platform_device *pdev)
{
	return ERR_PTR(-ENODEV);
}
#endif

/*
 * audio:
 */

int hdmi_audio_update(struct hdmi *hdmi);
int hdmi_audio_info_setup(struct hdmi *hdmi, bool enabled,
	uint32_t num_of_channels, uint32_t channel_allocation,
	uint32_t level_shift, bool down_mix);
void hdmi_audio_set_sample_rate(struct hdmi *hdmi, int rate);


/*
 * hdmi bridge:
 */

struct drm_bridge *hdmi_bridge_init(struct hdmi *hdmi);
void hdmi_bridge_destroy(struct drm_bridge *bridge);

/*
 * hdmi connector:
 */

void hdmi_connector_irq(struct drm_connector *connector);
struct drm_connector *hdmi_connector_init(struct hdmi *hdmi);

/*
 * i2c adapter for ddc:
 */

void hdmi_i2c_irq(struct i2c_adapter *i2c);
void hdmi_i2c_destroy(struct i2c_adapter *i2c);
struct i2c_adapter *hdmi_i2c_init(struct hdmi *hdmi);

/*
 * hdcp
 */
struct hdmi_hdcp_ctrl *hdmi_hdcp_init(struct hdmi *hdmi);
void hdmi_hdcp_destroy(struct hdmi *hdmi);
void hdmi_hdcp_on(struct hdmi_hdcp_ctrl *hdcp_ctrl);
void hdmi_hdcp_off(struct hdmi_hdcp_ctrl *hdcp_ctrl);
void hdmi_hdcp_irq(struct hdmi_hdcp_ctrl *hdcp_ctrl);

#endif /* __HDMI_CONNECTOR_H__ */
