/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>

#include "hfi.h"
#include "hfi_cmds.h"
#include "venus/venus_hfi.h"

#define TIMEOUT		msecs_to_jiffies(1000)

static enum hal_session_type to_session_type(int session_type)
{
	switch (session_type) {
	case VIDC_ENCODER:
		return HAL_VIDEO_SESSION_TYPE_ENCODER;
	case VIDC_DECODER:
		return HAL_VIDEO_SESSION_TYPE_DECODER;
	default:
		return -1;
	}
}

static enum hal_video_codec to_codec_type(u32 pixfmt)
{
	switch (pixfmt) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H264_NO_SC:
		return HAL_VIDEO_CODEC_H264;
	case V4L2_PIX_FMT_H264_MVC:
		return HAL_VIDEO_CODEC_MVC;
	case V4L2_PIX_FMT_H263:
		return HAL_VIDEO_CODEC_H263;
	case V4L2_PIX_FMT_MPEG1:
		return HAL_VIDEO_CODEC_MPEG1;
	case V4L2_PIX_FMT_MPEG2:
		return HAL_VIDEO_CODEC_MPEG2;
	case V4L2_PIX_FMT_MPEG4:
		return HAL_VIDEO_CODEC_MPEG4;
	case V4L2_PIX_FMT_VC1_ANNEX_G:
	case V4L2_PIX_FMT_VC1_ANNEX_L:
		return HAL_VIDEO_CODEC_VC1;
	case V4L2_PIX_FMT_VP8:
		return HAL_VIDEO_CODEC_VP8;
	case V4L2_PIX_FMT_XVID:
		return HAL_VIDEO_CODEC_DIVX;
	default:
		return HAL_UNUSED_CODEC;
	}
}

int vidc_hfi_core_init(struct hfi_device *hfi)
{
	int ret = 0;

	mutex_lock(&hfi->lock);

	if (hfi->state >= CORE_INIT)
		goto unlock;

	init_completion(&hfi->done);

	ret = call_hfi_op(hfi, core_init, hfi);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&hfi->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	ret = 0;

	if (hfi->error != HAL_ERR_NONE) {
		ret = -EINVAL;
		goto unlock;
	}

	hfi->state = CORE_INIT;
unlock:
	mutex_unlock(&hfi->lock);
	return ret;
}

int vidc_hfi_core_deinit(struct hfi_device *hfi)
{
	struct device *dev = hfi->dev;
	int ret = 0;

	mutex_lock(&hfi->lock);

	if (hfi->state == CORE_UNINIT)
		goto unlock;

	if (!list_empty(&hfi->instances)) {
		ret = -EBUSY;
		goto unlock;
	}

	/*
	 * Delay unloading of firmware. This is useful
	 * in avoiding firmware download delays in cases where we
	 * will have a burst of back to back video playback sessions
	 * e.g. thumbnail generation.
	 */
	ret = call_hfi_op(hfi, core_deinit, hfi);
	if (ret)
		dev_err(dev, "core deinit failed: %d\n", ret);

	hfi->state = CORE_UNINIT;

unlock:
	mutex_unlock(&hfi->lock);
	return ret;
}

int vidc_hfi_core_suspend(struct hfi_device *hfi)
{
	return call_hfi_op(hfi, suspend, hfi);
}

int vidc_hfi_core_resume(struct hfi_device *hfi)
{
	return call_hfi_op(hfi, resume, hfi);
}

int vidc_hfi_core_trigger_ssr(struct hfi_device *hfi,
			      enum hal_ssr_trigger_type type)
{
	int ret;

	ret = call_hfi_op(hfi, core_trigger_ssr, hfi, type);
	if (ret)
		return ret;

	return 0;
}

