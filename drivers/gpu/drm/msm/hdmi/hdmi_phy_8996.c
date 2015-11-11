/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#include "hdmi.h"

/* CONSTANTS */
#define HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD	3400000000
#define HDMI_DIG_FREQ_BIT_CLK_THRESHOLD		1500000000
#define HDMI_MID_FREQ_BIT_CLK_THRESHOLD		750000000
#define HDMI_CORECLK_DIV			5
#define HDMI_REF_CLOCK				19200000
#define HDMI_PLL_CMP_CNT			1024

#define HDMI_VCO_MAX_FREQ                        12000000000
#define HDMI_VCO_MIN_FREQ                        8000000000

/* PLL REGISTERS */
#define QSERDES_COM_ATB_SEL1                     (0x000)
#define QSERDES_COM_ATB_SEL2                     (0x004)
#define QSERDES_COM_FREQ_UPDATE                  (0x008)
#define QSERDES_COM_BG_TIMER                     (0x00C)
#define QSERDES_COM_SSC_EN_CENTER                (0x010)
#define QSERDES_COM_SSC_ADJ_PER1                 (0x014)
#define QSERDES_COM_SSC_ADJ_PER2                 (0x018)
#define QSERDES_COM_SSC_PER1                     (0x01C)
#define QSERDES_COM_SSC_PER2                     (0x020)
#define QSERDES_COM_SSC_STEP_SIZE1               (0x024)
#define QSERDES_COM_SSC_STEP_SIZE2               (0x028)
#define QSERDES_COM_POST_DIV                     (0x02C)
#define QSERDES_COM_POST_DIV_MUX                 (0x030)
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN          (0x034)
#define QSERDES_COM_CLK_ENABLE1                  (0x038)
#define QSERDES_COM_SYS_CLK_CTRL                 (0x03C)
#define QSERDES_COM_SYSCLK_BUF_ENABLE            (0x040)
#define QSERDES_COM_PLL_EN                       (0x044)
#define QSERDES_COM_PLL_IVCO                     (0x048)
#define QSERDES_COM_LOCK_CMP1_MODE0              (0x04C)
#define QSERDES_COM_LOCK_CMP2_MODE0              (0x050)
#define QSERDES_COM_LOCK_CMP3_MODE0              (0x054)
#define QSERDES_COM_LOCK_CMP1_MODE1              (0x058)
#define QSERDES_COM_LOCK_CMP2_MODE1              (0x05C)
#define QSERDES_COM_LOCK_CMP3_MODE1              (0x060)
#define QSERDES_COM_LOCK_CMP1_MODE2              (0x064)
#define QSERDES_COM_CMN_RSVD0                    (0x064)
#define QSERDES_COM_LOCK_CMP2_MODE2              (0x068)
#define QSERDES_COM_EP_CLOCK_DETECT_CTRL         (0x068)
#define QSERDES_COM_LOCK_CMP3_MODE2              (0x06C)
#define QSERDES_COM_SYSCLK_DET_COMP_STATUS       (0x06C)
#define QSERDES_COM_BG_TRIM                      (0x070)
#define QSERDES_COM_CLK_EP_DIV                   (0x074)
#define QSERDES_COM_CP_CTRL_MODE0                (0x078)
#define QSERDES_COM_CP_CTRL_MODE1                (0x07C)
#define QSERDES_COM_CP_CTRL_MODE2                (0x080)
#define QSERDES_COM_CMN_RSVD1                    (0x080)
#define QSERDES_COM_PLL_RCTRL_MODE0              (0x084)
#define QSERDES_COM_PLL_RCTRL_MODE1              (0x088)
#define QSERDES_COM_PLL_RCTRL_MODE2              (0x08C)
#define QSERDES_COM_CMN_RSVD2                    (0x08C)
#define QSERDES_COM_PLL_CCTRL_MODE0              (0x090)
#define QSERDES_COM_PLL_CCTRL_MODE1              (0x094)
#define QSERDES_COM_PLL_CCTRL_MODE2              (0x098)
#define QSERDES_COM_CMN_RSVD3                    (0x098)
#define QSERDES_COM_PLL_CNTRL                    (0x09C)
#define QSERDES_COM_PHASE_SEL_CTRL               (0x0A0)
#define QSERDES_COM_PHASE_SEL_DC                 (0x0A4)
#define QSERDES_COM_CORE_CLK_IN_SYNC_SEL         (0x0A8)
#define QSERDES_COM_BIAS_EN_CTRL_BY_PSM          (0x0A8)
#define QSERDES_COM_SYSCLK_EN_SEL                (0x0AC)
#define QSERDES_COM_CML_SYSCLK_SEL               (0x0B0)
#define QSERDES_COM_RESETSM_CNTRL                (0x0B4)
#define QSERDES_COM_RESETSM_CNTRL2               (0x0B8)
#define QSERDES_COM_RESTRIM_CTRL                 (0x0BC)
#define QSERDES_COM_RESTRIM_CTRL2                (0x0C0)
#define QSERDES_COM_RESCODE_DIV_NUM              (0x0C4)
#define QSERDES_COM_LOCK_CMP_EN                  (0x0C8)
#define QSERDES_COM_LOCK_CMP_CFG                 (0x0CC)
#define QSERDES_COM_DEC_START_MODE0              (0x0D0)
#define QSERDES_COM_DEC_START_MODE1              (0x0D4)
#define QSERDES_COM_DEC_START_MODE2              (0x0D8)
#define QSERDES_COM_VCOCAL_DEADMAN_CTRL          (0x0D8)
#define QSERDES_COM_DIV_FRAC_START1_MODE0        (0x0DC)
#define QSERDES_COM_DIV_FRAC_START2_MODE0        (0x0E0)
#define QSERDES_COM_DIV_FRAC_START3_MODE0        (0x0E4)
#define QSERDES_COM_DIV_FRAC_START1_MODE1        (0x0E8)
#define QSERDES_COM_DIV_FRAC_START2_MODE1        (0x0EC)
#define QSERDES_COM_DIV_FRAC_START3_MODE1        (0x0F0)
#define QSERDES_COM_DIV_FRAC_START1_MODE2        (0x0F4)
#define QSERDES_COM_VCO_TUNE_MINVAL1             (0x0F4)
#define QSERDES_COM_DIV_FRAC_START2_MODE2        (0x0F8)
#define QSERDES_COM_VCO_TUNE_MINVAL2             (0x0F8)
#define QSERDES_COM_DIV_FRAC_START3_MODE2        (0x0FC)
#define QSERDES_COM_CMN_RSVD4                    (0x0FC)
#define QSERDES_COM_INTEGLOOP_INITVAL            (0x100)
#define QSERDES_COM_INTEGLOOP_EN                 (0x104)
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE0        (0x108)
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE0        (0x10C)
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE1        (0x110)
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE1        (0x114)
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE2        (0x118)
#define QSERDES_COM_VCO_TUNE_MAXVAL1             (0x118)
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE2        (0x11C)
#define QSERDES_COM_VCO_TUNE_MAXVAL2             (0x11C)
#define QSERDES_COM_RES_TRIM_CONTROL2            (0x120)
#define QSERDES_COM_VCO_TUNE_CTRL                (0x124)
#define QSERDES_COM_VCO_TUNE_MAP                 (0x128)
#define QSERDES_COM_VCO_TUNE1_MODE0              (0x12C)
#define QSERDES_COM_VCO_TUNE2_MODE0              (0x130)
#define QSERDES_COM_VCO_TUNE1_MODE1              (0x134)
#define QSERDES_COM_VCO_TUNE2_MODE1              (0x138)
#define QSERDES_COM_VCO_TUNE1_MODE2              (0x13C)
#define QSERDES_COM_VCO_TUNE_INITVAL1            (0x13C)
#define QSERDES_COM_VCO_TUNE2_MODE2              (0x140)
#define QSERDES_COM_VCO_TUNE_INITVAL2            (0x140)
#define QSERDES_COM_VCO_TUNE_TIMER1              (0x144)
#define QSERDES_COM_VCO_TUNE_TIMER2              (0x148)
#define QSERDES_COM_SAR                          (0x14C)
#define QSERDES_COM_SAR_CLK                      (0x150)
#define QSERDES_COM_SAR_CODE_OUT_STATUS          (0x154)
#define QSERDES_COM_SAR_CODE_READY_STATUS        (0x158)
#define QSERDES_COM_CMN_STATUS                   (0x15C)
#define QSERDES_COM_RESET_SM_STATUS              (0x160)
#define QSERDES_COM_RESTRIM_CODE_STATUS          (0x164)
#define QSERDES_COM_PLLCAL_CODE1_STATUS          (0x168)
#define QSERDES_COM_PLLCAL_CODE2_STATUS          (0x16C)
#define QSERDES_COM_BG_CTRL                      (0x170)
#define QSERDES_COM_CLK_SELECT                   (0x174)
#define QSERDES_COM_HSCLK_SEL                    (0x178)
#define QSERDES_COM_INTEGLOOP_BINCODE_STATUS     (0x17C)
#define QSERDES_COM_PLL_ANALOG                   (0x180)
#define QSERDES_COM_CORECLK_DIV                  (0x184)
#define QSERDES_COM_SW_RESET                     (0x188)
#define QSERDES_COM_CORE_CLK_EN                  (0x18C)
#define QSERDES_COM_C_READY_STATUS               (0x190)
#define QSERDES_COM_CMN_CONFIG                   (0x194)
#define QSERDES_COM_CMN_RATE_OVERRIDE            (0x198)
#define QSERDES_COM_SVS_MODE_CLK_SEL             (0x19C)
#define QSERDES_COM_DEBUG_BUS0                   (0x1A0)
#define QSERDES_COM_DEBUG_BUS1                   (0x1A4)
#define QSERDES_COM_DEBUG_BUS2                   (0x1A8)
#define QSERDES_COM_DEBUG_BUS3                   (0x1AC)
#define QSERDES_COM_DEBUG_BUS_SEL                (0x1B0)
#define QSERDES_COM_CMN_MISC1                    (0x1B4)
#define QSERDES_COM_CMN_MISC2                    (0x1B8)
#define QSERDES_COM_CORECLK_DIV_MODE1            (0x1BC)
#define QSERDES_COM_CORECLK_DIV_MODE2            (0x1C0)
#define QSERDES_COM_CMN_RSVD5                    (0x1C0)

