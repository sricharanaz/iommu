/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <soc/qcom/scm.h>
#include <linux/dma-mapping.h>

/* QSEE_LOG_BUF_SIZE = 32K */
#define QSEE_LOG_BUF_SIZE			0x8000

/* TZ Diagnostic Area legacy version number */
#define TZBSP_DIAG_MAJOR_VERSION_LEGACY		2
/* Preprocessor Definitions and Constants */
#define TZBSP_MAX_CPU_COUNT			0x08
/* Number of VMID Tables */
#define TZBSP_DIAG_NUM_OF_VMID			16
/* VMID Description length */
#define TZBSP_DIAG_VMID_DESC_LEN 7
/* Number of Interrupts */
#define TZBSP_DIAG_INT_NUM  32
/* Length of descriptive name associated with Interrupt */
#define TZBSP_MAX_INT_DESC 16

/* VMID Table */
struct tzdbg_vmid_t {
	uint8_t vmid; /* Virtual Machine Identifier */
	uint8_t desc[TZBSP_DIAG_VMID_DESC_LEN];	/* ASCII Text */
};

/* Boot Info Table */
struct tzdbg_boot_info_t {
	uint32_t wb_entry_cnt;	/* Warmboot entry CPU Counter */
	uint32_t wb_exit_cnt;	/* Warmboot exit CPU Counter */
	uint32_t pc_entry_cnt;	/* Power Collapse entry CPU Counter */
	uint32_t pc_exit_cnt;	/* Power Collapse exit CPU counter */
	uint32_t warm_jmp_addr;	/* Last Warmboot Jump Address */
	uint32_t spare;	/* Reserved for future use. */
};

/* Reset Info Table */
struct tzdbg_reset_info_t {
	uint32_t reset_type;	/* Reset Reason */
	uint32_t reset_cnt;	/* Number of resets occured/CPU */
};

/*
 * Interrupt Info Table
 * @int_info: Type of Interrupt/exception
 * @avail: Availability of the slot
 * @spare: Reserved for future use
 * @int_num: Interrupt # for IRQ and FIQ
 * @int_desc: ASCII text describing type of interrupt e.g:
 *            Secure Timer, EBI XPU. This string is always null terminated,
 *            supporting at most TZBSP_MAX_INT_DESC characters.
 *            Any additional characters are truncated.
 * @int_count: # of times seen per CPU
 */
struct tzdbg_int_t {
	uint16_t int_info;
	uint8_t avail;
	uint8_t spare;
	uint32_t int_num;
	uint8_t int_desc[TZBSP_MAX_INT_DESC];
	uint64_t int_count[TZBSP_MAX_CPU_COUNT];
};

/* Log ring buffer position */
struct tzdbg_log_pos_t {
	uint16_t wrap;
	uint16_t offset;
};

/* Log ring buffer */
struct tzdbg_log_t {
	struct tzdbg_log_pos_t log_pos;
	/* open ended array to the end of the 4K IMEM buffer */
	uint8_t log_buf[];
};

/*
 * Diagnostic Table
 * Note: This is the reference data structure for tz diagnostic table
 * supporting TZBSP_MAX_CPU_COUNT, the real diagnostic data is directly
 * copied into buffer from i/o memory.
 * @magic_num:
 * @version
 * @cpu_count: Number of CPU's
 * @vmid_info_off: Offset of VMID Table
 * @boot_info_off: Offset of Boot Table
 * @reset_info_off: Offset of Reset info Table
 * @int_info_off: Offset of Interrupt info Table
 * @ring_off: Ring Buffer Offset
 * @ring_len: Ring Buffer Length
 * @vmid_info: VMID to EE Mapping
 * @boot_info: Boot Info
 * @reset_info: Reset Info
 * @num_interrupts:
 * @int_info:
 * @ring_buffer: TZ Ring Buffer - we need at least 2K for the ring buffer
 */
