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
#define DEBUG
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/qcom_scm.h>
#include <linux/slab.h>

#include "hfi/hfi_cmds.h"
#include "hfi/hfi_msgs.h"
#include "mem.h"
#include "venus_hfi.h"
#include "venus_hfi_io.h"
#include "core.h"
#include "../../vmem.h"

#define HFI_MASK_QHDR_TX_TYPE		0xff000000
#define HFI_MASK_QHDR_RX_TYPE		0x00ff0000
#define HFI_MASK_QHDR_PRI_TYPE		0x0000ff00
#define HFI_MASK_QHDR_ID_TYPE		0x000000ff

#define HFI_HOST_TO_CTRL_CMD_Q		0
#define HFI_CTRL_TO_HOST_MSG_Q		1
#define HFI_CTRL_TO_HOST_DBG_Q		2
#define HFI_MASK_QHDR_STATUS		0x000000ff

#define IFACEQ_NUM			3
#define IFACEQ_CMD_IDX			0
#define IFACEQ_MSG_IDX			1
#define IFACEQ_DBG_IDX			2
#define IFACEQ_MAX_BUF_COUNT		50
#define IFACEQ_MAX_PARALLEL_CLNTS	16
#define IFACEQ_DFLT_QHDR		0x01010000

#define POLL_INTERVAL_US		50

#define IFACEQ_MAX_PKT_SIZE		1024
#define IFACEQ_MED_PKT_SIZE		768
#define IFACEQ_MIN_PKT_SIZE		8
#define IFACEQ_VAR_SMALL_PKT_SIZE	100
#define IFACEQ_VAR_LARGE_PKT_SIZE	512
#define IFACEQ_VAR_HUGE_PKT_SIZE	(1024 * 12)

enum tzbsp_video_state {
	TZBSP_VIDEO_STATE_SUSPEND = 0,
	TZBSP_VIDEO_STATE_RESUME,
	TZBSP_VIDEO_STATE_RESTORE_THRESHOLD,
};

struct hfi_queue_table_header {
	u32 version;
	u32 size;
	u32 qhdr0_offset;
	u32 qhdr_size;
	u32 num_q;
	u32 num_active_q;
};

struct hfi_queue_header {
	u32 status;
	u32 start_addr;
	u32 type;
	u32 q_size;
	u32 pkt_size;
	u32 pkt_drop_cnt;
	u32 rx_wm;
	u32 tx_wm;
	u32 rx_req;
	u32 tx_req;
	u32 rx_irq_status;
	u32 tx_irq_status;
	u32 read_idx;
	u32 write_idx;
};

#define IFACEQ_TABLE_SIZE	\
	(sizeof(struct hfi_queue_table_header) +	\
	 sizeof(struct hfi_queue_header) * IFACEQ_NUM)

#define IFACEQ_QUEUE_SIZE	(IFACEQ_MAX_PKT_SIZE *	\
	IFACEQ_MAX_BUF_COUNT * IFACEQ_MAX_PARALLEL_CLNTS)

#define IFACEQ_GET_QHDR_START_ADDR(ptr, i)	\
	(void *)(((ptr) + sizeof(struct hfi_queue_table_header)) +	\
		((i) * sizeof(struct hfi_queue_header)))

#define QDSS_SIZE		SZ_4K
#define SFR_SIZE		SZ_4K
#define QUEUE_SIZE		\
	(IFACEQ_TABLE_SIZE + (IFACEQ_QUEUE_SIZE * IFACEQ_NUM))

#define ALIGNED_QDSS_SIZE	ALIGN(QDSS_SIZE, SZ_4K)
#define ALIGNED_SFR_SIZE	ALIGN(SFR_SIZE, SZ_4K)
#define ALIGNED_QUEUE_SIZE	ALIGN(QUEUE_SIZE, SZ_4K)
#define SHARED_QSIZE		ALIGN(ALIGNED_SFR_SIZE + ALIGNED_QUEUE_SIZE + \
				      ALIGNED_QDSS_SIZE, SZ_1M)

struct mem_desc {
	u32 da;		/* device address */
	void *kva;	/* kernel virtual address */
	u32 size;
	struct smem *smem;
};

struct iface_queue {
	struct hfi_queue_header *qhdr;
	struct mem_desc qmem;
};

enum venus_state {
	VENUS_STATE_DEINIT = 1,
	VENUS_STATE_INIT,
};

struct venus_hfi_device {
	void __iomem *base;
	u32 irq;
	u32 intr_status;
	u32 last_packet_type;
	bool power_enabled;
	bool suspended;
	struct mutex lock;
	struct mem_desc ifaceq_table;
	struct mem_desc sfr;
	struct iface_queue queues[IFACEQ_NUM];
	struct vidc_resources *res;
	struct device *dev;
	enum venus_state state;
	const struct hfi_packetization_ops *pkt_ops;
	enum hfi_packetization_type packetization_type;
	u8 pkt_buf[IFACEQ_VAR_HUGE_PKT_SIZE];
	u8 dbg_buf[IFACEQ_VAR_HUGE_PKT_SIZE];
};

static bool venus_pkt_debug = 0;
static int venus_fw_debug = 0x3f;//0x18;
static bool venus_sys_idle_indicator = 0;
static bool venus_fw_low_power_mode = true;
static int venus_hw_rsp_timeout = 1000;
static bool venus_fw_coverage = 0;

static DECLARE_COMPLETION(release_resources_done);
static DECLARE_COMPLETION(pc_prep_done);

static inline void venus_set_state(struct venus_hfi_device *hdev,
				   enum venus_state state)
{
	mutex_lock(&hdev->lock);
	hdev->state = state;
	mutex_unlock(&hdev->lock);
}

static inline bool venus_is_valid_state(struct venus_hfi_device *hdev)
{
	return hdev->state != VENUS_STATE_DEINIT;
}

static void venus_dump_packet(struct venus_hfi_device *hdev, u8 *packet)
{
	struct device *dev = hdev->dev;
	u32 c = 0, pkt_size = *(u32 *)packet;
	const int row_size = 32;
	/*
	 * row must contain enough for 0xdeadbaad * 8 to be converted into
	 * "de ad ba ab " * 8 + '\0'
	 */
	char row[3 * row_size];

	if (!venus_pkt_debug)
		return;

	for (c = 0; c * row_size < pkt_size; ++c) {
		int bytes_to_read = ((c + 1) * row_size > pkt_size) ?
			pkt_size % row_size : row_size;
		hex_dump_to_buffer(packet + c * row_size, bytes_to_read,
				   row_size, 4, row, sizeof(row), false);
		dev_dbg(dev, "%s\n", row);
	}
}