/* Tx Channel base addresses */
#define HDMI_TX_L0_BASE_OFFSET                   (0x400)
#define HDMI_TX_L1_BASE_OFFSET                   (0x600)
#define HDMI_TX_L2_BASE_OFFSET                   (0x800)
#define HDMI_TX_L3_BASE_OFFSET                   (0xA00)

/* Tx Channel PHY registers */
#define QSERDES_TX_L0_BIST_MODE_LANENO                    (0x000)
#define QSERDES_TX_L0_BIST_INVERT                         (0x004)
#define QSERDES_TX_L0_CLKBUF_ENABLE                       (0x008)
#define QSERDES_TX_L0_CMN_CONTROL_ONE                     (0x00C)
#define QSERDES_TX_L0_CMN_CONTROL_TWO                     (0x010)
#define QSERDES_TX_L0_CMN_CONTROL_THREE                   (0x014)
#define QSERDES_TX_L0_TX_EMP_POST1_LVL                    (0x018)
#define QSERDES_TX_L0_TX_POST2_EMPH                       (0x01C)
#define QSERDES_TX_L0_TX_BOOST_LVL_UP_DN                  (0x020)
#define QSERDES_TX_L0_HP_PD_ENABLES                       (0x024)
#define QSERDES_TX_L0_TX_IDLE_LVL_LARGE_AMP               (0x028)
#define QSERDES_TX_L0_TX_DRV_LVL                          (0x02C)
#define QSERDES_TX_L0_TX_DRV_LVL_OFFSET                   (0x030)
#define QSERDES_TX_L0_RESET_TSYNC_EN                      (0x034)
#define QSERDES_TX_L0_PRE_STALL_LDO_BOOST_EN              (0x038)
#define QSERDES_TX_L0_TX_BAND                             (0x03C)
#define QSERDES_TX_L0_SLEW_CNTL                           (0x040)
#define QSERDES_TX_L0_INTERFACE_SELECT                    (0x044)
#define QSERDES_TX_L0_LPB_EN                              (0x048)
#define QSERDES_TX_L0_RES_CODE_LANE_TX                    (0x04C)
#define QSERDES_TX_L0_RES_CODE_LANE_RX                    (0x050)
#define QSERDES_TX_L0_RES_CODE_LANE_OFFSET                (0x054)
#define QSERDES_TX_L0_PERL_LENGTH1                        (0x058)
#define QSERDES_TX_L0_PERL_LENGTH2                        (0x05C)
#define QSERDES_TX_L0_SERDES_BYP_EN_OUT                   (0x060)
#define QSERDES_TX_L0_DEBUG_BUS_SEL                       (0x064)
#define QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN    (0x068)
#define QSERDES_TX_L0_TX_POL_INV                          (0x06C)
#define QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN          (0x070)
#define QSERDES_TX_L0_BIST_PATTERN1                       (0x074)
#define QSERDES_TX_L0_BIST_PATTERN2                       (0x078)
#define QSERDES_TX_L0_BIST_PATTERN3                       (0x07C)
#define QSERDES_TX_L0_BIST_PATTERN4                       (0x080)
#define QSERDES_TX_L0_BIST_PATTERN5                       (0x084)
#define QSERDES_TX_L0_BIST_PATTERN6                       (0x088)
#define QSERDES_TX_L0_BIST_PATTERN7                       (0x08C)
#define QSERDES_TX_L0_BIST_PATTERN8                       (0x090)
#define QSERDES_TX_L0_LANE_MODE                           (0x094)
#define QSERDES_TX_L0_IDAC_CAL_LANE_MODE                  (0x098)
#define QSERDES_TX_L0_IDAC_CAL_LANE_MODE_CONFIGURATION    (0x09C)
#define QSERDES_TX_L0_ATB_SEL1                            (0x0A0)
#define QSERDES_TX_L0_ATB_SEL2                            (0x0A4)
#define QSERDES_TX_L0_RCV_DETECT_LVL                      (0x0A8)
#define QSERDES_TX_L0_RCV_DETECT_LVL_2                    (0x0AC)
#define QSERDES_TX_L0_PRBS_SEED1                          (0x0B0)
#define QSERDES_TX_L0_PRBS_SEED2                          (0x0B4)
#define QSERDES_TX_L0_PRBS_SEED3                          (0x0B8)
#define QSERDES_TX_L0_PRBS_SEED4                          (0x0BC)
#define QSERDES_TX_L0_RESET_GEN                           (0x0C0)
#define QSERDES_TX_L0_RESET_GEN_MUXES                     (0x0C4)
#define QSERDES_TX_L0_TRAN_DRVR_EMP_EN                    (0x0C8)
#define QSERDES_TX_L0_TX_INTERFACE_MODE                   (0x0CC)
#define QSERDES_TX_L0_PWM_CTRL                            (0x0D0)
#define QSERDES_TX_L0_PWM_ENCODED_OR_DATA                 (0x0D4)
#define QSERDES_TX_L0_PWM_GEAR_1_DIVIDER_BAND2            (0x0D8)
#define QSERDES_TX_L0_PWM_GEAR_2_DIVIDER_BAND2            (0x0DC)
#define QSERDES_TX_L0_PWM_GEAR_3_DIVIDER_BAND2            (0x0E0)
#define QSERDES_TX_L0_PWM_GEAR_4_DIVIDER_BAND2            (0x0E4)
#define QSERDES_TX_L0_PWM_GEAR_1_DIVIDER_BAND0_1          (0x0E8)
#define QSERDES_TX_L0_PWM_GEAR_2_DIVIDER_BAND0_1          (0x0EC)
#define QSERDES_TX_L0_PWM_GEAR_3_DIVIDER_BAND0_1          (0x0F0)
#define QSERDES_TX_L0_PWM_GEAR_4_DIVIDER_BAND0_1          (0x0F4)
#define QSERDES_TX_L0_VMODE_CTRL1                         (0x0F8)
#define QSERDES_TX_L0_VMODE_CTRL2                         (0x0FC)
#define QSERDES_TX_L0_TX_ALOG_INTF_OBSV_CNTL              (0x100)
#define QSERDES_TX_L0_BIST_STATUS                         (0x104)
#define QSERDES_TX_L0_BIST_ERROR_COUNT1                   (0x108)
#define QSERDES_TX_L0_BIST_ERROR_COUNT2                   (0x10C)
#define QSERDES_TX_L0_TX_ALOG_INTF_OBSV                   (0x110)