int vidc_hfi_core_ping(struct hfi_device *hfi)
{
	int ret;

	mutex_lock(&hfi->lock);

	ret = call_hfi_op(hfi, core_ping, hfi, 0xbeef);
	if (ret)
		return ret;

	ret = wait_for_completion_timeout(&hfi->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}
	ret = 0;
	if (hfi->error != HAL_ERR_NONE)
		ret = -ENODEV;
unlock:
	mutex_unlock(&hfi->lock);
	return ret;
}

struct hfi_device_inst *vidc_hfi_session_create(struct hfi_device *hfi,
						const struct hfi_inst_ops *ops,
						void *ops_priv)
{
	struct hfi_device_inst *inst;

	if (!ops)
		return ERR_PTR(-EINVAL);

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return ERR_PTR(-ENOMEM);

	mutex_init(&inst->lock);
	INIT_LIST_HEAD(&inst->list);
	inst->state = INST_UNINIT;
	inst->ops = ops;
	inst->ops_priv = ops_priv;

	mutex_lock(&hfi->lock);
	list_add_tail(&inst->list, &hfi->instances);
	mutex_unlock(&hfi->lock);

	return inst;
}

int vidc_hfi_session_init(struct hfi_device *hfi, struct hfi_device_inst *inst,
			  u32 pixfmt, enum session_type type)
{
	enum hal_session_type stype;
	enum hal_video_codec codec;
	int ret;

	if (!hfi || !inst)
		return -EINVAL;

	dev_dbg(hfi->dev, "%s: enter\n", __func__);

	stype = to_session_type(type);
	codec = to_codec_type(pixfmt);

	inst->session_type = stype;
	inst->codec = codec;

	init_completion(&inst->done);

	mutex_lock(&inst->lock);

	ret = call_hfi_op(hfi, session_init, hfi, inst, stype, codec);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&inst->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	if (inst->error != HAL_ERR_NONE) {
		dev_err(hfi->dev, "%s: session init failed (%x)\n", __func__,
			inst->error);
		ret = -EINVAL;
		goto unlock;
	}

	ret = 0;
	inst->state = INST_INIT;

unlock:
	mutex_unlock(&inst->lock);

	dev_dbg(hfi->dev, "%s: exit (%d)\n", __func__, ret);

	return ret;
}

void vidc_hfi_session_destroy(struct hfi_device *hfi,
			      struct hfi_device_inst *inst)
{
	mutex_lock(&hfi->lock);
	list_del(&inst->list);
	mutex_unlock(&hfi->lock);

	if (mutex_is_locked(&inst->lock))
		WARN(1, "session destroy");

	mutex_destroy(&inst->lock);
	kfree(inst);
}

int vidc_hfi_session_deinit(struct hfi_device *hfi,
			    struct hfi_device_inst *inst)
{
	int ret;

	dev_dbg(hfi->dev, "%s: enter\n", __func__);

	mutex_lock(&inst->lock);

	if (inst->state == INST_UNINIT) {
		ret = 0;
		goto unlock;
	}

	if (inst->state < INST_INIT) {
		ret = -EINVAL;
		goto unlock;
	}

	init_completion(&inst->done);

	ret = call_hfi_op(hfi, session_end, inst);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&inst->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	if (inst->error != HAL_ERR_NONE) {
		dev_err(hfi->dev, "%s: session deinit failed (%x)\n", __func__,
			inst->error);
		ret = -EINVAL;
		goto unlock;
	}

	ret = 0;
	inst->state = INST_UNINIT;

unlock:
	mutex_unlock(&inst->lock);

	dev_dbg(hfi->dev, "%s: exit (%d)\n", __func__, ret);

	return ret;
}

int vidc_hfi_session_start(struct hfi_device *hfi, struct hfi_device_inst *inst)
{
	int ret;

	dev_dbg(hfi->dev, "%s: enter\n", __func__);

	mutex_lock(&inst->lock);

	if (inst->state != INST_LOAD_RESOURCES) {
		ret = -EINVAL;
		goto unlock;
	}

	init_completion(&inst->done);

	ret = call_hfi_op(hfi, session_start, inst);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&inst->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	ret = 0;

	inst->state = INST_START;
unlock:
	mutex_unlock(&inst->lock);

	dev_dbg(hfi->dev, "%s: exit (%d)\n", __func__, ret);

	return ret;
}