static int venus_write_queue(struct venus_hfi_device *hdev,
			     struct iface_queue *queue,
			     void *packet, u32 *rx_req)
{
	struct hfi_queue_header *qhdr;
	u32 dwords, new_wr_idx;
	u32 empty_space, rd_idx, wr_idx, qsize;
	u32 *wr_ptr;

	if (!queue->qmem.kva)
		return -EINVAL;

	qhdr = queue->qhdr;
	if (!qhdr)
		return -EINVAL;

	venus_dump_packet(hdev, packet);

	dwords = (*(u32 *)packet) >> 2;
	if (!dwords)
		return -EINVAL;

	rd_idx = qhdr->read_idx;
	wr_idx = qhdr->write_idx;
	qsize = qhdr->q_size;
	/* ensure rd/wr indices's are read from memory */
	rmb();

	if (wr_idx >= rd_idx)
		empty_space = qsize - (wr_idx - rd_idx);
	else
		empty_space = rd_idx - wr_idx;

	if (empty_space <= dwords) {
		qhdr->tx_req = 1;
		/* ensure tx_req is updated in memory */
		wmb();
		return -ENOSPC;
	}

	qhdr->tx_req = 0;
	/* ensure tx_req is updated in memory */
	wmb();

	new_wr_idx = wr_idx + dwords;
	wr_ptr = (u32 *)(queue->qmem.kva + (wr_idx << 2));
	if (new_wr_idx < qsize) {
		memcpy(wr_ptr, packet, dwords << 2);
	} else {
		size_t len;

		new_wr_idx -= qsize;
		len = (dwords - new_wr_idx) << 2;
		memcpy(wr_ptr, packet, len);
		memcpy(queue->qmem.kva, packet + len, new_wr_idx << 2);
	}

	/* make sure packet is written before updating the write index */
	wmb();

	qhdr->write_idx = new_wr_idx;
	*rx_req = qhdr->rx_req ? 1 : 0;

	/* make sure write index is updated before an interupt is raised */
	mb();

	return 0;
}

static int venus_read_queue(struct venus_hfi_device *hdev,
			    struct iface_queue *queue, void *pkt, u32 *tx_req)
{
	struct hfi_queue_header *qhdr;
	u32 dwords, new_rd_idx;
	u32 rd_idx, wr_idx, type, qsize;
	u32 *rd_ptr;
	u32 recv_request = 0;
	int ret = 0;

	if (!queue->qmem.kva)
		return -EINVAL;

	qhdr = queue->qhdr;
	if (!qhdr)
		return -EINVAL;

	type = qhdr->type;
	rd_idx = qhdr->read_idx;
	wr_idx = qhdr->write_idx;
	qsize = qhdr->q_size;

	/* make sure data is valid before using it */
	rmb();

	/*
	 * Do not set receive request for debug queue, if set, Venus generates
	 * interrupt for debug messages even when there is no response message
	 * available. In general debug queue will not become full as it is being
	 * emptied out for every interrupt from Venus. Venus will anyway
	 * generates interrupt if it is full.
	 */
	if (type & HFI_CTRL_TO_HOST_MSG_Q)
		recv_request = 1;

	if (rd_idx == wr_idx) {
		qhdr->rx_req = recv_request;
		*tx_req = 0;
		/* update rx_req field in memory */
		wmb();
		return -ENODATA;
	}

	rd_ptr = (u32 *)(queue->qmem.kva + (rd_idx << 2));
	dwords = *rd_ptr >> 2;
	if (!dwords)
		return -EINVAL;

	new_rd_idx = rd_idx + dwords;
	if (((dwords << 2) <= IFACEQ_VAR_HUGE_PKT_SIZE) && rd_idx <= qsize) {
		if (new_rd_idx < qsize) {
			memcpy(pkt, rd_ptr, dwords << 2);
		} else {
			size_t len;

			new_rd_idx -= qsize;
			len = (dwords - new_rd_idx) << 2;
			memcpy(pkt, rd_ptr, len);
			memcpy(pkt + len, queue->qmem.kva, new_rd_idx << 2);
		}
	} else {
		/* bad packet received, dropping */
		new_rd_idx = qhdr->write_idx;
		ret = -EBADMSG;
	}

	/* ensure the packet is read before updating read index */
	rmb();

	qhdr->read_idx = new_rd_idx;
	/* ensure updating read index */
	wmb();

	rd_idx = qhdr->read_idx;
	wr_idx = qhdr->write_idx;
	/* ensure rd/wr indices are read from memory */
	rmb();

	if (rd_idx != wr_idx)
		qhdr->rx_req = 0;
	else
		qhdr->rx_req = recv_request;

	*tx_req = qhdr->tx_req ? 1 : 0;

	mb();

	venus_dump_packet(hdev, pkt);

	return ret;
}

static int venus_alloc(struct venus_hfi_device *hdev, struct mem_desc *desc,
		       u32 size, u32 align)
{
	struct smem *smem;

	smem = smem_alloc(hdev->dev, size, align, 1);
	if (IS_ERR(smem))
		return PTR_ERR(smem);

	desc->size = smem->size;
	desc->smem = smem;
	desc->kva = smem->kvaddr;
	desc->da = smem->da;

	return 0;
}

static void venus_free(struct smem *mem)
{
	smem_free(mem);
}

static void venus_writel(struct venus_hfi_device *hdev, u32 reg, u32 value)
{
	writel(value, hdev->base + reg);
}

static u32 venus_readl(struct venus_hfi_device *hdev, u32 reg)
{
	return readl(hdev->base + reg);
}

static void venus_set_registers(struct venus_hfi_device *hdev)
{
	const struct reg_val *tbl = hdev->res->reg_tbl;
	unsigned int count = hdev->res->reg_tbl_size;
	int i;

	for (i = 0; i < count; i++)
		venus_writel(hdev, tbl[i].reg, tbl[i].value);
}

static void venus_soft_int(struct venus_hfi_device *hdev)
{
	venus_writel(hdev, VIDC_CPU_IC_SOFTINT,
		     BIT(VIDC_CPU_IC_SOFTINT_H2A_SHFT));
}

static int venus_iface_cmdq_write_nolock(struct venus_hfi_device *hdev,
					 void *pkt)
{
	struct device *dev = hdev->dev;
	struct hfi_pkt_hdr *cmd_packet;
	struct iface_queue *queue;
	u32 rx_req;
	int ret;

	WARN(!mutex_is_locked(&hdev->lock),
	     "cmd queue write lock must be acquired");

	if (!venus_is_valid_state(hdev)) {
		dev_err(dev, "%s: fw not in init state\n", __func__);
		return -EINVAL;
	}

	cmd_packet = (struct hfi_pkt_hdr *)pkt;
	hdev->last_packet_type = cmd_packet->pkt_type;

	queue = &hdev->queues[IFACEQ_CMD_IDX];

	ret = venus_write_queue(hdev, queue, pkt, &rx_req);
	if (ret) {
		dev_err(dev, "write to iface cmd queue failed (%d)\n", ret);
		return ret;
	}

	if (rx_req)
		venus_soft_int(hdev);

	return 0;
}