/* HDMI PHY REGISTERS */
#define HDMI_PHY_BASE_OFFSET                  (0xC00)

#define HDMI_PHY_CFG                          (0x00)
#define HDMI_PHY_PD_CTL                       (0x04)
#define HDMI_PHY_MODE                         (0x08)
#define HDMI_PHY_MISR_CLEAR                   (0x0C)
#define HDMI_PHY_TX0_TX1_BIST_CFG0            (0x10)
#define HDMI_PHY_TX0_TX1_BIST_CFG1            (0x14)
#define HDMI_PHY_TX0_TX1_PRBS_SEED_BYTE0      (0x18)
#define HDMI_PHY_TX0_TX1_PRBS_SEED_BYTE1      (0x1C)
#define HDMI_PHY_TX0_TX1_BIST_PATTERN0        (0x20)
#define HDMI_PHY_TX0_TX1_BIST_PATTERN1        (0x24)
#define HDMI_PHY_TX2_TX3_BIST_CFG0            (0x28)
#define HDMI_PHY_TX2_TX3_BIST_CFG1            (0x2C)
#define HDMI_PHY_TX2_TX3_PRBS_SEED_BYTE0      (0x30)
#define HDMI_PHY_TX2_TX3_PRBS_SEED_BYTE1      (0x34)
#define HDMI_PHY_TX2_TX3_BIST_PATTERN0        (0x38)
#define HDMI_PHY_TX2_TX3_BIST_PATTERN1        (0x3C)
#define HDMI_PHY_DEBUG_BUS_SEL                (0x40)
#define HDMI_PHY_TXCAL_CFG0                   (0x44)
#define HDMI_PHY_TXCAL_CFG1                   (0x48)
#define HDMI_PHY_TX0_TX1_LANE_CTL             (0x4C)
#define HDMI_PHY_TX2_TX3_LANE_CTL             (0x50)
#define HDMI_PHY_LANE_BIST_CONFIG             (0x54)
#define HDMI_PHY_CLOCK                        (0x58)
#define HDMI_PHY_MISC1                        (0x5C)
#define HDMI_PHY_MISC2                        (0x60)
#define HDMI_PHY_TX0_TX1_BIST_STATUS0         (0x64)
#define HDMI_PHY_TX0_TX1_BIST_STATUS1         (0x68)
#define HDMI_PHY_TX0_TX1_BIST_STATUS2         (0x6C)
#define HDMI_PHY_TX2_TX3_BIST_STATUS0         (0x70)
#define HDMI_PHY_TX2_TX3_BIST_STATUS1         (0x74)
#define HDMI_PHY_TX2_TX3_BIST_STATUS2         (0x78)
#define HDMI_PHY_PRE_MISR_STATUS0             (0x7C)
#define HDMI_PHY_PRE_MISR_STATUS1             (0x80)
#define HDMI_PHY_PRE_MISR_STATUS2             (0x84)
#define HDMI_PHY_PRE_MISR_STATUS3             (0x88)
#define HDMI_PHY_POST_MISR_STATUS0            (0x8C)
#define HDMI_PHY_POST_MISR_STATUS1            (0x90)
#define HDMI_PHY_POST_MISR_STATUS2            (0x94)
#define HDMI_PHY_POST_MISR_STATUS3            (0x98)
#define HDMI_PHY_STATUS                       (0x9C)
#define HDMI_PHY_MISC3_STATUS                 (0xA0)
#define HDMI_PHY_MISC4_STATUS                 (0xA4)
#define HDMI_PHY_DEBUG_BUS0                   (0xA8)
#define HDMI_PHY_DEBUG_BUS1                   (0xAC)
#define HDMI_PHY_DEBUG_BUS2                   (0xB0)
#define HDMI_PHY_DEBUG_BUS3                   (0xB4)
#define HDMI_PHY_PHY_REVISION_ID0             (0xB8)
#define HDMI_PHY_PHY_REVISION_ID1             (0xBC)
#define HDMI_PHY_PHY_REVISION_ID2             (0xC0)
#define HDMI_PHY_PHY_REVISION_ID3             (0xC4)