int vidc_hfi_session_stop(struct hfi_device *hfi, struct hfi_device_inst *inst)
{
	int ret;

	dev_dbg(hfi->dev, "%s: enter\n", __func__);

	mutex_lock(&inst->lock);

	if (inst->state != INST_START) {
		ret = -EINVAL;
		goto unlock;
	}

	init_completion(&inst->done);

	ret = call_hfi_op(hfi, session_stop, inst);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&inst->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	ret = 0;

	inst->state = INST_STOP;
unlock:
	mutex_unlock(&inst->lock);

	dev_dbg(hfi->dev, "%s: exit (%d)\n", __func__, ret);

	return ret;
}

int vidc_hfi_session_abort(struct hfi_device *hfi, struct hfi_device_inst *inst)
{
	int ret;

	mutex_lock(&inst->lock);

	init_completion(&inst->done);

	ret = call_hfi_op(hfi, session_abort, inst);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&inst->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	ret = 0;

unlock:
	mutex_unlock(&inst->lock);

	return ret;
}

int vidc_hfi_session_load_res(struct hfi_device *hfi,
			      struct hfi_device_inst *inst)
{
	int ret;

	mutex_lock(&inst->lock);

	if (inst->state != INST_INIT) {
		ret = -EINVAL;
		goto unlock;
	}

	init_completion(&inst->done);

	ret = call_hfi_op(hfi, session_load_res, inst);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&inst->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	ret = 0;
	inst->state = INST_LOAD_RESOURCES;
unlock:
	mutex_unlock(&inst->lock);

	return ret;
}

int vidc_hfi_session_unload_res(struct hfi_device *hfi,
				struct hfi_device_inst *inst)
{
	int ret;

	mutex_lock(&inst->lock);

	if (inst->state != INST_STOP) {
		ret = -EINVAL;
		goto unlock;
	}

	init_completion(&inst->done);

	ret = call_hfi_op(hfi, session_release_res, inst);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&inst->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	ret = 0;
	inst->state = INST_RELEASE_RESOURCES;
unlock:
	mutex_unlock(&inst->lock);

	return ret;
}

int vidc_hfi_session_flush(struct hfi_device *hfi, struct hfi_device_inst *inst)
{
	int ret;

	mutex_lock(&inst->lock);
	init_completion(&inst->done);

	ret = call_hfi_op(hfi, session_flush, inst, HAL_FLUSH_ALL);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&inst->done, TIMEOUT);
	if (!ret)
		ret = -ETIMEDOUT;

	ret = 0;
unlock:
	mutex_unlock(&inst->lock);

	return ret;
}

int vidc_hfi_session_set_buffers(struct hfi_device *hfi,
				 struct hfi_device_inst *inst,
				 struct hal_buffer_addr_info *bai)
{
	int ret;

	mutex_lock(&inst->lock);
	ret = call_hfi_op(hfi, session_set_buffers, inst, bai);
	mutex_unlock(&inst->lock);

	return ret;
}

int vidc_hfi_session_unset_buffers(struct hfi_device *hfi,
				   struct hfi_device_inst *inst,
				   struct hal_buffer_addr_info *bai)
{
	int ret;

	mutex_lock(&inst->lock);

	init_completion(&inst->done);

	ret = call_hfi_op(hfi, session_release_buffers, inst, bai);
	if (ret)
		goto unlock;

	if (!bai->response_required) {
		ret = 0;
		goto unlock;
	}