struct tzdbg_t {
	uint32_t magic_num;
	uint32_t version;
	uint32_t cpu_count;
	uint32_t vmid_info_off;
	uint32_t boot_info_off;
	uint32_t reset_info_off;
	uint32_t int_info_off;
	uint32_t ring_off;
	uint32_t ring_len;
	struct tzdbg_vmid_t vmid_info[TZBSP_DIAG_NUM_OF_VMID];
	struct tzdbg_boot_info_t boot_info[TZBSP_MAX_CPU_COUNT];
	struct tzdbg_reset_info_t reset_info[TZBSP_MAX_CPU_COUNT];
	uint32_t num_interrupts;
	struct tzdbg_int_t int_info[TZBSP_DIAG_INT_NUM];
	struct tzdbg_log_t ring_buffer;
};

/* Enumeration order for VMID's */
enum tzdbg_stats_type {
	TZDBG_BOOT = 0,
	TZDBG_RESET,
	TZDBG_INTERRUPT,
	TZDBG_VMID,
	TZDBG_GENERAL,
	TZDBG_LOG,
	TZDBG_QSEE_LOG,
	TZDBG_STATS_MAX
};

struct tzdbg_stat {
	char *name;
	char *data;
};

struct tzdbg {
	void __iomem *virt_iobase;
	struct tzdbg_t *diag_buf;
	char *disp_buf;
	int debug_tz[TZDBG_STATS_MAX];
	struct tzdbg_stat stat[TZDBG_STATS_MAX];
};

static struct tzdbg tzdbg = {
	.stat[TZDBG_BOOT].name = "boot",
	.stat[TZDBG_RESET].name = "reset",
	.stat[TZDBG_INTERRUPT].name = "interrupt",
	.stat[TZDBG_VMID].name = "vmid",
	.stat[TZDBG_GENERAL].name = "general",
	.stat[TZDBG_LOG].name = "log",
	.stat[TZDBG_QSEE_LOG].name = "qsee_log",
};

static struct tzdbg_log_t *g_qsee_log;
static uint32_t debug_rw_buf_size;

/* Debugfs data structure and functions */
static int _disp_tz_general_stats(void)
{
	int len = 0;

	len += snprintf(tzdbg.disp_buf + len, debug_rw_buf_size - 1,
			"   Version        : 0x%x\n"
			"   Magic Number   : 0x%x\n"
			"   Number of CPU  : %d\n",
			tzdbg.diag_buf->version,
			tzdbg.diag_buf->magic_num,
			tzdbg.diag_buf->cpu_count);
	tzdbg.stat[TZDBG_GENERAL].data = tzdbg.disp_buf;

	return len;
}