static int venus_iface_cmdq_write(struct venus_hfi_device *hdev, void *pkt)
{
	int ret;

	mutex_lock(&hdev->lock);
	ret = venus_iface_cmdq_write_nolock(hdev, pkt);
	mutex_unlock(&hdev->lock);

	return ret;
}

static int venus_hfi_core_set_resource(struct venus_hfi_device *hdev,
				       struct hal_resource_hdr *hdr,
				       u32 res_addr)
{
	struct hfi_sys_set_resource_pkt *pkt;
	u8 packet[IFACEQ_VAR_SMALL_PKT_SIZE];
	int ret;

	pkt = (struct hfi_sys_set_resource_pkt *) packet;

	ret = call_hfi_pkt_op(hdev, sys_set_resource, pkt, hdr, res_addr);
	if (ret)
		return ret;

	ret = venus_iface_cmdq_write(hdev, pkt);
	if (ret)
		return ret;

	return 0;
}

static int
venus_hfi_core_unset_resource(void *device, struct hal_resource_hdr *hdr)
{
	struct hfi_sys_release_resource_pkt pkt;
	struct venus_hfi_device *hdev = device;
	int ret;

	ret = call_hfi_pkt_op(hdev, sys_release_resource, &pkt, hdr);
	if (ret)
		return ret;

	ret = venus_iface_cmdq_write(hdev, &pkt);
	if (ret)
		return ret;

	return 0;
}

static int venus_tzbsp_set_video_state(enum tzbsp_video_state state)
{
	return 0; //qcom_scm_set_video_state(state, 0);
}

void venus_enable_clock_config(struct vidc_core *core)
{
	struct venus_hfi_device *hdev = to_hfi_priv(&(core->hfi));

	venus_writel(hdev, VIDC_WRAPPER_CLOCK_CONFIG, 0);
	venus_writel(hdev, VIDC_WRAPPER_CPU_CLOCK_CONFIG, 0);
}

static int venus_reset_core(struct venus_hfi_device *hdev)
{
	struct device *dev = hdev->dev;
	u32 ctrl_status = 0, count = 0;
	int max_tries = 100, ret = 0;

	dev_err(hdev->dev, "cpu status %x",
		venus_readl(hdev, VIDC_WRAPPER_CPU_STATUS));

	dev_err(hdev->dev, "venus power status %x",
		venus_readl(hdev, VIDC_WRAPPER_POWER_STATUS));

	venus_writel(hdev, VIDC_CTRL_INIT, 0x1);
	venus_writel(hdev, VIDC_WRAPPER_INTR_MASK,
		     VIDC_WRAPPER_INTR_MASK_A2HVCODEC_BMSK);

	while (!ctrl_status && count < max_tries) {
		ctrl_status = venus_readl(hdev, VIDC_CPU_CS_SCIACMDARG0);
		if ((ctrl_status & 0xfe) == 0x4) {
			dev_err(dev, "invalid setting for UC_REGION\n");
			ret = -EINVAL;
			break;
		}

		usleep_range(500, 1000);
		count++;
	}

	if (count >= max_tries)
		ret = -ETIMEDOUT;

	return ret;
}

static u32 venus_hwversion(struct venus_hfi_device *hdev)
{
	struct device *dev = hdev->dev;
	u32 ver = venus_readl(hdev, VIDC_WRAPPER_HW_VERSION);
	u32 major, minor, step;

	major = ver & VIDC_WRAPPER_HW_VERSION_MAJOR_VERSION_MASK;
	major = major >> VIDC_WRAPPER_HW_VERSION_MAJOR_VERSION_SHIFT;
	minor = ver & VIDC_WRAPPER_HW_VERSION_MINOR_VERSION_MASK;
	minor = minor >> VIDC_WRAPPER_HW_VERSION_MINOR_VERSION_SHIFT;
	step = ver & VIDC_WRAPPER_HW_VERSION_STEP_VERSION_MASK;

	dev_info(dev, "venus hw version %x.%x.%x\n", major, minor, step);

	return major;
}

/*
 * Work around for H/W bug, need to reprogram these registers once
 * firmware is out reset
 */
static void venus_set_threshold_regs(struct venus_hfi_device *hdev)
{
	u32 version = venus_readl(hdev, VIDC_WRAPPER_HW_VERSION);
	int ret;

	version &= ~GENMASK(15, 0);
	if (version != (0x3 << 28 | 0x43 << 16)) {
		dev_err(hdev->dev,
			"%s: no need to restore threshold!\n",
			__func__);
		return;
	}

	ret = venus_tzbsp_set_video_state(
			TZBSP_VIDEO_STATE_RESTORE_THRESHOLD);
	if (ret)
		dev_err(hdev->dev,
			"failed to restore threshold values\n");
}


static int venus_run(struct venus_hfi_device *hdev)
{
	struct device *dev = hdev->dev;
	int ret;

	dev_err(hdev->dev, "%s: enter\n", __func__);

	venus_writel(hdev, VIDC_WRAPPER_CLOCK_CONFIG, 0);
	venus_writel(hdev, VIDC_WRAPPER_CPU_CLOCK_CONFIG, 0);

	/*
	 * Re-program all of the registers that get reset as a result of
	 * regulator_disable() and _enable()
	 */
	venus_set_registers(hdev);

	venus_writel(hdev, VIDC_UC_REGION_ADDR, hdev->ifaceq_table.da);
	venus_writel(hdev, VIDC_UC_REGION_SIZE, SHARED_QSIZE);
	venus_writel(hdev, VIDC_CPU_CS_SCIACMDARG2, hdev->ifaceq_table.da);
	venus_writel(hdev, VIDC_CPU_CS_SCIACMDARG1, 0x01);
	if (hdev->sfr.da)
		venus_writel(hdev, VIDC_SFR_ADDR, hdev->sfr.da);

	dev_err(hdev->dev, "%s: hdev->ifaceq_table.da %x", __func__,
		hdev->ifaceq_table.da);

	ret = venus_reset_core(hdev);
	if (ret) {
		dev_err(dev, "failed to reset venus core\n");
		return ret;
	}

	venus_hwversion(hdev);

	venus_set_threshold_regs(hdev);

	return 0;
}

static int venus_halt_axi(struct venus_hfi_device *hdev)
{
	struct device *dev = hdev->dev;
	void __iomem *base = hdev->base;
	u32 val;
	int ret;

	/* Halt AXI and AXI IMEM VBIF Access */
	val = venus_readl(hdev, VENUS_VBIF_AXI_HALT_CTRL0);
	val |= VENUS_VBIF_AXI_HALT_CTRL0_HALT_REQ;
	venus_writel(hdev, VENUS_VBIF_AXI_HALT_CTRL0, val);

	/* Request for AXI bus port halt */
	ret = readl_poll_timeout(base + VENUS_VBIF_AXI_HALT_CTRL1, val,
				 val & VENUS_VBIF_AXI_HALT_CTRL1_HALT_ACK,
				 POLL_INTERVAL_US,
				 VENUS_VBIF_AXI_HALT_ACK_TIMEOUT_US);
	if (ret) {
		dev_err(dev, "AXI bus port halt timeout\n");
		return ret;
	}

	return 0;
}