#define HDMI_PLL_POLL_MAX_READS                2500
#define HDMI_PLL_POLL_TIMEOUT_MS               150

struct hdmi_pll_8996 {
	struct hdmi_pll base;

	struct platform_device *pdev;
	void __iomem *mmio;

	unsigned long pixclk;
};

struct hdmi_8996_phy_pll_reg_cfg {
	u32 tx_l0_lane_mode;
	u32 tx_l2_lane_mode;
	u32 tx_l0_tx_band;
	u32 tx_l1_tx_band;
	u32 tx_l2_tx_band;
	u32 tx_l3_tx_band;
	u32 com_svs_mode_clk_sel;
	u32 com_hsclk_sel;
	u32 com_pll_cctrl_mode0;
	u32 com_pll_rctrl_mode0;
	u32 com_cp_ctrl_mode0;
	u32 com_dec_start_mode0;
	u32 com_div_frac_start1_mode0;
	u32 com_div_frac_start2_mode0;
	u32 com_div_frac_start3_mode0;
	u32 com_integloop_gain0_mode0;
	u32 com_integloop_gain1_mode0;
	u32 com_lock_cmp_en;
	u32 com_lock_cmp1_mode0;
	u32 com_lock_cmp2_mode0;
	u32 com_lock_cmp3_mode0;
	u32 com_core_clk_en;
	u32 com_coreclk_div;
	u32 com_vco_tune_ctrl;

	u32 tx_l0_tx_drv_lvl;
	u32 tx_l0_tx_emp_post1_lvl;
	u32 tx_l1_tx_drv_lvl;
	u32 tx_l1_tx_emp_post1_lvl;
	u32 tx_l2_tx_drv_lvl;
	u32 tx_l2_tx_emp_post1_lvl;
	u32 tx_l3_tx_drv_lvl;
	u32 tx_l3_tx_emp_post1_lvl;
	u32 tx_l0_vmode_ctrl1;
	u32 tx_l0_vmode_ctrl2;
	u32 tx_l1_vmode_ctrl1;
	u32 tx_l1_vmode_ctrl2;
	u32 tx_l2_vmode_ctrl1;
	u32 tx_l2_vmode_ctrl2;
	u32 tx_l3_vmode_ctrl1;
	u32 tx_l3_vmode_ctrl2;
	u32 tx_l0_res_code_lane_tx;
	u32 tx_l1_res_code_lane_tx;
	u32 tx_l2_res_code_lane_tx;
	u32 tx_l3_res_code_lane_tx;

	u32 phy_mode;
};

struct hdmi_8996_post_divider {
	u64 vco_freq;
	int hsclk_divsel;
	int vco_ratio;
	int tx_band_sel;
	int half_rate_mode;
};

static inline struct hdmi_pll_8996 *to_hdmi_pll_8996(struct hdmi_pll *pll)
{
	return container_of(pll, struct hdmi_pll_8996, base);
}

static inline struct hdmi_phy *pll_8996_get_phy(struct hdmi_pll_8996 *pll_8996)
{
	return platform_get_drvdata(pll_8996->pdev);
}

static inline u32 pll_get_cpctrl(u64 frac_start, bool gen_ssc)
{
	if ((frac_start != 0) || (gen_ssc == true))
		return (11000000 / (HDMI_REF_CLOCK / 20));

	return 0x23;
}

static inline u32 pll_get_rctrl(u64 frac_start, bool gen_ssc)
{
	if ((frac_start != 0) || (gen_ssc == true))
		return 0x16;

	return 0x10;
}

static inline u32 pll_get_cctrl(u64 frac_start, bool gen_ssc)
{
	if ((frac_start != 0) || (gen_ssc == true))
		return 0x28;

	return 0x1;
}

static inline u32 pll_get_integloop_gain(u64 frac_start, u64 bclk, u32 ref_clk,
					 bool gen_ssc)
{
	u64 digclk_divsel = bclk >= HDMI_DIG_FREQ_BIT_CLK_THRESHOLD ? 1 : 2;
	u64 base;

	if ((frac_start != 0) || (gen_ssc == true))
		base = (64 * ref_clk) / HDMI_REF_CLOCK;
	else
		base = (1022 * ref_clk) / 100;

	base <<= digclk_divsel;

	return (base <= 2046 ? base : 2046);
}

static inline u32 pll_get_pll_cmp(u64 fdata)
{
	u64 dividend = HDMI_PLL_CMP_CNT * fdata;
	u64 divisor = HDMI_REF_CLOCK * 10;
	u64 rem;

	rem = do_div(dividend, divisor);
	if (rem > (divisor >> 1))
		dividend++;

	return dividend - 1;
}

static inline u64 pll_cmp_to_fdata(u32 pll_cmp)
{
	u64 fdata = ((u64)pll_cmp) * HDMI_REF_CLOCK * 10;

	do_div(fdata, HDMI_PLL_CMP_CNT);

	return fdata;
}

static int pll_get_post_div(struct hdmi_8996_post_divider *pd, u64 bclk)
{
	int ratio[] = { 2, 3, 4, 5, 6, 9, 10, 12, 14, 15, 20, 21, 25, 28, 35 };
	int hs_divsel[] = { 0, 4, 8, 12, 1, 5, 2, 9, 3, 13 , 10, 7, 14, 11, 15 };
	int tx_band_sel[] = { 0, 1, 2, 3 };
	u64 vco_freq[60];
	u64 vco, vco_optimal;
	int half_rate_mode = 0;
	int vco_optimal_index, vco_freq_index;
	int i, j;

retry:
	vco_optimal = HDMI_VCO_MAX_FREQ;
	vco_optimal_index = -1;
	vco_freq_index = 0;
	for (i = 0; i < 15; i++) {
		for (j = 0; j < 4; j++) {
			u32 ratio_mult = ratio[i] << tx_band_sel[j];

			vco = bclk >> half_rate_mode;
			vco *= ratio_mult;
			vco_freq[vco_freq_index++] = vco;
		}
	}

	for (i = 0; i < 60; i++) {
		u64 vco_tmp = vco_freq[i];

		if ((vco_tmp >= HDMI_VCO_MIN_FREQ) &&
				(vco_tmp <= vco_optimal)) {
			vco_optimal = vco_tmp;
			vco_optimal_index = i;
		}
	}

	if (vco_optimal_index == -1) {
		if (!half_rate_mode) {
			half_rate_mode = 1;
			goto retry;
		}
	} else {
		pd->vco_freq = vco_optimal;
		pd->tx_band_sel = tx_band_sel[vco_optimal_index % 4];
		pd->vco_ratio = ratio[vco_optimal_index / 4];
		pd->hsclk_divsel = hs_divsel[vco_optimal_index / 4];

		return 0;
	}

	return -EINVAL;
}