	ret = wait_for_completion_timeout(&inst->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	ret = 0;

	if (inst->error != HAL_ERR_NONE) {
		dev_dbg(hfi->dev, "release buffers error %x\n", inst->error);
		ret = -EIO;
	}

unlock:
	mutex_unlock(&inst->lock);

	return ret;
}

int vidc_hfi_session_get_property(struct hfi_device *hfi,
				  struct hfi_device_inst *inst,
				  enum hal_property ptype,
				  union hal_get_property *hprop)
{
	int ret;

	mutex_lock(&inst->lock);

	if (inst->state < INST_INIT || inst->state >= INST_STOP) {
		ret = -EINVAL;
		goto unlock;
	}

	switch (ptype) {
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
	case HAL_PARAM_GET_BUFFER_REQUIREMENTS:
		break;
	default:
		ret = -ENOTSUPP;
		goto unlock;
	}

	init_completion(&inst->done);

	ret = call_hfi_op(hfi, session_get_property, inst, ptype);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&inst->done, TIMEOUT);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto unlock;
	}

	if (inst->error != HAL_ERR_NONE) {
		ret = -EINVAL;
		goto unlock;
	}

	ret = 0;
	*hprop = inst->hprop;
unlock:
	mutex_unlock(&inst->lock);

	return ret;
}

int vidc_hfi_session_set_property(struct hfi_device *hfi,
				  struct hfi_device_inst *inst,
				  enum hal_property ptype, void *pdata)
{
	int ret;

	mutex_lock(&inst->lock);

	if (inst->state < INST_INIT || inst->state >= INST_STOP) {
		ret = -EINVAL;
		goto unlock;
	}

	inst->last_ptype = ptype;

	ret = call_hfi_op(hfi, session_set_property, inst, ptype, pdata);
unlock:
	mutex_unlock(&inst->lock);

	return ret;
}

int vidc_hfi_session_etb(struct hfi_device *hfi, struct hfi_device_inst *inst,
			 struct hal_frame_data *fdata)
{
	return call_hfi_op(hfi, session_etb, inst, fdata);
}

int vidc_hfi_session_ftb(struct hfi_device *hfi, struct hfi_device_inst *inst,
			 struct hal_frame_data *fdata)
{
	return call_hfi_op(hfi, session_ftb, inst, fdata);
}

irqreturn_t vidc_hfi_isr_thread(int irq, void *dev_id)
{
	struct hfi_device *hfi = dev_id;

	return call_hfi_op(hfi, isr_thread, irq, hfi);
}

irqreturn_t vidc_hfi_isr(int irq, void *dev)
{
	struct hfi_device *hfi = dev;

	return call_hfi_op(hfi, isr, irq, hfi);
}

int vidc_hfi_create(struct hfi_device *hfi, struct vidc_resources *res)
{
	const char *hfi_version = res->hfi_version;
	int ret;

	if (!hfi->core_ops || !hfi->dev)
		return -EINVAL;

	if (hfi_version && !strcmp(hfi_version, "3xx"))
		hfi->packetization_type = HFI_PACKETIZATION_3XX;
	else
		hfi->packetization_type = HFI_PACKETIZATION_LEGACY;
	mutex_init(&hfi->lock);
	INIT_LIST_HEAD(&hfi->instances);
	hfi->state = CORE_UNINIT;

	hfi->pkt_ops = hfi_get_pkt_ops(hfi->packetization_type);
	if (!hfi->pkt_ops)
		return -EINVAL;

	switch (hfi->hfi_type) {
	case VIDC_VENUS:
		ret = venus_hfi_create(hfi, res);
		break;
	case VIDC_Q6:
	default:
		ret = -ENOTSUPP;
	}

	return ret;
}

void vidc_hfi_destroy(struct hfi_device *hfi)
{
	switch (hfi->hfi_type) {
	case VIDC_VENUS:
		venus_hfi_destroy(hfi);
		break;
	case VIDC_Q6:
	default:
		break;
	}

	mutex_destroy(&hfi->lock);
}