static int venus_power_off(struct venus_hfi_device *hdev)
{
	int ret;

	dev_err(hdev->dev, "%s: %d\n", __func__, __LINE__);

	if (!hdev->power_enabled)
		return 0;

	ret = venus_halt_axi(hdev);
	if (ret)
		return ret;

	ret = venus_tzbsp_set_video_state(TZBSP_VIDEO_STATE_SUSPEND);
	if (ret)
		return ret;

	hdev->power_enabled = false;

	dev_err(hdev->dev, "%s: %d\n", __func__, __LINE__);

	return 0;
}

static int venus_power_on(struct venus_hfi_device *hdev)
{
	int ret;

	dev_err(hdev->dev, "%s: %d\n", __func__, __LINE__);

	if (hdev->power_enabled)
		return 0;

	ret = venus_tzbsp_set_video_state(TZBSP_VIDEO_STATE_RESUME);
	if (ret)
		goto err;

	ret = venus_run(hdev);
	if (ret)
		goto err_suspend;

	hdev->power_enabled = true;

	dev_err(hdev->dev, "%s: %d\n", __func__, __LINE__);

	return 0;

err_suspend:
	venus_tzbsp_set_video_state(TZBSP_VIDEO_STATE_SUSPEND);
err:
	hdev->power_enabled = false;
	dev_err(hdev->dev, "%s: %d\n", __func__, __LINE__);
	return ret;
}

static int venus_iface_msgq_read_nolock(struct venus_hfi_device *hdev,
					void *pkt)
{
	struct iface_queue *queue;
	u32 tx_req;
	int ret;

	if (!venus_is_valid_state(hdev))
		return -EINVAL;

	queue = &hdev->queues[IFACEQ_MSG_IDX];

	ret = venus_read_queue(hdev, queue, pkt, &tx_req);
	if (ret)
		return ret;

	if (tx_req)
		venus_soft_int(hdev);

	return 0;
}

static int venus_iface_msgq_read(struct venus_hfi_device *hdev, void *pkt)
{
	int ret;

	mutex_lock(&hdev->lock);
	ret = venus_iface_msgq_read_nolock(hdev, pkt);
	mutex_unlock(&hdev->lock);

	return ret;
}

static int venus_iface_dbgq_read_nolock(struct venus_hfi_device *hdev,
					void *pkt)
{
	struct iface_queue *queue;
	u32 tx_req;
	int ret;

	ret = venus_is_valid_state(hdev);
	if (!ret)
		return -EINVAL;

	queue = &hdev->queues[IFACEQ_DBG_IDX];

	ret = venus_read_queue(hdev, queue, pkt, &tx_req);
	if (ret)
		return ret;

	if (tx_req)
		venus_soft_int(hdev);

	return 0;
}

static int venus_iface_dbgq_read(struct venus_hfi_device *hdev, void *pkt)
{
	int ret;

	if (!pkt)
		return -EINVAL;

	mutex_lock(&hdev->lock);
	ret = venus_iface_dbgq_read_nolock(hdev, pkt);
	mutex_unlock(&hdev->lock);

	return ret;
}

static void venus_set_qhdr_defaults(struct hfi_queue_header *qhdr)
{
	qhdr->status = 1;
	qhdr->type = IFACEQ_DFLT_QHDR;
	qhdr->q_size = IFACEQ_QUEUE_SIZE / 4;
	qhdr->pkt_size = 0;
	qhdr->rx_wm = 1;
	qhdr->tx_wm = 1;
	qhdr->rx_req = 1;
	qhdr->tx_req = 0;
	qhdr->rx_irq_status = 0;
	qhdr->tx_irq_status = 0;
	qhdr->read_idx = 0;
	qhdr->write_idx = 0;
}

static void venus_interface_queues_release(struct venus_hfi_device *hdev)
{
	mutex_lock(&hdev->lock);

	venus_free(hdev->ifaceq_table.smem);
	venus_free(hdev->sfr.smem);

	memset(hdev->queues, 0, sizeof(hdev->queues));
	memset(&hdev->ifaceq_table, 0, sizeof(hdev->ifaceq_table));
	memset(&hdev->sfr, 0, sizeof(hdev->sfr));

	mutex_unlock(&hdev->lock);
}

static int venus_interface_queues_init(struct venus_hfi_device *hdev)
{
	struct hfi_queue_table_header *tbl_hdr;
	struct iface_queue *queue;
	struct hfi_sfr *sfr;
	struct mem_desc desc = {0};
	unsigned int offset;
	unsigned int i;
	int ret;

	ret = venus_alloc(hdev, &desc, ALIGNED_QUEUE_SIZE, 1);
	if (ret)
		return ret;

	hdev->ifaceq_table.kva = desc.kva;
	hdev->ifaceq_table.da = desc.da;
	hdev->ifaceq_table.size = IFACEQ_TABLE_SIZE;
	hdev->ifaceq_table.smem = desc.smem;
	offset = hdev->ifaceq_table.size;

	dev_info(hdev->dev, "%s: ifaceq_table: kva:%p, da:%08x\n", __func__,
		hdev->ifaceq_table.kva, hdev->ifaceq_table.da);


	for (i = 0; i < IFACEQ_NUM; i++) {
		queue = &hdev->queues[i];
		queue->qmem.da = desc.da + offset;
		queue->qmem.kva = desc.kva + offset;
		queue->qmem.size = IFACEQ_QUEUE_SIZE;
		queue->qmem.smem = NULL;
		offset += queue->qmem.size;
		queue->qhdr =
			IFACEQ_GET_QHDR_START_ADDR(hdev->ifaceq_table.kva, i);

		venus_set_qhdr_defaults(queue->qhdr);

		queue->qhdr->start_addr = queue->qmem.da;

		if (i == IFACEQ_CMD_IDX)
			queue->qhdr->type |= HFI_HOST_TO_CTRL_CMD_Q;
		else if (i == IFACEQ_MSG_IDX)
			queue->qhdr->type |= HFI_CTRL_TO_HOST_MSG_Q;
		else if (i == IFACEQ_DBG_IDX)
			queue->qhdr->type |= HFI_CTRL_TO_HOST_DBG_Q;
	}

	tbl_hdr = hdev->ifaceq_table.kva;
	tbl_hdr->version = 0;
	tbl_hdr->size = IFACEQ_TABLE_SIZE;
	tbl_hdr->qhdr0_offset = sizeof(struct hfi_queue_table_header);
	tbl_hdr->qhdr_size = sizeof(struct hfi_queue_header);
	tbl_hdr->num_q = IFACEQ_NUM;
	tbl_hdr->num_active_q = IFACEQ_NUM;

	/*
	 * Set receive request to zero on debug queue as there is no
	 * need of interrupt from video hardware for debug messages
	 */
	queue = &hdev->queues[IFACEQ_DBG_IDX];
	queue->qhdr->rx_req = 0;

	ret = venus_alloc(hdev, &desc, ALIGNED_SFR_SIZE, 1);
	if (ret) {
		hdev->sfr.da = 0;
	} else {
		hdev->sfr.da = desc.da;
		hdev->sfr.kva = desc.kva;
		hdev->sfr.size = ALIGNED_SFR_SIZE;
		hdev->sfr.smem = desc.smem;
		sfr = hdev->sfr.kva;
		sfr->buf_size = ALIGNED_SFR_SIZE;
	}

	/* ensure table and queue header structs are settled in memory */
	wmb();

	return 0;
}