static int pll_calculate(unsigned long pix_clk, struct hdmi_8996_phy_pll_reg_cfg *cfg)
{
	struct hdmi_8996_post_divider pd;
	u64 fdata, tmds_clk;
	u64 bclk;
	u32 pll_cmp;
	u64 dec_start;
	u64 frac_start;
	u64 pll_divisor;
	u32 cpctrl;
	u32 rctrl;
	u32 cctrl;
	u32 integloop_gain;
	u64 rem;
	int ret;

	/* bit clk = 10 * pix_clk */
	bclk = ((u64)pix_clk) * 10;

	if (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD)
		tmds_clk = pix_clk >> 2;
	else
		tmds_clk = pix_clk;

	ret = pll_get_post_div(&pd, bclk);
	if (ret)
		return ret;

	dec_start = pd.vco_freq;
	pll_divisor = 4 * HDMI_REF_CLOCK;
	do_div(dec_start, pll_divisor);

	frac_start = pd.vco_freq * (1 << 20);

	rem = do_div(frac_start, pll_divisor);
	frac_start -= dec_start * (1 << 20);
	if (rem > (pll_divisor >> 1))
		frac_start++;

	cpctrl = pll_get_cpctrl(frac_start, false);
	rctrl = pll_get_rctrl(frac_start, false);
	cctrl = pll_get_cctrl(frac_start, false);
	integloop_gain = pll_get_integloop_gain(frac_start, bclk,
				HDMI_REF_CLOCK, false);

	fdata = pd.vco_freq;
	do_div(fdata, pd.vco_ratio);

	pll_cmp = pll_get_pll_cmp(fdata);

	DBG("VCO freq: %llu", pd.vco_freq);
	DBG("fdata: %llu", fdata);
	DBG("pix_clk: %lu", pix_clk);
	DBG("tmds clk: %llu", tmds_clk);
	DBG("HSCLK_SEL: %d", pd.hsclk_divsel);
	DBG("DEC_START: %llu", dec_start);
	DBG("DIV_FRAC_START: %llu", frac_start);
	DBG("PLL_CPCTRL: %u", cpctrl);
	DBG("PLL_RCTRL: %u", rctrl);
	DBG("PLL_CCTRL: %u", cctrl);
	DBG("INTEGLOOP_GAIN: %u", integloop_gain);
	DBG("TX_BAND: %d", pd.tx_band_sel);
	DBG("PLL_CMP: %u", pll_cmp);

	/* Convert these values to register specific values */
	cfg->tx_l0_tx_band =
		cfg->tx_l1_tx_band =
		cfg->tx_l2_tx_band =
		cfg->tx_l3_tx_band = pd.tx_band_sel + 4;

	if (bclk > HDMI_DIG_FREQ_BIT_CLK_THRESHOLD)
		cfg->com_svs_mode_clk_sel = 1;
	else
		cfg->com_svs_mode_clk_sel = 2;

	cfg->com_hsclk_sel = (0x20 | pd.hsclk_divsel);
	cfg->com_pll_cctrl_mode0 = cctrl;
	cfg->com_pll_rctrl_mode0 = rctrl;
	cfg->com_cp_ctrl_mode0 = cpctrl;
	cfg->com_dec_start_mode0 = dec_start;
	cfg->com_div_frac_start1_mode0 = (frac_start & 0xff);
	cfg->com_div_frac_start2_mode0 = ((frac_start & 0xff00) >> 8);
	cfg->com_div_frac_start3_mode0 = ((frac_start & 0xf0000) >> 16);
	cfg->com_integloop_gain0_mode0 = (integloop_gain & 0xff);
	cfg->com_integloop_gain1_mode0 = ((integloop_gain & 0xf00) >> 8);
	cfg->com_lock_cmp1_mode0 = (pll_cmp & 0xff);
	cfg->com_lock_cmp2_mode0 = ((pll_cmp & 0xff00) >> 8);
	cfg->com_lock_cmp3_mode0 = ((pll_cmp & 0x30000) >> 16);
	cfg->com_lock_cmp_en = 0x0;
	cfg->com_core_clk_en = 0x2c;
	cfg->com_coreclk_div = HDMI_CORECLK_DIV;
	cfg->phy_mode = (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD) ? 0x10 : 0x0;
	cfg->com_vco_tune_ctrl = 0x0;

	cfg->tx_l0_lane_mode = 0x43;
	cfg->tx_l2_lane_mode = 0x43;

	if (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD) {
		cfg->tx_l0_tx_drv_lvl = 0x25;
		cfg->tx_l0_tx_emp_post1_lvl = 0x23;
		cfg->tx_l1_tx_drv_lvl = 0x25;
		cfg->tx_l1_tx_emp_post1_lvl = 0x23;
		cfg->tx_l2_tx_drv_lvl = 0x25;
		cfg->tx_l2_tx_emp_post1_lvl = 0x23;
		cfg->tx_l3_tx_drv_lvl = 0x22;
		cfg->tx_l3_tx_emp_post1_lvl = 0x27;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0D;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0D;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0D;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x00;
	} else if (bclk > HDMI_MID_FREQ_BIT_CLK_THRESHOLD) {
		cfg->tx_l0_tx_drv_lvl = 0x25;
		cfg->tx_l0_tx_emp_post1_lvl = 0x23;
		cfg->tx_l1_tx_drv_lvl = 0x25;
		cfg->tx_l1_tx_emp_post1_lvl = 0x23;
		cfg->tx_l2_tx_drv_lvl = 0x25;
		cfg->tx_l2_tx_emp_post1_lvl = 0x23;
		cfg->tx_l3_tx_drv_lvl = 0x25;
		cfg->tx_l3_tx_emp_post1_lvl = 0x23;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0D;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0D;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0D;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x00;
	} else {
		cfg->tx_l0_tx_drv_lvl = 0x20;
		cfg->tx_l0_tx_emp_post1_lvl = 0x20;
		cfg->tx_l1_tx_drv_lvl = 0x20;
		cfg->tx_l1_tx_emp_post1_lvl = 0x20;
		cfg->tx_l2_tx_drv_lvl = 0x20;
		cfg->tx_l2_tx_emp_post1_lvl = 0x20;
		cfg->tx_l3_tx_drv_lvl = 0x20;
		cfg->tx_l3_tx_emp_post1_lvl = 0x20;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0E;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0E;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0E;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x0E;
	}

	DBG("tx_l0_tx_band = 0x%x", cfg->tx_l0_tx_band);
	DBG("tx_l1_tx_band = 0x%x", cfg->tx_l1_tx_band);
	DBG("tx_l2_tx_band = 0x%x", cfg->tx_l2_tx_band);
	DBG("tx_l3_tx_band = 0x%x", cfg->tx_l3_tx_band);
	DBG("com_svs_mode_clk_sel = 0x%x",cfg->com_svs_mode_clk_sel);
	DBG("com_hsclk_sel = 0x%x", cfg->com_hsclk_sel);
	DBG("com_lock_cmp_en = 0x%x", cfg->com_lock_cmp_en);
	DBG("com_pll_cctrl_mode0 = 0x%x", cfg->com_pll_cctrl_mode0);
	DBG("com_pll_rctrl_mode0 = 0x%x", cfg->com_pll_rctrl_mode0);
	DBG("com_cp_ctrl_mode0 = 0x%x", cfg->com_cp_ctrl_mode0);
	DBG("com_dec_start_mode0 = 0x%x", cfg->com_dec_start_mode0);
	DBG("com_div_frac_start1_mode0 = 0x%x", cfg->com_div_frac_start1_mode0);
	DBG("com_div_frac_start2_mode0 = 0x%x", cfg->com_div_frac_start2_mode0);
	DBG("com_div_frac_start3_mode0 = 0x%x", cfg->com_div_frac_start3_mode0);
	DBG("com_integloop_gain0_mode0 = 0x%x", cfg->com_integloop_gain0_mode0);
	DBG("com_integloop_gain1_mode0 = 0x%x", cfg->com_integloop_gain1_mode0);
	DBG("com_lock_cmp1_mode0 = 0x%x", cfg->com_lock_cmp1_mode0);
	DBG("com_lock_cmp2_mode0 = 0x%x", cfg->com_lock_cmp2_mode0);
	DBG("com_lock_cmp3_mode0 = 0x%x", cfg->com_lock_cmp3_mode0);
	DBG("com_core_clk_en = 0x%x", cfg->com_core_clk_en);
	DBG("com_coreclk_div = 0x%x", cfg->com_coreclk_div);
	DBG("phy_mode = 0x%x", cfg->phy_mode);

	DBG("tx_l0_lane_mode = 0x%x", cfg->tx_l0_lane_mode);
	DBG("tx_l2_lane_mode = 0x%x", cfg->tx_l2_lane_mode);
	DBG("l0_tx_drv_lvl = 0x%x", cfg->tx_l0_tx_drv_lvl);
	DBG("l0_tx_emp_post1_lvl = 0x%x", cfg->tx_l0_tx_emp_post1_lvl);
	DBG("l1_tx_drv_lvl = 0x%x", cfg->tx_l1_tx_drv_lvl);
	DBG("l1_tx_emp_post1_lvl = 0x%x", cfg->tx_l1_tx_emp_post1_lvl);
	DBG("l2_tx_drv_lvl = 0x%x", cfg->tx_l2_tx_drv_lvl);
	DBG("l2_tx_emp_post1_lvl = 0x%x", cfg->tx_l2_tx_emp_post1_lvl);
	DBG("l3_tx_drv_lvl = 0x%x", cfg->tx_l3_tx_drv_lvl);
	DBG("l3_tx_emp_post1_lvl = 0x%x", cfg->tx_l3_tx_emp_post1_lvl);

	DBG("l0_vmode_ctrl1 = 0x%x", cfg->tx_l0_vmode_ctrl1);
	DBG("l0_vmode_ctrl2 = 0x%x", cfg->tx_l0_vmode_ctrl2);
	DBG("l1_vmode_ctrl1 = 0x%x", cfg->tx_l1_vmode_ctrl1);
	DBG("l1_vmode_ctrl2 = 0x%x", cfg->tx_l1_vmode_ctrl2);
	DBG("l2_vmode_ctrl1 = 0x%x", cfg->tx_l2_vmode_ctrl1);
	DBG("l2_vmode_ctrl2 = 0x%x", cfg->tx_l2_vmode_ctrl2);
	DBG("l3_vmode_ctrl1 = 0x%x", cfg->tx_l3_vmode_ctrl1);
	DBG("l3_vmode_ctrl2 = 0x%x", cfg->tx_l3_vmode_ctrl2);

	return 0;
}

