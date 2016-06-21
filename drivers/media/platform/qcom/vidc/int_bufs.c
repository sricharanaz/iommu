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

#include "helpers.h"
#include "int_bufs.h"
#include "mem.h"

struct vidc_internal_buf {
	struct list_head list;
	enum hal_buffer_type type;
	struct smem *smem;
};

static enum hal_buffer_type
scratch_buf_sufficient(struct vidc_inst *inst, enum hal_buffer_type buffer_type)
{
	struct hal_buffer_requirements *bufreq;
	struct vidc_internal_buf *buf;
	u32 count = 0;

	bufreq = vidc_get_buff_req_buffer(inst, buffer_type);
	if (!bufreq)
		return HAL_BUFFER_NONE;

	/* Check if current scratch buffers are sufficient */
	mutex_lock(&inst->scratchbufs.lock);
	list_for_each_entry(buf, &inst->scratchbufs.list, list) {
		if (buf->type == buffer_type &&
		    buf->smem->size >= bufreq->size)
			count++;
	}
	mutex_unlock(&inst->scratchbufs.lock);

	if (count != bufreq->count_actual)
		return HAL_BUFFER_NONE;

	return buffer_type;
}

static int internal_set_buf_on_fw(struct vidc_inst *inst,
				  enum hal_buffer_type buffer_type,
				  struct smem *mem, bool reuse)
{
	struct device *dev = inst->core->dev;
	struct hfi_device *hfi = &inst->core->hfi;
	struct hal_buffer_addr_info bai = {0};
	int ret;

	bai.buffer_size = mem->size;
	bai.buffer_type = buffer_type;
	bai.num_buffers = 1;
	bai.device_addr = mem->da;

	ret = vidc_hfi_session_set_buffers(hfi, inst->hfi_inst, &bai);
	if (ret) {
		dev_err(dev, "set session buffers failed\n");
		return ret;
	}

	return 0;
}

static int internal_alloc_and_set(struct vidc_inst *inst,
				  struct hal_buffer_requirements *bufreq,
				  struct vidc_list *buf_list)
{
	struct smem *smem;
	struct vidc_internal_buf *buf;
	unsigned int i;
	int ret = 0;

	if (!bufreq->size)
		return 0;

	for (i = 0; i < bufreq->count_actual; i++) {
		smem = smem_alloc(inst->core->dev, bufreq->size, 1, 0);
		if (IS_ERR(smem)) {
			ret = PTR_ERR(smem);
			goto err_no_mem;
		}

		buf = kzalloc(sizeof(*buf), GFP_KERNEL);
		if (!buf) {
			ret = -ENOMEM;
			goto fail_kzalloc;
		}

		buf->smem = smem;
		buf->type = bufreq->type;

		ret = internal_set_buf_on_fw(inst, bufreq->type, smem, false);
		if (ret)
			goto fail_set_buffers;

		mutex_lock(&buf_list->lock);
		list_add_tail(&buf->list, &buf_list->list);
		mutex_unlock(&buf_list->lock);
	}

	return ret;

fail_set_buffers:
	kfree(buf);
fail_kzalloc:
	smem_free(smem);
err_no_mem:
	return ret;
}

static bool
scratch_reuse_buffer(struct vidc_inst *inst, enum hal_buffer_type buffer_type)
{
	struct device *dev = inst->core->dev;
	struct vidc_internal_buf *buf;
	bool reused = false;
	int ret = 0;

	mutex_lock(&inst->scratchbufs.lock);
	list_for_each_entry(buf, &inst->scratchbufs.list, list) {
		if (buf->type != buffer_type)
			continue;

		ret = internal_set_buf_on_fw(inst, buffer_type, buf->smem,
					     true);
		if (ret) {
			dev_err(dev, "set internal buffers failed\n");
			reused = false;
			break;
		}

		reused = true;
	}
	mutex_unlock(&inst->scratchbufs.lock);

	return reused;
}

static int scratch_set_buffer(struct vidc_inst *inst, enum hal_buffer_type type)
{
	struct hal_buffer_requirements *bufreq;

	bufreq = vidc_get_buff_req_buffer(inst, type);
	if (!bufreq)
		return 0;

	if (scratch_reuse_buffer(inst, type))
		return 0;

	return internal_alloc_and_set(inst, bufreq, &inst->scratchbufs);
}

static int persist_set_buffer(struct vidc_inst *inst, enum hal_buffer_type type)
{
	struct hal_buffer_requirements *bufreq;

	bufreq = vidc_get_buff_req_buffer(inst, type);
	if (!bufreq)
		return 0;

	mutex_lock(&inst->persistbufs.lock);
	if (!list_empty(&inst->persistbufs.list)) {
		mutex_unlock(&inst->persistbufs.lock);
		return 0;
	}
	mutex_unlock(&inst->persistbufs.lock);

	return internal_alloc_and_set(inst, bufreq, &inst->persistbufs);
}