static int venus_sys_set_debug(struct venus_hfi_device *hdev, u32 debug)
{
	struct hfi_sys_set_property_pkt *pkt;
	u8 packet[IFACEQ_VAR_SMALL_PKT_SIZE];
	int ret;

	pkt = (struct hfi_sys_set_property_pkt *) &packet;

	call_hfi_pkt_op(hdev, sys_debug_config, pkt, HFI_DEBUG_MODE_QUEUE,
			debug);

	ret = venus_iface_cmdq_write(hdev, pkt);
	if (ret)
		return ret;

	return 0;
}

static int venus_sys_set_coverage(struct venus_hfi_device *hdev, u32 mode)
{
	struct hfi_sys_set_property_pkt *pkt;
	u8 packet[IFACEQ_VAR_SMALL_PKT_SIZE];
	int ret;

	pkt = (struct hfi_sys_set_property_pkt *) packet;

	call_hfi_pkt_op(hdev, sys_coverage_config, pkt, mode);

	ret = venus_iface_cmdq_write(hdev, pkt);
	if (ret)
		return ret;

	return 0;
}

static int venus_sys_set_idle_message(struct venus_hfi_device *hdev,
				      bool enable)
{
	struct hfi_sys_set_property_pkt *pkt;
	u8 packet[IFACEQ_VAR_SMALL_PKT_SIZE];
	int ret;

	if (!enable)
		return 0;

	pkt = (struct hfi_sys_set_property_pkt *) packet;

	call_hfi_pkt_op(hdev, sys_idle_indicator, pkt, enable);

	ret = venus_iface_cmdq_write(hdev, pkt);
	if (ret)
		return ret;

	return 0;
}

static int venus_sys_set_power_control(struct venus_hfi_device *hdev,
				       bool enable)
{
	struct hfi_sys_set_property_pkt *pkt;
	u8 packet[IFACEQ_VAR_SMALL_PKT_SIZE];
	int ret;

	pkt = (struct hfi_sys_set_property_pkt *) packet;

	call_hfi_pkt_op(hdev, sys_power_control, pkt, enable);

	ret = venus_iface_cmdq_write(hdev, pkt);
	if (ret)
		return ret;

	return 0;
}

static int venus_get_queue_size(struct venus_hfi_device *hdev,
				unsigned int index)
{
	struct hfi_queue_header *qhdr;

	if (index >= IFACEQ_NUM)
		return -EINVAL;

	qhdr = hdev->queues[index].qhdr;
	if (!qhdr)
		return -EINVAL;

	return abs(qhdr->read_idx - qhdr->write_idx);
}

static int venus_sys_set_default_properties(struct venus_hfi_device *hdev)
{
	struct device *dev = hdev->dev;
	int ret;

	ret = venus_sys_set_debug(hdev, venus_fw_debug);
	if (ret)
		dev_warn(dev, "setting fw debug msg ON failed (%d)\n", ret);

	ret = venus_sys_set_idle_message(hdev, venus_sys_idle_indicator);
	if (ret)
		dev_warn(dev, "setting idle response ON failed (%d)\n", ret);

	ret = venus_sys_set_power_control(hdev, venus_fw_low_power_mode);
	if (ret)
		dev_warn(dev, "setting hw power collapse ON failed (%d)\n",
			 ret);

	return ret;
}

static int venus_session_cmd(struct hfi_device_inst *inst, u32 pkt_type)
{
	struct venus_hfi_device *hdev = inst->device;
	struct hfi_session_pkt pkt;
	int ret;

	call_hfi_pkt_op(hdev, session_cmd, &pkt, pkt_type, inst);

	ret = venus_iface_cmdq_write(hdev, &pkt);
	if (ret)
		return ret;

	return 0;
}

static void venus_flush_debug_queue(struct venus_hfi_device *hdev)
{
	void *packet = hdev->dbg_buf;
	struct device *dev = hdev->dev;

	while (!venus_iface_dbgq_read(hdev, packet)) {
		struct hfi_msg_sys_coverage_pkt *pkt = packet;

		if (pkt->hdr.pkt_type != HFI_MSG_SYS_COV) {
			struct hfi_msg_sys_debug_pkt *pkt = packet;

			printk("%s", pkt->msg_data);
		}
	}
}

static int venus_prepare_power_collapse(struct venus_hfi_device *hdev)
{
	unsigned long timeout = msecs_to_jiffies(venus_hw_rsp_timeout);
	struct hfi_sys_pc_prep_pkt pkt;
	int ret;

	init_completion(&pc_prep_done);

	call_hfi_pkt_op(hdev, sys_pc_prep, &pkt);

	ret = venus_iface_cmdq_write(hdev, &pkt);
	if (ret)
		return ret;

	ret = wait_for_completion_timeout(&pc_prep_done, timeout);
	if (!ret) {
		venus_flush_debug_queue(hdev);
		return -ETIMEDOUT;
	}

	return 0;
}

static int venus_are_queues_empty(struct venus_hfi_device *hdev)
{
	int ret1, ret2;

	ret1 = venus_get_queue_size(hdev, IFACEQ_MSG_IDX);
	if (ret1 < 0)
		return ret1;

	ret2 = venus_get_queue_size(hdev, IFACEQ_CMD_IDX);
	if (ret2 < 0)
		return ret2;

	if (!ret1 && !ret2)
		return 1;

	return 0;
}

static void venus_sfr_print(struct venus_hfi_device *hdev)
{
	struct device *dev = hdev->dev;
	struct hfi_sfr *sfr = hdev->sfr.kva;
	void *p;

	if (!sfr)
		return;

	p = memchr(sfr->data, '\0', sfr->buf_size);
	/*
	 * SFR isn't guaranteed to be NULL terminated since SYS_ERROR indicates
	 * that Venus is in the process of crashing.
	 */
	if (p == NULL)
		sfr->data[sfr->buf_size - 1] = '\0';

	dev_err(dev, "SFR message from FW: %s\n", sfr->data);
}