/* since we have weird base registers, let's just pass the iomem address */
static inline void hdmi_pll_write(void __iomem *reg, u32 data)
{
	msm_writel(data, reg);
}

static inline u32 hdmi_pll_read(void __iomem *reg)
{
	return msm_readl(reg);
}

static int hdmi_8996_pll_set_clk_rate(struct clk_hw *hw, unsigned long rate,
				      unsigned long parent_rate)
{
	struct hdmi_pll *pll = hw_clk_to_pll(hw);
	struct hdmi_pll_8996 *pll_8996 = to_hdmi_pll_8996(pll);
	struct hdmi_phy *phy = pll_8996_get_phy(pll_8996);
	struct hdmi_8996_phy_pll_reg_cfg cfg = { 0 };
	int ret;

	ret = pll_calculate(rate, &cfg);
	if (ret) {
		DRM_ERROR("PLL calculation failed\n");
		return ret;
	}

	/* Initially shut down PHY */
	DBG("Disabling PHY");
	hdmi_phy_write(phy, HDMI_PHY_PD_CTL, 0x0);
	udelay(500);

	/* Power up sequence */
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_BG_CTRL, 0x04);

	hdmi_phy_write(phy, HDMI_PHY_PD_CTL, 0x1);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_RESETSM_CNTRL, 0x20);
	hdmi_phy_write(phy, HDMI_PHY_TX0_TX1_LANE_CTL, 0x0F);
	hdmi_phy_write(phy, HDMI_PHY_TX2_TX3_LANE_CTL, 0x0F);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
				     QSERDES_TX_L0_CLKBUF_ENABLE, 0x03);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
				     QSERDES_TX_L0_CLKBUF_ENABLE, 0x03);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
				     QSERDES_TX_L0_CLKBUF_ENABLE, 0x03);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
				     QSERDES_TX_L0_CLKBUF_ENABLE, 0x03);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
		     QSERDES_TX_L0_LANE_MODE, cfg.tx_l0_lane_mode);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
		     QSERDES_TX_L0_LANE_MODE, cfg.tx_l2_lane_mode);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
		     QSERDES_TX_L0_TX_BAND, cfg.tx_l0_tx_band);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
		     QSERDES_TX_L0_TX_BAND, cfg.tx_l1_tx_band);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
		     QSERDES_TX_L0_TX_BAND, cfg.tx_l2_tx_band);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
		     QSERDES_TX_L0_TX_BAND, cfg.tx_l3_tx_band);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
			       QSERDES_TX_L0_RESET_TSYNC_EN, 0x03);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
			       QSERDES_TX_L0_RESET_TSYNC_EN, 0x03);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
			       QSERDES_TX_L0_RESET_TSYNC_EN, 0x03);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
			       QSERDES_TX_L0_RESET_TSYNC_EN, 0x03);

	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_SYSCLK_BUF_ENABLE, 0x1E);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x07);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_SYSCLK_EN_SEL, 0x37);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_SYS_CLK_CTRL, 0x02);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_CLK_ENABLE1, 0x0E);

	/* Bypass VCO calibration */
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_SVS_MODE_CLK_SEL,
					cfg.com_svs_mode_clk_sel);

	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_BG_TRIM, 0x0F);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_PLL_IVCO, 0x0F);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_VCO_TUNE_CTRL,
			cfg.com_vco_tune_ctrl);

	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_BG_CTRL, 0x06);

	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_CLK_SELECT, 0x30);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_HSCLK_SEL,
		       cfg.com_hsclk_sel);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_LOCK_CMP_EN,
		       cfg.com_lock_cmp_en);

	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_PLL_CCTRL_MODE0,
		       cfg.com_pll_cctrl_mode0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_PLL_RCTRL_MODE0,
		       cfg.com_pll_rctrl_mode0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_CP_CTRL_MODE0,
		       cfg.com_cp_ctrl_mode0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_DEC_START_MODE0,
		       cfg.com_dec_start_mode0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_DIV_FRAC_START1_MODE0,
		       cfg.com_div_frac_start1_mode0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_DIV_FRAC_START2_MODE0,
		       cfg.com_div_frac_start2_mode0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_DIV_FRAC_START3_MODE0,
		       cfg.com_div_frac_start3_mode0);

	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_INTEGLOOP_GAIN0_MODE0,
			cfg.com_integloop_gain0_mode0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_INTEGLOOP_GAIN1_MODE0,
			cfg.com_integloop_gain1_mode0);

	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_LOCK_CMP1_MODE0,
			cfg.com_lock_cmp1_mode0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_LOCK_CMP2_MODE0,
			cfg.com_lock_cmp2_mode0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_LOCK_CMP3_MODE0,
			cfg.com_lock_cmp3_mode0);

	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_VCO_TUNE_MAP, 0x00);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_CORE_CLK_EN,
		       cfg.com_core_clk_en);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_CORECLK_DIV,
		       cfg.com_coreclk_div);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_CMN_CONFIG, 0x02);

	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_RESCODE_DIV_NUM, 0x15);

	/* TX lanes setup (TX 0/1/2/3) */
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
		       QSERDES_TX_L0_TX_DRV_LVL,
		       cfg.tx_l0_tx_drv_lvl);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
		       QSERDES_TX_L0_TX_EMP_POST1_LVL,
		       cfg.tx_l0_tx_emp_post1_lvl);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
		       QSERDES_TX_L0_TX_DRV_LVL,
		       cfg.tx_l1_tx_drv_lvl);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
		       QSERDES_TX_L0_TX_EMP_POST1_LVL,
		       cfg.tx_l1_tx_emp_post1_lvl);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
		       QSERDES_TX_L0_TX_DRV_LVL,
		       cfg.tx_l2_tx_drv_lvl);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
		       QSERDES_TX_L0_TX_EMP_POST1_LVL,
		       cfg.tx_l2_tx_emp_post1_lvl);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
		       QSERDES_TX_L0_TX_DRV_LVL,
		       cfg.tx_l3_tx_drv_lvl);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
		       QSERDES_TX_L0_TX_EMP_POST1_LVL,
		       cfg.tx_l3_tx_emp_post1_lvl);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
		       QSERDES_TX_L0_VMODE_CTRL1,
		       cfg.tx_l0_vmode_ctrl1);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
		       QSERDES_TX_L0_VMODE_CTRL2,
		       cfg.tx_l0_vmode_ctrl2);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
		       QSERDES_TX_L0_VMODE_CTRL1,
		       cfg.tx_l1_vmode_ctrl1);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
		       QSERDES_TX_L0_VMODE_CTRL2,
		       cfg.tx_l1_vmode_ctrl2);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
		       QSERDES_TX_L0_VMODE_CTRL1,
		       cfg.tx_l2_vmode_ctrl1);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
		       QSERDES_TX_L0_VMODE_CTRL2,
		       cfg.tx_l2_vmode_ctrl2);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
		       QSERDES_TX_L0_VMODE_CTRL1,
		       cfg.tx_l3_vmode_ctrl1);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
		       QSERDES_TX_L0_VMODE_CTRL2,
		       cfg.tx_l3_vmode_ctrl2);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
		       QSERDES_TX_L0_TX_DRV_LVL_OFFSET, 0x00);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
		       QSERDES_TX_L0_TX_DRV_LVL_OFFSET, 0x00);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
		       QSERDES_TX_L0_TX_DRV_LVL_OFFSET, 0x00);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
		       QSERDES_TX_L0_TX_DRV_LVL_OFFSET, 0x00);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
		       QSERDES_TX_L0_RES_CODE_LANE_OFFSET, 0x00);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
		       QSERDES_TX_L0_RES_CODE_LANE_OFFSET, 0x00);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
		       QSERDES_TX_L0_RES_CODE_LANE_OFFSET, 0x00);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
		       QSERDES_TX_L0_RES_CODE_LANE_OFFSET, 0x00);

	hdmi_phy_write(phy, HDMI_PHY_MODE, cfg.phy_mode);
	hdmi_phy_write(phy, HDMI_PHY_PD_CTL, 0x1F);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
			QSERDES_TX_L0_TRAN_DRVR_EMP_EN, 0x03);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
			QSERDES_TX_L0_TRAN_DRVR_EMP_EN, 0x03);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
			QSERDES_TX_L0_TRAN_DRVR_EMP_EN, 0x03);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
			QSERDES_TX_L0_TRAN_DRVR_EMP_EN, 0x03);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
			QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN, 0x40);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
			QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN, 0x40);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
			QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN, 0x40);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
			QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN, 0x40);

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
		       QSERDES_TX_L0_HP_PD_ENABLES, 0x0C);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
		       QSERDES_TX_L0_HP_PD_ENABLES, 0x0C);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
		       QSERDES_TX_L0_HP_PD_ENABLES, 0x0C);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
		       QSERDES_TX_L0_HP_PD_ENABLES, 0x03);

	/*
	 * Ensure that vco configuration gets flushed to hardware before
	 * enabling the PLL
	 */
	wmb();

	return 0;
}