static int scratch_release_buffers(struct vidc_inst *inst, bool reuse)
{
	struct hfi_device *hfi = &inst->core->hfi;
	struct vidc_internal_buf *buf, *n;
	struct hal_buffer_addr_info bai = {0};
	enum hal_buffer_type sufficiency = HAL_BUFFER_NONE;
	int ret = 0;

	if (reuse) {
		sufficiency |= scratch_buf_sufficient(inst,
					HAL_BUFFER_INTERNAL_SCRATCH);
		sufficiency |= scratch_buf_sufficient(inst,
					HAL_BUFFER_INTERNAL_SCRATCH_1);
		sufficiency |= scratch_buf_sufficient(inst,
					HAL_BUFFER_INTERNAL_SCRATCH_2);
	}

	mutex_lock(&inst->scratchbufs.lock);
	list_for_each_entry_safe(buf, n, &inst->scratchbufs.list, list) {
		bai.buffer_size = buf->smem->size;
		bai.buffer_type = buf->type;
		bai.num_buffers = 1;
		bai.device_addr = buf->smem->da;
		bai.response_required = true;

		ret = vidc_hfi_session_unset_buffers(hfi, inst->hfi_inst, &bai);

		/* If scratch buffers can be reused, do not free the buffers */
		if (sufficiency & buf->type)
			continue;

		list_del(&buf->list);
		smem_free(buf->smem);
		kfree(buf);
	}
	mutex_unlock(&inst->scratchbufs.lock);

	return ret;
}

static int persist_release_buffers(struct vidc_inst *inst)
{
	struct hfi_device *hfi = &inst->core->hfi;
	struct vidc_internal_buf *buf, *n;
	struct hal_buffer_addr_info bai = {0};
	int ret = 0;

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_entry_safe(buf, n, &inst->persistbufs.list, list) {
		bai.buffer_size = buf->smem->size;
		bai.buffer_type = buf->type;
		bai.num_buffers = 1;
		bai.device_addr = buf->smem->da;
		bai.response_required = true;

		ret = vidc_hfi_session_unset_buffers(hfi, inst->hfi_inst, &bai);

		list_del(&buf->list);
		smem_free(buf->smem);
		kfree(buf);
	}
	mutex_unlock(&inst->persistbufs.lock);

	return ret;
}

static int scratch_set_buffers(struct vidc_inst *inst)
{
	struct device *dev = inst->core->dev;
	int ret;

	ret = scratch_release_buffers(inst, true);
	if (ret)
		dev_warn(dev, "Failed to release scratch buffers\n");

	ret = scratch_set_buffer(inst, HAL_BUFFER_INTERNAL_SCRATCH);
	if (ret)
		goto error;

	ret = scratch_set_buffer(inst, HAL_BUFFER_INTERNAL_SCRATCH_1);
	if (ret)
		goto error;

	ret = scratch_set_buffer(inst, HAL_BUFFER_INTERNAL_SCRATCH_2);
	if (ret)
		goto error;

	return 0;
error:
	scratch_release_buffers(inst, false);
	return ret;
}

static int persist_set_buffers(struct vidc_inst *inst)
{
	int ret;

	ret = persist_set_buffer(inst, HAL_BUFFER_INTERNAL_PERSIST);
	if (ret)
		goto error;

	ret = persist_set_buffer(inst, HAL_BUFFER_INTERNAL_PERSIST_1);
	if (ret)
		goto error;

	return 0;

error:
	persist_release_buffers(inst);
	return ret;
}

int internal_bufs_alloc(struct vidc_inst *inst)
{
	struct device *dev = inst->core->dev;
	int ret;

	ret = vidc_get_bufreqs(inst);
	if (ret) {
		dev_err(dev, "buffers requirements (%d)\n", ret);
		return ret;
	}

	ret = scratch_set_buffers(inst);
	if (ret) {
		dev_err(dev, "set scratch buffers (%d)\n", ret);
		return ret;
	}

	ret = persist_set_buffers(inst);
	if (ret) {
		dev_err(dev, "set persist buffers (%d)\n", ret);
		goto error;
	}

	return 0;

error:
	scratch_release_buffers(inst, false);
	return ret;
}

int internal_bufs_free(struct vidc_inst *inst)
{
	struct device *dev = inst->core->dev;
	int ret;

	ret = scratch_release_buffers(inst, false);
	if (ret)
		dev_err(dev, "failed to release scratch buffers: %d\n", ret);

	ret = persist_release_buffers(inst);
	if (ret)
		dev_err(dev, "failed to release persist buffers: %d\n", ret);

	return ret;
}