static void venus_process_msg_sys_error(struct venus_hfi_device *hdev,
					void *packet)
{
	struct hfi_msg_event_notify_pkt *event_pkt = packet;

	if (event_pkt->event_id != HFI_EVENT_SYS_ERROR)
		return;

	venus_set_state(hdev, VENUS_STATE_DEINIT);

	/*
	 * Once SYS_ERROR received from HW, it is safe to halt the AXI.
	 * With SYS_ERROR, Venus FW may have crashed and HW might be
	 * active and causing unnecessary transactions. Hence it is
	 * safe to stop all AXI transactions from venus subsystem.
	 */
	venus_halt_axi(hdev);
	venus_sfr_print(hdev);
}

static irqreturn_t venus_isr_thread(int irq, struct hfi_device *hfi)
{
	struct venus_hfi_device *hdev = to_hfi_priv(hfi);
	void *pkt = hdev->pkt_buf;
	u32 msg_ret;

	if (!hdev)
		return IRQ_NONE;

	if (hdev->intr_status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK) {
		disable_irq_nosync(hdev->irq);
		venus_sfr_print(hdev);
		hfi_process_watchdog_timeout(hfi);
	}

	while (!venus_iface_msgq_read(hdev, pkt)) {
		msg_ret = hfi_process_msg_packet(hfi, pkt);

		dev_err(hdev->dev, "%s: read msg %x\n", __func__, msg_ret);

		switch (msg_ret) {
		case HFI_MSG_EVENT_NOTIFY:
			venus_process_msg_sys_error(hdev, pkt);
			break;
		case HFI_MSG_SYS_RELEASE_RESOURCE:
			complete(&release_resources_done);
			break;
		case HFI_MSG_SYS_PC_PREP:
			complete(&pc_prep_done);
			break;
		case HFI_MSG_SESSION_LOAD_RESOURCES:
			/*
			 * Work around for H/W bug, need to re-program these
			 * registers as part of a handshake agreement with the
			 * firmware.  This strictly only needs to be done for
			 * decoder secure sessions, but there's no harm in doing
			 * so for all sessions as it's at worst a NO-OP.
			 */
			venus_set_threshold_regs(hdev);
			break;
		case HFI_MSG_SYS_INIT: {
			struct hal_resource_hdr res;
			int ret;
			unsigned long size = 524288;
			phys_addr_t vmem_buffer = 0;
		
			vmem_allocate(size, &vmem_buffer);
			dev_err(hdev->dev, "\n vmem_allocate %x", vmem_buffer);
			res.resource_id = VIDC_RESOURCE_VMEM;
			res.resource_handle = hdev;
			res.size = 524288;
			ret = venus_hfi_core_set_resource(hdev, &res, vmem_buffer);
			if (ret)
				dev_err(hdev->dev, "set resource fail (%d)\n",
					ret);
			else
				dev_err(hdev->dev, "set vmem resource\n");

			break;
		}
		case HFI_MSG_SESSION_START:
			dev_err(hdev->dev, "cpu   status  %x",
				venus_readl(hdev, VIDC_WRAPPER_CPU_STATUS));
			dev_err(hdev->dev, "power status  %x",
				venus_readl(hdev, VIDC_WRAPPER_POWER_STATUS));
			dev_err(hdev->dev, "power control %x",
				venus_readl(hdev, 0xe0048));
		default:
			break;
		}
	}

	venus_flush_debug_queue(hdev);

	return IRQ_HANDLED;
}

static irqreturn_t venus_isr(int irq, struct hfi_device *hfi)
{
	struct venus_hfi_device *hdev = to_hfi_priv(hfi);
	u32 status;

	if (!hdev)
		return IRQ_NONE;

	status = venus_readl(hdev, VIDC_WRAPPER_INTR_STATUS);

	if (status & VIDC_WRAPPER_INTR_STATUS_A2H_BMSK ||
	    status & VIDC_WRAPPER_INTR_STATUS_A2HWD_BMSK ||
	    status & VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_INIT_IDLE_MSG_BMSK)
		hdev->intr_status = status;

	venus_writel(hdev, VIDC_CPU_CS_A2HSOFTINTCLR, 1);
	venus_writel(hdev, VIDC_WRAPPER_INTR_CLEAR, status);

	return IRQ_WAKE_THREAD;
}