static int hdmi_8996_phy_ready_status(struct hdmi_phy *phy)
{
	u32 nb_tries = HDMI_PLL_POLL_MAX_READS;
	u32 timeout_ms = HDMI_PLL_POLL_TIMEOUT_MS;
	u32 status;
	int phy_ready = 0;

	DBG("Waiting for PHY ready");

	while (nb_tries--) {
		status = hdmi_phy_read(phy, HDMI_PHY_STATUS);
		phy_ready = status & BIT(0);

		if (phy_ready)
			break;

		mdelay(timeout_ms);
	}

	DBG("PHY is %sready", phy_ready ? "" : "*not* ");

	return phy_ready;
}

static int hdmi_8996_pll_lock_status(struct hdmi_pll *pll)
{
	struct hdmi_pll_8996 *pll_8996 = to_hdmi_pll_8996(pll);
	u32 nb_tries = HDMI_PLL_POLL_MAX_READS;
	u32 timeout_ms = HDMI_PLL_POLL_TIMEOUT_MS;
	u32 status;
	int pll_locked = 0;

	DBG("Waiting for PLL lock");

	while (nb_tries--) {
		status = hdmi_pll_read(pll_8996->mmio + QSERDES_COM_C_READY_STATUS);
		pll_locked = status & BIT(0);

		if (pll_locked)
			break;

		mdelay(timeout_ms);
	}

	DBG("HDMI PLL is %slocked", pll_locked ? "" : "*not* ");

	return pll_locked;
}