static int _disp_tz_vmid_stats(void)
{
	int i, num_vmid;
	int len = 0;
	struct tzdbg_vmid_t *ptr;

	ptr = (struct tzdbg_vmid_t *)((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->vmid_info_off);
	num_vmid = ((tzdbg.diag_buf->boot_info_off -
				tzdbg.diag_buf->vmid_info_off)/
					(sizeof(struct tzdbg_vmid_t)));

	for (i = 0; i < num_vmid; i++) {
		if (ptr->vmid < 0xFF) {
			len += snprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"   0x%x        %s\n",
				(uint32_t)ptr->vmid, (uint8_t *)ptr->desc);
		}
		if (len > (debug_rw_buf_size - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}
		ptr++;
	}

	tzdbg.stat[TZDBG_VMID].data = tzdbg.disp_buf;

	return len;
}

static int _disp_tz_boot_stats(void)
{
	int i;
	int len = 0;
	struct tzdbg_boot_info_t *ptr;

	ptr = (struct tzdbg_boot_info_t *)((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->boot_info_off);

	for (i = 0; i < tzdbg.diag_buf->cpu_count; i++) {
		len += snprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"  CPU #: %d\n"
				"     Warmboot jump address     : 0x%x\n"
				"     Warmboot entry CPU counter: 0x%x\n"
				"     Warmboot exit CPU counter : 0x%x\n"
				"     Power Collapse entry CPU counter: 0x%x\n"
				"     Power Collapse exit CPU counter : 0x%x\n",
				i, ptr->warm_jmp_addr, ptr->wb_entry_cnt,
				ptr->wb_exit_cnt, ptr->pc_entry_cnt,
				ptr->pc_exit_cnt);

		if (len > (debug_rw_buf_size - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}
		ptr++;
	}

	tzdbg.stat[TZDBG_BOOT].data = tzdbg.disp_buf;

	return len;
}

static int _disp_tz_reset_stats(void)
{
	int i;
	int len = 0;
	struct tzdbg_reset_info_t *ptr;

	ptr = (struct tzdbg_reset_info_t *)((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->reset_info_off);

	for (i = 0; i < tzdbg.diag_buf->cpu_count; i++) {
		len += snprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"  CPU #: %d\n"
				"     Reset Type (reason)       : 0x%x\n"
				"     Reset counter             : 0x%x\n",
				i, ptr->reset_type, ptr->reset_cnt);

		if (len > (debug_rw_buf_size - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}

		ptr++;
	}

	tzdbg.stat[TZDBG_RESET].data = tzdbg.disp_buf;

	return len;
}

static int _disp_tz_interrupt_stats(void)
{
	int i, j, int_info_size;
	int len = 0;
	int *num_int;
	unsigned char *ptr;
	struct tzdbg_int_t *tzdbg_ptr;

	num_int = (uint32_t *)((unsigned char *)tzdbg.diag_buf +
			(tzdbg.diag_buf->int_info_off - sizeof(uint32_t)));
	ptr = ((unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->int_info_off);
	int_info_size = ((tzdbg.diag_buf->ring_off -
				tzdbg.diag_buf->int_info_off)/(*num_int));

	for (i = 0; i < (*num_int); i++) {
		tzdbg_ptr = (struct tzdbg_int_t *)ptr;
		len += snprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"     Interrupt Number          : 0x%x\n"
				"     Type of Interrupt         : 0x%x\n"
				"     Description of interrupt  : %s\n",
				tzdbg_ptr->int_num,
				(uint32_t)tzdbg_ptr->int_info,
				(uint8_t *)tzdbg_ptr->int_desc);
		for (j = 0; j < tzdbg.diag_buf->cpu_count; j++) {
			len += snprintf(tzdbg.disp_buf + len,
				(debug_rw_buf_size - 1) - len,
				"     int_count on CPU # %d      : %u\n",
				(uint32_t)j,
				(uint32_t)tzdbg_ptr->int_count[j]);
		}
		len += snprintf(tzdbg.disp_buf + len, debug_rw_buf_size - 1,
									"\n");

		if (len > (debug_rw_buf_size - 1)) {
			pr_warn("%s: Cannot fit all info into the buffer\n",
								__func__);
			break;
		}

		ptr += int_info_size;
	}

	tzdbg.stat[TZDBG_INTERRUPT].data = tzdbg.disp_buf;

	return len;
}

static int _disp_tz_log_stats_legacy(void)
{
	int len = 0;
	unsigned char *ptr;

	ptr = (unsigned char *)tzdbg.diag_buf +
					tzdbg.diag_buf->ring_off;
	len += snprintf(tzdbg.disp_buf, (debug_rw_buf_size - 1) - len,
							"%s\n", ptr);

	tzdbg.stat[TZDBG_LOG].data = tzdbg.disp_buf;

	return len;
}

static int _disp_log_stats(struct tzdbg_log_t *log,
			   struct tzdbg_log_pos_t *log_start, uint32_t log_len,
			   size_t count, uint32_t buf_idx)
{
	uint32_t wrap_start;
	uint32_t wrap_end;
	uint32_t wrap_cnt;
	int max_len;
	int len = 0;
	int i = 0;

	wrap_start = log_start->wrap;
	wrap_end = log->log_pos.wrap;

	/* Calculate difference in # of buffer wrap-arounds */
	if (wrap_end >= wrap_start) {
		wrap_cnt = wrap_end - wrap_start;
	} else {
		/* wrap counter has wrapped around, invalidate start position */
		wrap_cnt = 2;
	}

	if (wrap_cnt > 1) {
		/* end position has wrapped around more than once, */
		/* current start no longer valid                   */
		log_start->wrap = log->log_pos.wrap - 1;
		log_start->offset = (log->log_pos.offset + 1) % log_len;
	} else if ((wrap_cnt == 1) &&
		(log->log_pos.offset > log_start->offset)) {
		/* end position has overwritten start */
		log_start->offset = (log->log_pos.offset + 1) % log_len;
	}

	while (log_start->offset == log->log_pos.offset) {
		/*
		 * No data in ring buffer,
		 * so we'll hang around until something happens
		 */
		unsigned long t = msleep_interruptible(50);
		if (t != 0) {
			/* Some event woke us up, so let's quit */
			return 0;
		}

		if (buf_idx == TZDBG_LOG)
			memcpy_fromio((void *)tzdbg.diag_buf, tzdbg.virt_iobase,
						debug_rw_buf_size);

	}

	max_len = (count > debug_rw_buf_size) ? debug_rw_buf_size : count;

	/*
	 *  Read from ring buff while there is data and space in return buff
	 */
	while ((log_start->offset != log->log_pos.offset) && (len < max_len)) {
		tzdbg.disp_buf[i++] = log->log_buf[log_start->offset];
		log_start->offset = (log_start->offset + 1) % log_len;
		if (0 == log_start->offset)
			++log_start->wrap;
		++len;
	}

	/*
	 * return buffer to caller
	 */
	tzdbg.stat[buf_idx].data = tzdbg.disp_buf;
	return len;
}

static int _disp_tz_log_stats(size_t count)
{
	static struct tzdbg_log_pos_t log_start = {0};
	struct tzdbg_log_t *log_ptr;
	log_ptr = (struct tzdbg_log_t *)((unsigned char *)tzdbg.diag_buf +
				tzdbg.diag_buf->ring_off -
				offsetof(struct tzdbg_log_t, log_buf));

	return _disp_log_stats(log_ptr, &log_start,
				tzdbg.diag_buf->ring_len, count, TZDBG_LOG);
}

static int _disp_qsee_log_stats(size_t count)
{
	static struct tzdbg_log_pos_t log_start = {0};

	return _disp_log_stats(g_qsee_log, &log_start,
			QSEE_LOG_BUF_SIZE - sizeof(struct tzdbg_log_pos_t),
			count, TZDBG_QSEE_LOG);
}

static ssize_t tzdbgfs_read(struct file *file, char __user *buf,
			    size_t count, loff_t *offp)
{
	int len = 0;
	int *tz_id = file->private_data;

	memcpy_fromio((void *)tzdbg.diag_buf, tzdbg.virt_iobase,
		      debug_rw_buf_size);

	switch (*tz_id) {
	case TZDBG_BOOT:
		len = _disp_tz_boot_stats();
		break;
	case TZDBG_RESET:
		len = _disp_tz_reset_stats();
		break;
	case TZDBG_INTERRUPT:
		len = _disp_tz_interrupt_stats();
		break;
	case TZDBG_GENERAL:
		len = _disp_tz_general_stats();
		break;
	case TZDBG_VMID:
		len = _disp_tz_vmid_stats();
		break;
	case TZDBG_LOG:
		if (TZBSP_DIAG_MAJOR_VERSION_LEGACY <
				(tzdbg.diag_buf->version >> 16)) {
			len = _disp_tz_log_stats(count);
			*offp = 0;
		} else {
			len = _disp_tz_log_stats_legacy();
		}
		break;
	case TZDBG_QSEE_LOG:
		len = _disp_qsee_log_stats(count);
		*offp = 0;
		break;
	default:
		break;
	}

	if (len > count)
		len = count;

	return simple_read_from_buffer(buf, len, offp,
				       tzdbg.stat[(*tz_id)].data, len);
}

static int tzdbgfs_open(struct inode *inode, struct file *pfile)
{
	pfile->private_data = inode->i_private;

	return 0;
}

const struct file_operations tzdbg_fops = {
	.owner = THIS_MODULE,
	.read = tzdbgfs_read,
	.open = tzdbgfs_open,
};

struct __packed qseecom_reg_log_buf_ireq {
	u32 cmd_id;
	u32 phy_addr;
	u32 len;
};

enum qseecom_command_scm_resp_type {
	QSEOS_APP_ID = 0xEE01,
	QSEOS_LISTENER_ID
};

/*
 * struct qseecom_command_scm_resp - qseecom response buffer
 * @cmd_status: value from enum tz_sched_cmd_status
 * @sb_in_rsp_addr: points to physical location of response
 *                buffer
 * @sb_in_rsp_len: length of command response
 */
__packed struct qseecom_command_scm_resp {
	uint32_t result;
	enum qseecom_command_scm_resp_type resp_type;
	unsigned int data;
};

#define QSEOS_REGISTER_LOG_BUF_COMMAND	14
#define QSEOS_RESULT_SUCCESS		0

/*
 * Allocates log buffer from ION, registers the buffer at TZ
 */
static int tzdbg_register_qsee_log_buf(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct qseecom_reg_log_buf_ireq req;
	struct qseecom_command_scm_resp resp = {0};
	dma_addr_t pa;
	size_t len = QSEE_LOG_BUF_SIZE;
	int ret = 0;
	void *buf;
	DEFINE_DMA_ATTRS(attrs);

	dev->coherent_dma_mask = DMA_BIT_MASK(sizeof(dma_addr_t) * 8);
	dma_set_attr(DMA_ATTR_STRONGLY_ORDERED, &attrs);

	buf = dma_alloc_attrs(dev, len, &pa, GFP_KERNEL, &attrs);
	if (!buf) {
		dev_err(dev, "cannot allocate dma memory\n");
		return -ENOMEM;
	}

	req.cmd_id = QSEOS_REGISTER_LOG_BUF_COMMAND;
	req.phy_addr = (u32)pa;
	req.len = len;

	if (!is_scm_armv8()) {
		/*  SCM_CALL  to register the log buffer */
		ret = scm_call(SCM_SVC_TZSCHEDULER, 1,  &req, sizeof(req),
			       &resp, sizeof(resp));
	} else {
		struct scm_desc desc = {0};

		desc.args[0] = (uint32_t)pa;
		desc.args[1] = len;
		desc.arginfo = 0x22;

		ret = scm_call2(SCM_QSEEOS_FNID(1, 6), &desc);

		resp.result = desc.ret[0];
	}

	if (ret) {
		dev_err(dev, "scm_call to register log buffer failed\n");
		goto err2;
	}

	if (resp.result != QSEOS_RESULT_SUCCESS) {
		dev_err(dev, "scm_call to register log buf failed, resp result =%d\n",
			resp.result);
		ret = -EINVAL;
		goto err2;
	}

	g_qsee_log = buf;
	g_qsee_log->log_pos.wrap = 0;
	g_qsee_log->log_pos.offset = 0;

	return 0;

err2:
	dma_free_attrs(dev, len, buf, pa, &attrs);

	return ret;
}

static int tzdbgfs_init(struct platform_device *pdev)
{
	int i;
	struct dentry *dent_dir;
	struct dentry *dent;
	int ret;

	dent_dir = debugfs_create_dir("tzdbg", NULL);
	if (!dent_dir) {
		dev_err(&pdev->dev, "tzdbg debugfs_create_dir failed\n");
		return -EINVAL;
	}

	for (i = 0; i < TZDBG_STATS_MAX; i++) {
		tzdbg.debug_tz[i] = i;
		dent = debugfs_create_file(tzdbg.stat[i].name,
					   S_IRUGO, dent_dir,
					   &tzdbg.debug_tz[i], &tzdbg_fops);
		if (!dent) {
			dev_err(&pdev->dev, "TZ debugfs_create_file failed\n");
			ret = -EINVAL;
			goto err;
		}
	}

	tzdbg.disp_buf = kzalloc(debug_rw_buf_size, GFP_KERNEL);
	if (!tzdbg.disp_buf) {
		dev_err(&pdev->dev,
			"%s: Can't Allocate memory for tzdbg.disp_buf\n",
			__func__);
		ret = -ENOMEM;
		goto err;
	}

	platform_set_drvdata(pdev, dent_dir);

	return 0;
err:
	debugfs_remove_recursive(dent_dir);

	return ret;
}

static int tz_log_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *resource;
	void __iomem *virt_iobase;
	phys_addr_t tzdiag_phy_iobase;
	u32 *ptr = NULL;
	int ret;

	/* Get address that stores the physical location diagnostic data */
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		dev_err(dev, "%s: ERROR Missing MEM resource\n", __func__);
		return -ENODEV;
	};

	/* Get the debug buffer size */
	debug_rw_buf_size = resource->end - resource->start + 1;

	/* Map address that stores the physical location diagnostic data */
	virt_iobase = devm_ioremap_nocache(dev, resource->start,
					   debug_rw_buf_size);
	if (!virt_iobase) {
		dev_err(dev, "%s: ERROR could not ioremap: start=%pr, len=%u\n",
			__func__, &resource->start,
			(unsigned int)(debug_rw_buf_size));
		return -ENODEV;
	}

	/* Retrieve the address of diagnostic data */
	tzdiag_phy_iobase = readl_relaxed(virt_iobase);

	dev_info(dev, "tzdiag_phy_base: %pa\n", &tzdiag_phy_iobase);

	/* Map the diagnostic information area */
	tzdbg.virt_iobase = devm_ioremap_nocache(dev, tzdiag_phy_iobase,
						 debug_rw_buf_size);
	if (!tzdbg.virt_iobase) {
		dev_err(dev, "%s: ERROR could not ioremap: start=%pr, len=%u\n",
			__func__, &tzdiag_phy_iobase,
			debug_rw_buf_size);
		return -ENODEV;
	}

	ptr = kzalloc(debug_rw_buf_size, GFP_KERNEL);
	if (!ptr) {
		dev_err(dev, "%s: Can't Allocate memory: ptr\n", __func__);
		return -ENOMEM;
	}

	tzdbg.diag_buf = (struct tzdbg_t *)ptr;

	ret = tzdbgfs_init(pdev);
	if (ret)
		goto err;

	ret = tzdbg_register_qsee_log_buf(pdev);
	if (ret)
		goto err;

	return 0;
err:
	kfree(tzdbg.diag_buf);
	dev_err(dev, "tzdbg probe failed (%d)\n", ret);
	return ret;
}


static int tz_log_remove(struct platform_device *pdev)
{
	struct dentry *dent_dir = platform_get_drvdata(pdev);

	kzfree(tzdbg.diag_buf);
	kzfree(tzdbg.disp_buf);
	debugfs_remove_recursive(dent_dir);

	return 0;
}

static struct of_device_id tzlog_match[] = {
	{ .compatible = "qcom,tz-log", },
	{ }
};

static struct platform_driver tz_log_driver = {
	.probe = tz_log_probe,
	.remove = tz_log_remove,
	.driver = {
		.name = "tz_log",
		.owner = THIS_MODULE,
		.of_match_table = tzlog_match,
	},
};

module_platform_driver(tz_log_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TZ Log driver");
MODULE_VERSION("1.1");
MODULE_ALIAS("platform:tz_log");