static int venus_protect_cp_mem(struct venus_hfi_device *hdev)
{
	struct device *dev = hdev->dev;
	u32 cp_start, cp_size, cp_nonpixel_start, cp_nonpixel_size;
	int ret;

	cp_start		= 0;
	cp_size			= 0x70800000;
	cp_nonpixel_start	= 0x01000000;
	cp_nonpixel_size	= 0x24800000;

	ret = 0; /*qcom_scm_mem_protect_video_var(cp_start, cp_size,
					     cp_nonpixel_start,
					     cp_nonpixel_size);*/
	if (ret) {
		dev_err(dev, "failed to protect memory (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int venus_hfi_core_init(struct hfi_device *hfi)
{
	struct venus_hfi_device *hdev = to_hfi_priv(hfi);
	struct device *dev = hfi->dev;
	struct hfi_sys_get_property_pkt version_pkt;
	struct hfi_sys_init_pkt pkt;
	int ret;

	call_hfi_pkt_op(hdev, sys_init, &pkt, HFI_VIDEO_ARCH_OX);

	ret = venus_iface_cmdq_write(hdev, &pkt);
	if (ret)
		return ret;

	call_hfi_pkt_op(hdev, sys_image_version, &version_pkt);

	ret = venus_iface_cmdq_write(hdev, &version_pkt);
	if (ret)
		dev_warn(dev, "failed to send image version pkt to fw\n");

//	ret = venus_protect_cp_mem(hdev);
//	if (ret)
//		return ret;

	venus_set_state(hdev, VENUS_STATE_INIT);

	return 0;
}

static int venus_hfi_core_deinit(struct hfi_device *hfi)
{
	struct venus_hfi_device *hdev = to_hfi_priv(hfi);

	venus_set_state(hdev, VENUS_STATE_DEINIT);

	return 0;
}

static int venus_hfi_core_ping(struct hfi_device *hfi, u32 cookie)
{
	struct venus_hfi_device *hdev = to_hfi_priv(hfi);
	struct hfi_sys_ping_pkt pkt;

	call_hfi_pkt_op(hdev, sys_ping, &pkt, cookie);

	return venus_iface_cmdq_write(hdev, &pkt);
}

static int venus_hfi_core_trigger_ssr(struct hfi_device *hfi,
				      enum hal_ssr_trigger_type type)
{
	struct venus_hfi_device *hdev = to_hfi_priv(hfi);
	struct hfi_sys_test_ssr_pkt pkt;

	call_hfi_pkt_op(hdev, ssr_cmd, type, &pkt);

	return venus_iface_cmdq_write(hdev, &pkt);
}

int venus_hfi_session_init(struct hfi_device *hfi, struct hfi_device_inst *inst,
			   enum hal_session_type type,
			   enum hal_video_codec codec)
{
	struct venus_hfi_device *hdev = to_hfi_priv(hfi);
	struct hfi_session_init_pkt pkt;
	int ret;

	inst->device = hdev;

	ret = venus_sys_set_default_properties(hdev);
	if (ret)
		return ret;

	ret = call_hfi_pkt_op(hdev, session_init, &pkt, inst, type, codec);
	if (ret)
		goto err;

	ret = venus_iface_cmdq_write(hdev, &pkt);
	if (ret)
		goto err;

	return 0;

err:
	venus_flush_debug_queue(hdev);
	return ret;
}

static int venus_hfi_session_end(struct hfi_device_inst *inst)
{
	struct venus_hfi_device *hdev = inst->device;
	struct device *dev = hdev->dev;

	if (venus_fw_coverage) {
		if (venus_sys_set_coverage(hdev, venus_fw_coverage))
			dev_warn(dev, "fw coverage msg ON failed\n");
	}

	return venus_session_cmd(inst, HFI_CMD_SYS_SESSION_END);
}

static int venus_hfi_session_abort(struct hfi_device_inst *inst)
{
	struct venus_hfi_device *hdev = inst->device;

	venus_flush_debug_queue(hdev);

	return venus_session_cmd(inst, HFI_CMD_SYS_SESSION_ABORT);
}

static int venus_hfi_session_flush(struct hfi_device_inst *inst,
				   enum hal_flush flush_mode)
{
	struct venus_hfi_device *hdev = inst->device;
	struct hfi_session_flush_pkt pkt;
	int ret;

	ret = call_hfi_pkt_op(hdev, session_flush, &pkt, inst, flush_mode);
	if (ret)
		return ret;

	return venus_iface_cmdq_write(hdev, &pkt);
}

static int venus_hfi_session_start(struct hfi_device_inst *inst)
{
	return venus_session_cmd(inst, HFI_CMD_SESSION_START);
}

static int venus_hfi_session_stop(struct hfi_device_inst *inst)
{
	return venus_session_cmd(inst, HFI_CMD_SESSION_STOP);
}

static int venus_hfi_session_etb(struct hfi_device_inst *inst,
				 struct hal_frame_data *in_frame)
{
	struct venus_hfi_device *hdev = inst->device;
	enum hal_session_type session_type = inst->session_type;
	int ret;

	if (session_type == HAL_VIDEO_SESSION_TYPE_DECODER) {
		struct hfi_session_empty_buffer_compressed_pkt pkt;

		ret = call_hfi_pkt_op(hdev, session_etb_decoder, &pkt, inst,
				      in_frame);
		if (ret)
			return ret;

		ret = venus_iface_cmdq_write(hdev, &pkt);
	} else if (session_type == HAL_VIDEO_SESSION_TYPE_ENCODER) {
		struct hfi_session_empty_buffer_uncompressed_plane0_pkt pkt;

		ret = call_hfi_pkt_op(hdev, session_etb_encoder, &pkt, inst,
				      in_frame);
		if (ret)
			return ret;

		ret = venus_iface_cmdq_write(hdev, &pkt);
	} else {
		ret = -EINVAL;
	}

	venus_flush_debug_queue(hdev);

	return ret;
}

static int venus_hfi_session_ftb(struct hfi_device_inst *inst,
				 struct hal_frame_data *out_frame)
{
	struct venus_hfi_device *hdev = inst->device;
	struct hfi_session_fill_buffer_pkt pkt;
	int ret;

	ret = call_hfi_pkt_op(hdev, session_ftb, &pkt, inst, out_frame);
	if (ret)
		return ret;

	venus_flush_debug_queue(hdev);
	return venus_iface_cmdq_write(hdev, &pkt);
}

static int venus_hfi_session_set_buffers(struct hfi_device_inst *inst,
					 struct hal_buffer_addr_info *bai)
{
	struct venus_hfi_device *hdev = inst->device;
	struct hfi_session_set_buffers_pkt *pkt;
	u8 packet[IFACEQ_VAR_LARGE_PKT_SIZE];
	int ret;

	if (bai->buffer_type == HAL_BUFFER_INPUT)
		return 0;

	pkt = (struct hfi_session_set_buffers_pkt *)packet;

	ret = call_hfi_pkt_op(hdev, session_set_buffers, pkt, inst, bai);
	if (ret)
		return ret;

	return venus_iface_cmdq_write(hdev, pkt);
}

static int venus_hfi_session_release_buffers(struct hfi_device_inst *inst,
					     struct hal_buffer_addr_info *bai)
{
	struct venus_hfi_device *hdev = inst->device;
	struct hfi_session_release_buffer_pkt *pkt;
	u8 packet[IFACEQ_VAR_LARGE_PKT_SIZE];
	int ret;

	if (bai->buffer_type == HAL_BUFFER_INPUT)
		return 0;

	pkt = (struct hfi_session_release_buffer_pkt *) packet;

	ret = call_hfi_pkt_op(hdev, session_release_buffers, pkt, inst, bai);
	if (ret)
		return ret;

	return venus_iface_cmdq_write(hdev, pkt);
}

static int venus_hfi_session_load_res(struct hfi_device_inst *inst)
{
	return venus_session_cmd(inst, HFI_CMD_SESSION_LOAD_RESOURCES);
}

static int venus_hfi_session_release_res(struct hfi_device_inst *inst)
{
	return venus_session_cmd(inst, HFI_CMD_SESSION_RELEASE_RESOURCES);
}

static int venus_hfi_session_parse_seq_hdr(struct hfi_device_inst *inst,
					   struct hal_seq_hdr *seq_hdr)
{
	struct venus_hfi_device *hdev = inst->device;
	struct hfi_session_parse_sequence_header_pkt *pkt;
	u8 packet[IFACEQ_VAR_SMALL_PKT_SIZE];
	int ret;

	pkt = (struct hfi_session_parse_sequence_header_pkt *) packet;

	ret = call_hfi_pkt_op(hdev, session_parse_seq_header,
			      pkt, inst, seq_hdr);
	if (ret)
		return ret;

	ret = venus_iface_cmdq_write(hdev, pkt);
	if (ret)
		return ret;

	return 0;
}

static int venus_hfi_session_get_seq_hdr(struct hfi_device_inst *inst,
					 struct hal_seq_hdr *seq_hdr)
{
	struct venus_hfi_device *hdev = inst->device;
	struct hfi_session_get_sequence_header_pkt *pkt;
	u8 packet[IFACEQ_VAR_SMALL_PKT_SIZE];
	int ret;

	pkt = (struct hfi_session_get_sequence_header_pkt *) packet;

	ret = call_hfi_pkt_op(hdev, session_get_seq_hdr, pkt, inst, seq_hdr);
	if (ret)
		return ret;

	return venus_iface_cmdq_write(hdev, pkt);
}

static int venus_hfi_session_set_property(struct hfi_device_inst *inst,
					  enum hal_property ptype, void *pdata)
{
	struct venus_hfi_device *hdev = inst->device;
	struct hfi_session_set_property_pkt *pkt;
	u8 packet[IFACEQ_VAR_LARGE_PKT_SIZE];
	int ret;

	pkt = (struct hfi_session_set_property_pkt *) packet;

	ret = call_hfi_pkt_op(hdev, session_set_property, pkt, inst, ptype,
			      pdata);
	if (ret)
		return ret;

	dev_err(hdev->dev, "%s: set property %x\n", __func__, ptype);

	return venus_iface_cmdq_write(hdev, pkt);
}

static int venus_hfi_session_get_property(struct hfi_device_inst *inst,
					  enum hal_property ptype)
{
	struct venus_hfi_device *hdev = inst->device;
	struct hfi_session_get_property_pkt pkt;
	int ret;

	ret = call_hfi_pkt_op(hdev, session_get_property, &pkt, inst, ptype);
	if (ret)
		return ret;

	return venus_iface_cmdq_write(hdev, &pkt);
}

static int venus_hfi_resume(struct hfi_device *hfi)
{
	struct venus_hfi_device *hdev = to_hfi_priv(hfi);
	int ret = 0;

	mutex_lock(&hdev->lock);

	if (hdev->suspended == false)
		goto unlock;

	ret = venus_power_on(hdev);

unlock:
	if (!ret)
		hdev->suspended = false;

	mutex_unlock(&hdev->lock);

	return ret;
}

static int venus_hfi_suspend(struct hfi_device *hfi)
{
	struct venus_hfi_device *hdev = to_hfi_priv(hfi);
	struct device *dev = hfi->dev;
	u32 ctrl_status;
	int ret;

	return 0;

	if (!hdev->power_enabled || hdev->suspended)
		return 0;

	mutex_lock(&hdev->lock);
	ret = venus_is_valid_state(hdev);
	mutex_unlock(&hdev->lock);

	if (!ret) {
		dev_err(dev, "bad state, cannot suspend\n");
		return -EINVAL;
	}

	ret = venus_prepare_power_collapse(hdev);
	if (ret) {
		dev_err(dev, "prepare for power collapse fail (%d)\n", ret);
		return ret;
	}

	mutex_lock(&hdev->lock);

	if (hdev->last_packet_type != HFI_CMD_SYS_PC_PREP) {
		mutex_unlock(&hdev->lock);
		return -EINVAL;
	}

	ret = venus_are_queues_empty(hdev);
	if (ret < 0 || !ret) {
		mutex_unlock(&hdev->lock);
		return -EINVAL;
	}

	ctrl_status = venus_readl(hdev, VIDC_CPU_CS_SCIACMDARG0);
	if (!(ctrl_status & VIDC_CPU_CS_SCIACMDARG0_HFI_CTRL_PC_READY)) {
		mutex_unlock(&hdev->lock);
		return -EINVAL;
	}

	ret = venus_power_off(hdev);
	if (ret) {
		mutex_unlock(&hdev->lock);
		return ret;
	}

	hdev->suspended = true;

	mutex_unlock(&hdev->lock);

	return 0;
}

static int venus_hfi_session_continue(struct hfi_device_inst *inst)
{
	return venus_session_cmd(inst, HFI_CMD_SESSION_CONTINUE);
}

static const struct hfi_ops venus_hfi_ops = {
	.core_init			= venus_hfi_core_init,
	.core_deinit			= venus_hfi_core_deinit,
	.core_ping			= venus_hfi_core_ping,
	.core_trigger_ssr		= venus_hfi_core_trigger_ssr,

	.session_init			= venus_hfi_session_init,
	.session_end			= venus_hfi_session_end,
	.session_abort			= venus_hfi_session_abort,
	.session_flush			= venus_hfi_session_flush,
	.session_start			= venus_hfi_session_start,
	.session_stop			= venus_hfi_session_stop,
	.session_etb			= venus_hfi_session_etb,
	.session_ftb			= venus_hfi_session_ftb,
	.session_set_buffers		= venus_hfi_session_set_buffers,
	.session_release_buffers	= venus_hfi_session_release_buffers,
	.session_load_res		= venus_hfi_session_load_res,
	.session_release_res		= venus_hfi_session_release_res,
	.session_parse_seq_hdr		= venus_hfi_session_parse_seq_hdr,
	.session_get_seq_hdr		= venus_hfi_session_get_seq_hdr,
	.session_set_property		= venus_hfi_session_set_property,
	.session_get_property		= venus_hfi_session_get_property,

	.resume				= venus_hfi_resume,
	.suspend			= venus_hfi_suspend,

	.isr				= venus_isr,
	.isr_thread			= venus_isr_thread,

	.session_continue		= venus_hfi_session_continue,
};

void venus_hfi_destroy(struct hfi_device *hfi)
{
	struct venus_hfi_device *hdev = to_hfi_priv(hfi);

	venus_interface_queues_release(hdev);
	mutex_destroy(&hdev->lock);
	kfree(hdev);
}

int venus_hfi_create(struct hfi_device *hfi, struct vidc_resources *res)
{
	struct venus_hfi_device *hdev;
	int ret;

	hdev = kzalloc(sizeof(*hdev), GFP_KERNEL);
	if (!hdev)
		return -ENOMEM;

	mutex_init(&hdev->lock);

	hdev->res = res;
	hdev->pkt_ops = hfi->pkt_ops;
	hdev->packetization_type = HFI_PACKETIZATION_3XX;//HFI_PACKETIZATION_LEGACY;
	hdev->irq = res->irq;
	hdev->base = res->base;
	hdev->dev = hfi->dev;
	hdev->suspended = true;

	hfi->priv = hdev;
	hfi->ops = &venus_hfi_ops;
	hfi->core_caps = HAL_VIDEO_ENCODER_ROTATION_CAPABILITY |
			 HAL_VIDEO_ENCODER_SCALING_CAPABILITY |
			 HAL_VIDEO_ENCODER_DEINTERLACE_CAPABILITY |
			 HAL_VIDEO_DECODER_MULTI_STREAM_CAPABILITY;

	if (hdev->packetization_type == HFI_PACKETIZATION_3XX)
		hfi->def_properties = HAL_VIDEO_DYNAMIC_BUF_MODE;

	ret = venus_interface_queues_init(hdev);
	if (ret)
		goto err_kfree;

	return 0;

err_kfree:
	kfree(hdev);
	hfi->priv = NULL;
	hfi->ops = NULL;
	return ret;
}