static int hdmi_8996_pll_prepare(struct clk_hw *hw)
{
	int rc = 0;
	struct hdmi_pll *pll = hw_clk_to_pll(hw);
	struct hdmi_pll_8996 *pll_8996 = to_hdmi_pll_8996(pll);
	struct hdmi_phy *phy = pll_8996_get_phy(pll_8996);

	hdmi_phy_write(phy, HDMI_PHY_CFG, 0x1);
	udelay(100);

	hdmi_phy_write(phy, HDMI_PHY_CFG, 0x19);
	udelay(100);

	rc = hdmi_8996_pll_lock_status(pll);
	if (!rc)
		return rc;

	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L0_BASE_OFFSET +
		       QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN,
		       0x6F);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L1_BASE_OFFSET +
		       QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN,
		       0x6F);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L2_BASE_OFFSET +
		       QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN,
		       0x6F);
	hdmi_pll_write(pll_8996->mmio + HDMI_TX_L3_BASE_OFFSET +
		       QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN,
		       0x6F);

	/* Disable SSC */
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_SSC_PER1, 0x0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_SSC_PER2, 0x0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_SSC_STEP_SIZE1, 0x0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_SSC_STEP_SIZE2, 0x0);
	hdmi_pll_write(pll_8996->mmio + QSERDES_COM_SSC_EN_CENTER, 0x2);

	rc = hdmi_8996_phy_ready_status(phy);
	if (!rc)
		return rc;

	/* Restart the retiming buffer */
	hdmi_phy_write(phy, HDMI_PHY_CFG, 0x18);
	udelay(1);
	hdmi_phy_write(phy, HDMI_PHY_CFG, 0x19);

	return 0;
}

long hdmi_8996_pll_round_rate(struct clk_hw *hw,
		unsigned long rate, unsigned long *parent_rate)
{
	DBG("rrate=%ld\n", rate);

	return rate;
}

static unsigned long hdmi_8996_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct hdmi_pll *pll = hw_clk_to_pll(hw);
	struct hdmi_pll_8996 *pll_8996 = to_hdmi_pll_8996(pll);
	u64 fdata;
	u32 cmp1, cmp2, cmp3, pll_cmp;

	cmp1 = hdmi_pll_read(pll_8996->mmio + QSERDES_COM_LOCK_CMP1_MODE0);
	cmp2 = hdmi_pll_read(pll_8996->mmio + QSERDES_COM_LOCK_CMP2_MODE0);
	cmp3 = hdmi_pll_read(pll_8996->mmio + QSERDES_COM_LOCK_CMP3_MODE0);

	pll_cmp = cmp1 | (cmp2 << 8) | (cmp3 << 16);

	fdata = pll_cmp_to_fdata(pll_cmp + 1);

	do_div(fdata, 10);

	return fdata;
}

static void hdmi_8996_pll_unprepare(struct clk_hw *hw)
{
}

static int hdmi_8996_pll_is_enabled(struct clk_hw *hw)
{
	struct hdmi_pll *pll = hw_clk_to_pll(hw);

	return hdmi_8996_pll_lock_status(pll);
}

static struct clk_ops hdmi_8996_pll_ops = {
	.set_rate = hdmi_8996_pll_set_clk_rate,
	.round_rate = hdmi_8996_pll_round_rate,
	.recalc_rate = hdmi_8996_pll_recalc_rate,
	.prepare = hdmi_8996_pll_prepare,
	.unprepare = hdmi_8996_pll_unprepare,
	.is_enabled = hdmi_8996_pll_is_enabled,
};

static const char *hdmi_pll_parents[] = {
	"xo",
};

static struct clk_init_data pll_init = {
	.name = "hdmipll",
	.ops = &hdmi_8996_pll_ops,
	.parent_names = hdmi_pll_parents,
	.num_parents = ARRAY_SIZE(hdmi_pll_parents),
};

struct hdmi_pll *hdmi_pll_8996_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hdmi_pll_8996 *pll_8996;
	struct hdmi_pll *pll;
	struct clk *clk;

	pll_8996 = devm_kzalloc(dev, sizeof(*pll_8996), GFP_KERNEL);
	if (!pll_8996)
		return ERR_PTR(-ENOMEM);

	pll_8996->pdev = pdev;

	pll = &pll_8996->base;

	pll_8996->mmio = msm_ioremap(pdev, "hdmi_pll", "HDMI_PLL");
	if (IS_ERR(pll_8996->mmio)) {
		dev_err(dev, "failed to map pll base\n");
		return ERR_PTR(-ENOMEM);
	}

	pll->clk_hw.init = &pll_init;

	clk = devm_clk_register(dev, &pll->clk_hw);
	if (IS_ERR(clk)) {
		dev_err(dev, "failed to register pll clock\n");
		return ERR_PTR(-EINVAL);
	}

	return pll;
}

static const char * const hdmi_phy_8996_reg_names[] = {
	"vddio",
	"vcca",
};

static const char * const hdmi_phy_8996_clk_names[]= {
	"mmagic_iface_clk",
	"iface_clk",
	"ref_clk",
};

const struct hdmi_phy_cfg hdmi_phy_8996_cfg = {
	.type = MSM_HDMI_PHY_8996,
	.reg_names = hdmi_phy_8996_reg_names,
	.num_regs = ARRAY_SIZE(hdmi_phy_8996_reg_names),
	.clk_names = hdmi_phy_8996_clk_names,
	.num_clks = ARRAY_SIZE(hdmi_phy_8996_clk_names),
};
