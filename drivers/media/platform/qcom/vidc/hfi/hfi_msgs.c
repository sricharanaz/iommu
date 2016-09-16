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
#include <linux/hash.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <media/videobuf2-v4l2.h>

#include "hfi.h"
#include "hfi_helper.h"
#include "hfi_msgs.h"

static enum hal_error to_hal_error(u32 hfi_err)
{
	switch (hfi_err) {
	case HFI_ERR_NONE:
	case HFI_ERR_SESSION_SAME_STATE_OPERATION:
		return HAL_ERR_NONE;
	case HFI_ERR_SYS_FATAL:
		return HAL_ERR_HW_FATAL;
	case HFI_ERR_SYS_VERSION_MISMATCH:
	case HFI_ERR_SYS_INVALID_PARAMETER:
	case HFI_ERR_SYS_SESSION_ID_OUT_OF_RANGE:
	case HFI_ERR_SESSION_INVALID_PARAMETER:
	case HFI_ERR_SESSION_INVALID_SESSION_ID:
	case HFI_ERR_SESSION_INVALID_STREAM_ID:
		return HAL_ERR_BAD_PARAM;
	case HFI_ERR_SYS_INSUFFICIENT_RESOURCES:
	case HFI_ERR_SYS_UNSUPPORTED_DOMAIN:
	case HFI_ERR_SYS_UNSUPPORTED_CODEC:
	case HFI_ERR_SESSION_UNSUPPORTED_PROPERTY:
	case HFI_ERR_SESSION_UNSUPPORTED_SETTING:
	case HFI_ERR_SESSION_INSUFFICIENT_RESOURCES:
	case HFI_ERR_SESSION_UNSUPPORTED_STREAM:
		return HAL_ERR_NOT_SUPPORTED;
	case HFI_ERR_SYS_MAX_SESSIONS_REACHED:
		return HAL_ERR_MAX_CLIENTS;
	case HFI_ERR_SYS_SESSION_IN_USE:
		return HAL_ERR_CLIENT_PRESENT;
	case HFI_ERR_SESSION_FATAL:
		return HAL_ERR_CLIENT_FATAL;
	case HFI_ERR_SESSION_BAD_POINTER:
		return HAL_ERR_BAD_PARAM;
	case HFI_ERR_SESSION_INCORRECT_STATE_OPERATION:
		return HAL_ERR_BAD_STATE;
	case HFI_ERR_SESSION_STREAM_CORRUPT:
	case HFI_ERR_SESSION_STREAM_CORRUPT_OUTPUT_STALLED:
		return HAL_ERR_BITSTREAM_ERR;
	case HFI_ERR_SESSION_SYNC_FRAME_NOT_DETECTED:
		return HAL_ERR_IFRAME_EXPECTED;
	case HFI_ERR_SESSION_START_CODE_NOT_FOUND:
		return HAL_ERR_START_CODE_NOT_FOUND;
	case HFI_ERR_SESSION_EMPTY_BUFFER_DONE_OUTPUT_PENDING:
	default:
		return HAL_ERR_FAIL;
	}

	return HAL_ERR_FAIL;
}

static struct hfi_device_inst *
to_hfi_instance(struct hfi_device *hfi, u32 session_id)
{
	struct hfi_device_inst *inst;

	mutex_lock(&hfi->lock);
	list_for_each_entry(inst, &hfi->instances, list)
		if (hash32_ptr(inst) == session_id) {
			mutex_unlock(&hfi->lock);
			return inst;
		}
	mutex_unlock(&hfi->lock);

	return NULL;
}

static void event_seq_changed(struct hfi_device *hfi,
			      struct hfi_device_inst *inst,
			      struct hfi_msg_event_notify_pkt *pkt)
{
	struct device *dev = hfi->dev;
	struct hal_cb_event event = {0};
	int num_properties_changed;
	struct hfi_framesize *frame_sz;
	struct hfi_profile_level *profile_level;
	u8 *data_ptr;
	u32 ptype;

	switch (pkt->event_data1) {
	case HFI_EVENT_DATA_SEQUENCE_CHANGED_SUFFICIENT_BUFFER_RESOURCES:
		event.event_type = HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES;
		break;
	case HFI_EVENT_DATA_SEQUENCE_CHANGED_INSUFFICIENT_BUFFER_RESOURCES:
		event.event_type = HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES;
		break;
	default:
		break;
	}

	num_properties_changed = pkt->event_data2;
	if (!num_properties_changed)
		goto done;

	data_ptr = (u8 *) &pkt->ext_event_data[0];
	do {
		ptype = *((u32 *)data_ptr);
		switch (ptype) {
		case HFI_PROPERTY_PARAM_FRAME_SIZE:
			data_ptr += sizeof(u32);
			frame_sz = (struct hfi_framesize *) data_ptr;
			event.width = frame_sz->width;
			event.height = frame_sz->height;
			data_ptr += sizeof(frame_sz);
			dev_dbg(dev, "%s cmd: frame size: %ux%u\n",
				__func__, event.width, event.height);
			break;
		case HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT:
			data_ptr += sizeof(u32);
			profile_level = (struct hfi_profile_level *) data_ptr;
			event.profile = profile_level->profile;
			event.level = profile_level->level;
			data_ptr += sizeof(profile_level);
			dev_dbg(dev, "%s cmd: profile-level: %u - %u\n",
				__func__, event.profile, event.level);
			break;
		default:
			dev_dbg(dev, "%s cmd: %#x not supported\n",
				__func__, ptype);
			break;
		}
		num_properties_changed--;
	} while (num_properties_changed > 0);

done:
	inst->error = HAL_ERR_NONE;
	inst->ops->event_notify(inst, EVT_SYS_EVENT_CHANGE, &event);
}

static void event_release_buffer_ref(struct hfi_device *hfi,
				     struct hfi_device_inst *inst,
				     struct hfi_msg_event_notify_pkt *pkt)
{
	struct hal_cb_event event = {0};
	struct hfi_msg_event_release_buffer_ref_pkt *data;

	data = (struct hfi_msg_event_release_buffer_ref_pkt *)
		pkt->ext_event_data;

	event.event_type = HAL_EVENT_RELEASE_BUFFER_REFERENCE;
	event.packet_buffer = data->packet_buffer;
	event.extradata_buffer = data->extradata_buffer;

	inst->error = HAL_ERR_NONE;
	inst->ops->event_notify(inst, EVT_SYS_EVENT_CHANGE, &event);
}

static void event_sys_error(struct hfi_device *hfi, u32 event)
{
	hfi->core_ops->event_notify(hfi, event);
}

static void event_session_error(struct hfi_device *hfi,
				struct hfi_device_inst *inst,
				struct hfi_msg_event_notify_pkt *pkt)
{
	struct device *dev = hfi->dev;

	dev_err(dev, "session error: event id:%x, session id:%x\n",
		pkt->event_data1, pkt->shdr.session_id);

	if (!inst)
		return;

	switch (pkt->event_data1) {
	/* non fatal session errors */
	case HFI_ERR_SESSION_INVALID_SCALE_FACTOR:
	case HFI_ERR_SESSION_UNSUPPORT_BUFFERTYPE:
	case HFI_ERR_SESSION_UNSUPPORTED_SETTING:
	case HFI_ERR_SESSION_UPSCALE_NOT_SUPPORTED:
		inst->error = HAL_ERR_NONE;
		break;
	default:
		dev_err(dev, "session error: event id:%x, session id:%x\n",
			pkt->event_data1, pkt->shdr.session_id);

		inst->error = to_hal_error(pkt->event_data1);
		inst->ops->event_notify(inst, EVT_SESSION_ERROR, NULL);
		break;
	}
}

static void hfi_event_notify(struct hfi_device *hfi,
			     struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_event_notify_pkt *pkt = packet;

	if (!packet) {
		dev_err(hfi->dev, "invalid packet\n");
		return;
	}

	dev_err(hfi->dev, "%s: recv event id %x\n", __func__, pkt->event_id);

	switch (pkt->event_id) {
	case HFI_EVENT_SYS_ERROR:
		event_sys_error(hfi, EVT_SYS_ERROR);
		break;
	case HFI_EVENT_SESSION_ERROR:
		event_session_error(hfi, inst, pkt);
		break;
	case HFI_EVENT_SESSION_SEQUENCE_CHANGED:
		event_seq_changed(hfi, inst, pkt);
		break;
	case HFI_EVENT_RELEASE_BUFFER_REFERENCE:
		event_release_buffer_ref(hfi, inst, pkt);
		break;
	case HFI_EVENT_SESSION_PROPERTY_CHANGED:
		break;
	default:
		break;
	}
}

struct hfi_max_sessions_supported {
        u32 max_sessions;
};


static void
sys_get_prop_image_version(struct device *dev,
			   struct hfi_msg_sys_property_info_pkt *pkt)
{
	int req_bytes;

	req_bytes = pkt->hdr.size - sizeof(*pkt);

	if (req_bytes < 128 || !pkt->data[1] || pkt->num_properties > 1)
		/* bad packet */
		return;

	dev_err(dev, "F/W version: %s\n", (u8 *)&pkt->data[1]);
}

static void hfi_sys_property_info(struct hfi_device *hfi,
				  struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_sys_property_info_pkt *pkt = packet;
	struct device *dev = hfi->dev;

	if (!pkt->num_properties) {
		dev_dbg(dev, "%s: no properties\n", __func__);
		return;
	}

	switch (pkt->data[0]) {
	case HFI_PROPERTY_SYS_IMAGE_VERSION:
		sys_get_prop_image_version(dev, pkt);
		break;
	default:
		dev_dbg(dev, "%s: unknown property data\n", __func__);
		break;
	}
}

static void hfi_sys_rel_resource_done(struct hfi_device *hfi,
				      struct hfi_device_inst *inst,
				      void *packet)
{
	struct hfi_msg_sys_release_resource_done_pkt *pkt = packet;

	hfi->error = to_hal_error(pkt->error_type);
	complete(&hfi->done);
}

static void hfi_sys_ping_done(struct hfi_device *hfi,
			      struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_sys_ping_ack_pkt *pkt = packet;

	hfi->error = HAL_ERR_NONE;

	if (pkt->client_data != 0xbeef)
		hfi->error = HAL_ERR_FAIL;

	complete(&hfi->done);
}

static void hfi_sys_idle_done(struct hfi_device *hfi,
			      struct hfi_device_inst *inst, void *packet)
{
	dev_dbg(hfi->dev, "sys idle\n");
}

static void hfi_sys_pc_prepare_done(struct hfi_device *hfi,
				    struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_sys_pc_prep_done_pkt *pkt = packet;

	dev_dbg(hfi->dev, "pc prepare done (error %x)\n", pkt->error_type);
}

static void hfi_copy_cap_prop(struct hfi_capability *in,
			      struct hal_session_init_done *init_done)
{
	struct hal_capability *out;

	if (!in || !init_done)
		return;

	switch (in->capability_type) {
	case HFI_CAPABILITY_FRAME_WIDTH:
		out = &init_done->width;
		break;
	case HFI_CAPABILITY_FRAME_HEIGHT:
		out = &init_done->height;
		break;
	case HFI_CAPABILITY_MBS_PER_FRAME:
		out = &init_done->mbs_per_frame;
		break;
	case HFI_CAPABILITY_MBS_PER_SECOND:
		out = &init_done->mbs_per_sec;
		break;
	case HFI_CAPABILITY_FRAMERATE:
		out = &init_done->frame_rate;
		break;
	case HFI_CAPABILITY_SCALE_X:
		out = &init_done->scale_x;
		break;
	case HFI_CAPABILITY_SCALE_Y:
		out = &init_done->scale_y;
		break;
	case HFI_CAPABILITY_BITRATE:
		out = &init_done->bitrate;
		break;
	case HFI_CAPABILITY_HIER_P_NUM_ENH_LAYERS:
		out = &init_done->hier_p;
		break;
	case HFI_CAPABILITY_ENC_LTR_COUNT:
		out = &init_done->ltr_count;
		break;
	case HFI_CAPABILITY_CP_OUTPUT2_THRESH:
		out = &init_done->secure_output2_threshold;
		break;
	default:
		out = NULL;
		break;
	}

	if (out) {
		out->min = in->min;
		out->max = in->max;
		out->step_size = in->step_size;
	}
}

static void
session_get_prop_profile_level(struct hfi_msg_session_property_info_pkt *pkt,
			       struct hal_profile_level *profile_level)
{
	struct hfi_profile_level *hfi;
	u32 req_bytes;

	req_bytes = pkt->shdr.hdr.size - sizeof(*pkt);

	if (!req_bytes || req_bytes % sizeof(struct hfi_profile_level))
		/* bad packet */
		return;

	hfi = (struct hfi_profile_level *)&pkt->data[1];
	profile_level->profile = hfi->profile;
	profile_level->level = hfi->level;
}

static void
session_get_prop_buf_req(struct device *dev,
			 struct hfi_msg_session_property_info_pkt *pkt,
			 struct hal_buffer_requirements *bufreq)
{
	struct hfi_buffer_requirements *buf_req;
	u32 req_bytes;

	req_bytes = pkt->shdr.hdr.size - sizeof(*pkt);

	if (!req_bytes || req_bytes % sizeof(*buf_req) || !pkt->data[1])
		/* bad packet */
		return;

	buf_req = (struct hfi_buffer_requirements *)&pkt->data[1];

	if (!buf_req)
		/* invalid requirements buffer */
		return;

	while (req_bytes) {
		if (buf_req->size && buf_req->count_min > buf_req->count_actual)
			dev_warn(dev, "bad requirements buffer\n");

		dev_dbg(dev, "got buffer requirements for: %x\n",
			buf_req->type);

		switch (buf_req->type) {
		case HFI_BUFFER_INPUT:
			memcpy(&bufreq[0], buf_req, sizeof(*buf_req));
			bufreq[0].type = HAL_BUFFER_INPUT;
			break;
		case HFI_BUFFER_OUTPUT:
			memcpy(&bufreq[1], buf_req, sizeof(*buf_req));
			bufreq[1].type = HAL_BUFFER_OUTPUT;
			break;
		case HFI_BUFFER_OUTPUT2:
			memcpy(&bufreq[2], buf_req, sizeof(*buf_req));
			bufreq[2].type = HAL_BUFFER_OUTPUT2;
			break;
		case HFI_BUFFER_EXTRADATA_INPUT:
			memcpy(&bufreq[3], buf_req, sizeof(*buf_req));
			bufreq[3].type = HAL_BUFFER_EXTRADATA_INPUT;
			break;
		case HFI_BUFFER_EXTRADATA_OUTPUT:
			memcpy(&bufreq[4], buf_req, sizeof(*buf_req));
			bufreq[4].type = HAL_BUFFER_EXTRADATA_OUTPUT;
			break;
		case HFI_BUFFER_EXTRADATA_OUTPUT2:
			memcpy(&bufreq[5], buf_req, sizeof(*buf_req));
			bufreq[5].type = HAL_BUFFER_EXTRADATA_OUTPUT2;
			break;
		case HFI_BUFFER_INTERNAL_SCRATCH:
			memcpy(&bufreq[6], buf_req, sizeof(*buf_req));
			bufreq[6].type = HAL_BUFFER_INTERNAL_SCRATCH;
			break;
		case HFI_BUFFER_INTERNAL_SCRATCH_1:
			memcpy(&bufreq[7], buf_req, sizeof(*buf_req));
			bufreq[7].type = HAL_BUFFER_INTERNAL_SCRATCH_1;
			break;
		case HFI_BUFFER_INTERNAL_SCRATCH_2:
			memcpy(&bufreq[8], buf_req, sizeof(*buf_req));
			bufreq[8].type = HAL_BUFFER_INTERNAL_SCRATCH_2;
			break;
		case HFI_BUFFER_INTERNAL_PERSIST:
			memcpy(&bufreq[9], buf_req, sizeof(*buf_req));
			bufreq[9].type = HAL_BUFFER_INTERNAL_PERSIST;
			break;
		case HFI_BUFFER_INTERNAL_PERSIST_1:
			memcpy(&bufreq[10], buf_req, sizeof(*buf_req));
			bufreq[10].type = HAL_BUFFER_INTERNAL_PERSIST_1;
			break;
		default:
			/* bad buffer type */
			break;
		}

		req_bytes -= sizeof(struct hfi_buffer_requirements);
		buf_req++;
	}
}

static void
session_get_prop_vdec_entropy(struct hfi_msg_session_property_info_pkt *pkt,
			      u32 *entropy)
{
	u32 req_bytes;

	req_bytes = pkt->shdr.hdr.size - sizeof(*pkt);

	if (!req_bytes || req_bytes % sizeof(*entropy) || !pkt->data[1])
		/* bad packet */
		return;

	*entropy = pkt->data[1];
}

static void hfi_session_prop_info(struct hfi_device *hfi,
				  struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_session_property_info_pkt *pkt = packet;
	struct device *dev = hfi->dev;
	union hal_get_property *hprop = &inst->hprop;
	u32 dec_entropy;

	inst->error = HAL_ERR_NONE;

	if (!pkt->num_properties) {
		inst->error = HAL_ERR_BAD_PARAM;
		dev_err(dev, "%s: no properties\n", __func__);
		goto done;
	}

	switch (pkt->data[0]) {
	case HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS:
		memset(hprop->bufreq, 0, sizeof(hprop->bufreq));
		session_get_prop_buf_req(dev, pkt, hprop->bufreq);
		break;
	case HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT:
		memset(&hprop->profile_level, 0, sizeof(hprop->profile_level));
		session_get_prop_profile_level(pkt, &hprop->profile_level);
		break;
	case HFI_PROPERTY_CONFIG_VDEC_ENTROPY:
		session_get_prop_vdec_entropy(pkt, &dec_entropy);
		break;
	default:
		dev_dbg(dev, "%s: unknown property id:%x\n", __func__,
			pkt->data[0]);
		return;
	}

done:
	complete(&inst->done);
}

static enum hal_error
session_init_done_read_prop(struct device *dev,
			    u32 rem_bytes, u32 num_props, u8 *data,
			    struct hal_session_init_done *init_done)
{
	u32 ptype, next_offset = 0;
	enum hal_error error = 0;
	u32 prop_count = 0;

	while (error == HAL_ERR_NONE && num_props && rem_bytes >= sizeof(u32)) {
		ptype = *((u32 *)data);
		next_offset = sizeof(u32);

		switch (ptype) {
		case HFI_PROPERTY_PARAM_CAPABILITY_SUPPORTED: {
			struct hfi_capabilities *caps;
			struct hfi_capability *cap;
			u32 num_caps;

			if ((rem_bytes - next_offset) < sizeof(*cap)) {
				error = HAL_ERR_BAD_PARAM;
				break;
			}

			caps = (struct hfi_capabilities *)(data + next_offset);

			num_caps = caps->num_capabilities;
			cap = &caps->data[0];
			next_offset += sizeof(u32);

			while (num_caps &&
			      (rem_bytes - next_offset) >= sizeof(u32)) {
				hfi_copy_cap_prop(cap, init_done);
				cap++;
				next_offset += sizeof(*cap);
				num_caps--;
			}
			num_props--;
			break;
		}
		case HFI_PROPERTY_PARAM_UNCOMPRESSED_FORMAT_SUPPORTED: {
			struct hfi_uncompressed_format_supported *prop =
				(struct hfi_uncompressed_format_supported *)
				(data + next_offset);
			u32 num_fmt_entries;
			u8 *fmt;
			struct hfi_uncompressed_plane_info *inf;

			if ((rem_bytes - next_offset) < sizeof(*prop)) {
				error = HAL_ERR_BAD_PARAM;
				break;
			}

			num_fmt_entries = prop->format_entries;
			next_offset = sizeof(*prop) - sizeof(u32);
			fmt = (u8 *)&prop->format_info[0];

			dev_dbg(dev, "uncomm format support num_fmt_entries:%u\n",
				num_fmt_entries);

			while (num_fmt_entries) {
				struct hfi_uncompressed_plane_constraints *cnts;
				u32 bytes_to_skip;
				inf = (struct hfi_uncompressed_plane_info *)fmt;

				if ((rem_bytes - next_offset) < sizeof(*inf)) {
					error = HAL_ERR_BAD_PARAM;
					break;
				}

				dev_dbg(dev, "plane info: format:%x, num_planes:%x\n",
					inf->format, inf->num_planes);

				cnts = &inf->plane_format[0];
				dev_dbg(dev, "%u %u %u %u\n",
					cnts->stride_multiples,
					cnts->max_stride,
					cnts->min_plane_buffer_height_multiple,
					cnts->buffer_alignment);

				bytes_to_skip = sizeof(*inf) - sizeof(*cnts) +
						inf->num_planes * sizeof(*cnts);

				fmt += bytes_to_skip;
				next_offset += bytes_to_skip;
				num_fmt_entries--;
			}
			num_props--;
			break;
		}
		case HFI_PROPERTY_PARAM_PROPERTIES_SUPPORTED: {
			struct hfi_properties_supported *prop =
				(struct hfi_properties_supported *)
				(data + next_offset);

			next_offset += sizeof(*prop) - sizeof(u32)
					+ prop->num_properties * sizeof(u32);
			num_props--;
			break;
		}
		case HFI_PROPERTY_PARAM_PROFILE_LEVEL_SUPPORTED: {
			struct hfi_profile_level *prop_level;
			struct hfi_profile_level_supported *prop =
				(struct hfi_profile_level_supported *)
				(data + next_offset);
			struct hal_profile_level_supported *hal;
			int count = 0;
			u8 *ptr;

			ptr = (u8 *)&prop->profile_level[0];
			prop_count = prop->profile_count;

			if (prop_count > MAX_PROFILE_COUNT)
				prop_count = MAX_PROFILE_COUNT;

			hal = &init_done->profile_level;

			while (prop_count) {
				ptr++;
				prop_level = (struct hfi_profile_level *)ptr;

				hal->profile_level[count].profile =
					prop_level->profile;
				hal->profile_level[count].level =
					prop_level->level;
				prop_count--;
				count++;
				ptr += sizeof(*prop_level) / sizeof(u32);
			}

			next_offset += sizeof(*prop) - sizeof(*prop_level) +
				prop->profile_count * sizeof(*prop_level);

			num_props--;
			break;
		}
		case HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SUPPORTED: {
			struct hfi_nal_stream_format *nal =
				(struct hfi_nal_stream_format *)(data + next_offset);
			dev_dbg(dev, "NAL format: %x\n", nal->format);
			next_offset += sizeof(*nal);
			num_props--;
			break;
		}
		case HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SELECT: {
			next_offset += sizeof(u32);
			num_props--;
			break;
		}
		case HFI_PROPERTY_PARAM_MAX_SEQUENCE_HEADER_SIZE: {
			u32 *max_seq_sz = (u32 *)(data + next_offset);

			dev_dbg(dev, "max seq header sz: %x\n", *max_seq_sz);
			next_offset += sizeof(u32);
			num_props--;
			break;
		}
		case HFI_PROPERTY_PARAM_VENC_INTRA_REFRESH: {
			next_offset += sizeof(struct hfi_intra_refresh);
			num_props--;
			break;
		}
		case HFI_PROPERTY_PARAM_BUFFER_ALLOC_MODE_SUPPORTED: {
			struct hfi_buffer_alloc_mode_supported *prop =
				(struct hfi_buffer_alloc_mode_supported *)
				(data + next_offset);
			int i;

			if (prop->buffer_type == HFI_BUFFER_OUTPUT ||
			    prop->buffer_type == HFI_BUFFER_OUTPUT2) {
				init_done->alloc_mode_out = 0;

				for (i = 0; i < prop->num_entries; i++) {
					switch (prop->data[i]) {
					case HFI_BUFFER_MODE_STATIC:
						init_done->alloc_mode_out
						|= HAL_BUFFER_MODE_STATIC;
						break;
					case HFI_BUFFER_MODE_DYNAMIC:
						init_done->alloc_mode_out
						|= HAL_BUFFER_MODE_DYNAMIC;
						break;
					}

					if (i >= 32) {
						dev_err(dev,
						"%s - num_entries: %d from f/w seems suspect\n",
						__func__, prop->num_entries);
						break;
					}
				}
			}
			next_offset += sizeof(*prop) -
				sizeof(u32) + prop->num_entries * sizeof(u32);
			num_props--;
			dev_dbg(dev, "alloc_mode_out:%x\n",
				init_done->alloc_mode_out);
			break;
		}
		default:
			dev_dbg(dev, "%s: default case %#x\n", __func__, ptype);
			break;
		}

		rem_bytes -= next_offset;
		data += next_offset;
	}

	return error;
}

static void hfi_sys_init_done(struct hfi_device *hfi,
			      struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_sys_init_done_pkt *pkt = packet;
	u32 enc_codecs = 0, dec_codecs = 0;
	u32 rem_bytes, read_bytes = 0, num_properties;
	struct hal_session_init_done *cap = &inst->caps;
	enum hal_error error;
	u8 *data_ptr;
	u32 ptype;

	dev_err(hfi->dev, "%s: enter\n", __func__);

	error = to_hal_error(pkt->error_type);
	if (error != HAL_ERR_NONE)
		goto err_no_prop;

	num_properties = pkt->num_properties;
	if (!num_properties) {
		error = HAL_ERR_FAIL;
		goto err_no_prop;
	}

	rem_bytes = pkt->hdr.size - sizeof(*pkt) + sizeof(u32);
	if (!rem_bytes) {
		/* missing property data */
		error = HAL_ERR_FAIL;
		goto err_no_prop;
	}

	data_ptr = (u8 *)&pkt->data[0];

	ptype = *((u32 *)data_ptr);
	data_ptr += sizeof(u32);

	dev_err(hfi->dev, "%s: ptype:%x, num_properties: %d\n", __func__,
		ptype, num_properties);

	switch (ptype) {
	case HFI_PROPERTY_PARAM_CODEC_SUPPORTED: {
		struct hfi_codec_supported *prop;

		prop = (struct hfi_codec_supported *)data_ptr;

		if (rem_bytes < sizeof(*prop)) {
			error = HAL_ERR_BAD_PARAM;
			break;
		}

		dec_codecs = prop->dec_codecs;
		enc_codecs = prop->enc_codecs;
		read_bytes += sizeof(*prop) + sizeof(u32);
		data_ptr += sizeof(*prop);

		break;
	}
	default:
		/* bad property id */
		error = HAL_ERR_BAD_PARAM;
		break;
	}

	ptype = *((u32 *)data_ptr);

	if (ptype == HFI_PROPERTY_PARAM_MAX_SESSIONS_SUPPORTED) {
		struct hfi_max_sessions_supported *prop1 =
			(struct hfi_max_sessions_supported *)
			(data_ptr + sizeof(u32));

		data_ptr += sizeof(u32);

		// Fill the property value here in to some variable later
                read_bytes += sizeof(struct hfi_max_sessions_supported) +
				sizeof(u32);
		data_ptr += sizeof(struct hfi_max_sessions_supported);
	}

	num_properties--;
	rem_bytes -= read_bytes;

	hfi->enc_codecs = enc_codecs;
	hfi->dec_codecs = dec_codecs;

	if (hfi->hfi_type == VIDC_VENUS &&
	   (hfi->dec_codecs & HAL_VIDEO_CODEC_H264))
		hfi->dec_codecs |= HAL_VIDEO_CODEC_MVC;

	goto err_no_prop;

	error = session_init_done_read_prop(hfi->dev, rem_bytes, num_properties,
					    data_ptr, cap);

err_no_prop:
	dev_err(hfi->dev, "%s: exit (error: %x)\n", __func__, error);
	hfi->error = error;
	complete(&hfi->done);
}

static void hfi_session_init_done(struct hfi_device *hfi,
				  struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_session_init_done_pkt *pkt = packet;
	struct hal_session_init_done *cap = &inst->caps;
	enum hal_error error;
	u32 rem_bytes, num_props;
	u8 *data;

	error = to_hal_error(pkt->error_type);
	if (error != HAL_ERR_NONE)
		goto done;

	memset(cap, 0, sizeof(*cap));

        rem_bytes = pkt->shdr.hdr.size - sizeof(*pkt) + sizeof(u32);
        if (!rem_bytes) {
                dev_err(hfi->dev, "%s: missing property info\n", __func__);
                goto done;
        }

        data = (u8 *) &pkt->data[0];
        num_props = pkt->num_properties;

	dev_dbg(hfi->dev, "%s: num_props %d\n", __func__, num_props);

	error = session_init_done_read_prop(hfi->dev, rem_bytes, num_props,
					    data, cap);
	if (error != HAL_ERR_NONE)
		goto done;

	dev_err(hfi->dev,
		"width %u-%u (%u), height %u-%d (%u), framerate %u-%u (%u)\n",
		cap->width.min, cap->width.max, cap->width.step_size,
		cap->height.min, cap->height.max, cap->height.step_size,
		cap->frame_rate.min, cap->frame_rate.max,
		cap->frame_rate.step_size);

	dev_err(hfi->dev,
		"scalex %u-%u (%u), scaley %u-%d (%u), mbsperframe %u-%u (%u)\n",
		cap->scale_x.min, cap->scale_x.max, cap->scale_x.step_size,
		cap->scale_y.min, cap->scale_y.max, cap->scale_y.step_size,
		cap->mbs_per_frame.min, cap->mbs_per_frame.max,
		cap->mbs_per_frame.step_size);

done:
	inst->error = error;
	complete(&inst->done);
}

static void hfi_session_load_res_done(struct hfi_device *hfi,
				      struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_session_load_resources_done_pkt *pkt = packet;

	inst->error = to_hal_error(pkt->error_type);
	complete(&inst->done);
}

static void hfi_session_flush_done(struct hfi_device *hfi,
				   struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_session_flush_done_pkt *pkt = packet;

	inst->error = to_hal_error(pkt->error_type);
	complete(&inst->done);
}

static void hfi_session_etb_done(struct hfi_device *hfi,
				 struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_session_empty_buffer_done_pkt *pkt = packet;
	u32 flags = 0;

	printk(KERN_ALERT "\n hfi_session_etb_done");
	inst->error = to_hal_error(pkt->error_type);

	if (inst->error == HAL_ERR_NOT_SUPPORTED)
		flags |= HAL_ERR_NOT_SUPPORTED;
	if (inst->error == HAL_ERR_BITSTREAM_ERR)
		flags |= HAL_ERR_BITSTREAM_ERR;
	if (inst->error == HAL_ERR_START_CODE_NOT_FOUND)
		flags |= HAL_ERR_START_CODE_NOT_FOUND;

	inst->ops->empty_buf_done(inst, pkt->input_tag, pkt->filled_len,
				  pkt->offset, flags);
}

static void hfi_session_ftb_done(struct hfi_device *hfi,
				 struct hfi_device_inst *inst, void *packet)
{
	enum hal_session_type session_type = inst->session_type;
	struct hal_fbd fbd = {0};
	struct timeval timestamp;
	int64_t time_usec = 0;
	unsigned int error;
	u32 flags = 0;

	printk(KERN_ALERT "\n hfi_session_ftb_done");
	if (session_type == HAL_VIDEO_SESSION_TYPE_ENCODER) {
		struct hfi_msg_session_fbd_compressed_pkt *pkt = packet;

		fbd.timestamp_hi = pkt->time_stamp_hi;
		fbd.timestamp_lo = pkt->time_stamp_lo;
		fbd.flags1 = pkt->flags;
		fbd.offset1 = pkt->offset;
		fbd.alloc_len1 = pkt->alloc_len;
		fbd.filled_len1 = pkt->filled_len;
		fbd.picture_type = pkt->picture_type;
		fbd.packet_buffer1 = pkt->packet_buffer;
		fbd.extradata_buffer = pkt->extradata_buffer;
		fbd.buffer_type = HAL_BUFFER_OUTPUT;

		error = to_hal_error(pkt->error_type);
	} else if (session_type == HAL_VIDEO_SESSION_TYPE_DECODER) {
		struct hfi_msg_session_fbd_uncompressed_plane0_pkt *pkt =
			packet;

		fbd.timestamp_hi = pkt->time_stamp_hi;
		fbd.timestamp_lo = pkt->time_stamp_lo;
		fbd.flags1 = pkt->flags;
		fbd.offset1 = pkt->offset;
		fbd.alloc_len1 = pkt->alloc_len;
		fbd.filled_len1 = pkt->filled_len;
		fbd.picture_type = pkt->picture_type;
		fbd.packet_buffer1 = pkt->packet_buffer;
		fbd.extradata_buffer = pkt->extradata_buffer;

		if (pkt->stream_id == 0)
			fbd.buffer_type = HAL_BUFFER_OUTPUT;
		else if (pkt->stream_id == 1)
			fbd.buffer_type = HAL_BUFFER_OUTPUT2;

		error = to_hal_error(pkt->error_type);
	} else {
		error = HAL_ERR_BAD_PARAM;
	}

	if (fbd.buffer_type != HAL_BUFFER_OUTPUT)
		return;

#if 0
	if (fbd.flags1 & HAL_BUFFERFLAG_READONLY)
		flags |= V4L2_QCOM_BUF_FLAG_READONLY;
	if (fbd.flags1 & HAL_BUFFERFLAG_EOS)
		flags |= V4L2_QCOM_BUF_FLAG_EOS;
	if (fbd.flags1 & HAL_BUFFERFLAG_CODECCONFIG)
		flags &= ~V4L2_QCOM_BUF_FLAG_CODECCONFIG;
	if (fbd.flags1 & HAL_BUFFERFLAG_SYNCFRAME)
		flags |= V4L2_QCOM_BUF_FLAG_IDRFRAME;
	if (fbd.flags1 & HAL_BUFFERFLAG_EOSEQ)
		flags |= V4L2_QCOM_BUF_FLAG_EOSEQ;
	if (fbd.flags1 & HAL_BUFFERFLAG_DECODEONLY)
		flags |= V4L2_QCOM_BUF_FLAG_DECODEONLY;
	if (fbd.flags1 & HAL_BUFFERFLAG_DATACORRUPT)
		flags |= V4L2_QCOM_BUF_DATA_CORRUPT;
	if (fbd.flags1 & HAL_BUFFERFLAG_DROP_FRAME)
		flags |= V4L2_QCOM_BUF_DROP_FRAME;
	if (fbd.flags1 & HAL_BUFFERFLAG_MBAFF)
		flags |= V4L2_MSM_BUF_FLAG_MBAFF;
	if (fbd.flags1 & HAL_BUFFERFLAG_TS_DISCONTINUITY)
		flags |= V4L2_QCOM_BUF_TS_DISCONTINUITY;
	if (fbd.flags1 & HAL_BUFFERFLAG_TS_ERROR)
		flags |= V4L2_QCOM_BUF_TS_ERROR;
#endif
	if (fbd.flags1 & HAL_BUFFERFLAG_EOS)
		flags |= V4L2_BUF_FLAG_LAST;

	switch (fbd.picture_type) {
	case HAL_PICTURE_IDR:
		flags |= V4L2_BUF_FLAG_KEYFRAME;
		break;
	case HAL_PICTURE_I:
		flags |= V4L2_BUF_FLAG_KEYFRAME;
		break;
	case HAL_PICTURE_P:
		flags |= V4L2_BUF_FLAG_PFRAME;
		break;
	case HAL_PICTURE_B:
		flags |= V4L2_BUF_FLAG_BFRAME;
		break;
	case HAL_FRAME_NOTCODED:
	case HAL_UNUSED_PICT:
	case HAL_FRAME_YUV:
		break;
	default:
		break;
	}

	if (!(fbd.flags1 & HAL_BUFFERFLAG_TIMESTAMPINVALID) &&
	      fbd.filled_len1) {
		time_usec = fbd.timestamp_hi;
		time_usec = (time_usec << 32) | fbd.timestamp_lo;
	}

	timestamp = ns_to_timeval(time_usec * NSEC_PER_USEC);

	inst->error = error;
	inst->ops->fill_buf_done(inst, fbd.packet_buffer1, fbd.filled_len1,
				 fbd.offset1, flags, &timestamp);
}

static void hfi_session_start_done(struct hfi_device *hfi,
				   struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_session_start_done_pkt *pkt = packet;

	inst->error = to_hal_error(pkt->error_type);
	complete(&inst->done);
}

static void hfi_session_stop_done(struct hfi_device *hfi,
				  struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_session_stop_done_pkt *pkt = packet;

	inst->error = to_hal_error(pkt->error_type);
	complete(&inst->done);
}

static void hfi_session_rel_res_done(struct hfi_device *hfi,
				     struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_session_release_resources_done_pkt *pkt = packet;

	inst->error = to_hal_error(pkt->error_type);
	complete(&inst->done);
}

static void hfi_session_rel_buf_done(struct hfi_device *hfi,
				     struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_session_release_buffers_done_pkt *pkt = packet;

	/*
	 * the address of the released buffer can be extracted:
	 * if (pkt->buffer_info) {
	 *	cmd.data = &pkt->buffer_info;
	 *	cmd.size = sizeof(struct hfi_buffer_info);
	 * }
	 */
	inst->error = to_hal_error(pkt->error_type);
	complete(&inst->done);
}

static void hfi_session_end_done(struct hfi_device *hfi,
				 struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_session_end_done_pkt *pkt = packet;

	inst->error = to_hal_error(pkt->error_type);
	complete(&inst->done);
}

static void hfi_session_abort_done(struct hfi_device *hfi,
				   struct hfi_device_inst *inst, void *packet)
{
	struct hfi_msg_sys_session_abort_done_pkt *pkt = packet;

	inst->error = to_hal_error(pkt->error_type);
	complete(&inst->done);
}

static void hfi_session_get_seq_hdr_done(struct hfi_device *hfi,
					 struct hfi_device_inst *inst,
					 void *packet)
{
	struct hfi_msg_session_get_sequence_hdr_done_pkt *pkt = packet;

	/*
	 * output_done.packet_buffer1 = pkt->sequence_header;
	 * output_done.filled_len1 = pkt->header_len;
	 */

	inst->error = to_hal_error(pkt->error_type);
	complete(&inst->done);
}

struct hfi_done_handler {
	u32 pkt;
	u32 pkt_sz;
	u32 pkt_sz2;
	void (*done)(struct hfi_device *, struct hfi_device_inst *, void *);
	bool is_sys_pkt;
};

static const struct hfi_done_handler handlers[] = {
	{.pkt = HFI_MSG_EVENT_NOTIFY,
	 .pkt_sz = sizeof(struct hfi_msg_event_notify_pkt),
	 .done = hfi_event_notify,
	},
	{.pkt = HFI_MSG_SYS_INIT,
	 .pkt_sz = sizeof(struct hfi_msg_sys_init_done_pkt),
	 .done = hfi_sys_init_done,
	 .is_sys_pkt = true,
	},
	{.pkt = HFI_MSG_SYS_PROPERTY_INFO,
	 .pkt_sz = sizeof(struct hfi_msg_sys_property_info_pkt),
	 .done = hfi_sys_property_info,
	 .is_sys_pkt = true,
	},
	{.pkt = HFI_MSG_SYS_RELEASE_RESOURCE,
	 .pkt_sz = sizeof(struct hfi_msg_sys_release_resource_done_pkt),
	 .done = hfi_sys_rel_resource_done,
	 .is_sys_pkt = true,
	},
	{.pkt = HFI_MSG_SYS_PING_ACK,
	 .pkt_sz = sizeof(struct hfi_msg_sys_ping_ack_pkt),
	 .done = hfi_sys_ping_done,
	 .is_sys_pkt = true,
	},
	{.pkt = HFI_MSG_SYS_IDLE,
	 .pkt_sz = sizeof(struct hfi_msg_sys_idle_pkt),
	 .done = hfi_sys_idle_done,
	 .is_sys_pkt = true,
	},
	{.pkt = HFI_MSG_SYS_PC_PREP,
	 .pkt_sz = sizeof(struct hfi_msg_sys_pc_prep_done_pkt),
	 .done = hfi_sys_pc_prepare_done,
	 .is_sys_pkt = true,
	},
	{.pkt = HFI_MSG_SYS_SESSION_INIT,
	 .pkt_sz = sizeof(struct hfi_msg_session_init_done_pkt),
	 .done = hfi_session_init_done,
	},
	{.pkt = HFI_MSG_SYS_SESSION_END,
	 .pkt_sz = sizeof(struct hfi_msg_session_end_done_pkt),
	 .done = hfi_session_end_done,
	},
	{.pkt = HFI_MSG_SESSION_LOAD_RESOURCES,
	 .pkt_sz = sizeof(struct hfi_msg_session_load_resources_done_pkt),
	 .done = hfi_session_load_res_done,
	},
	{.pkt = HFI_MSG_SESSION_START,
	 .pkt_sz = sizeof(struct hfi_msg_session_start_done_pkt),
	 .done = hfi_session_start_done,
	},
	{.pkt = HFI_MSG_SESSION_STOP,
	 .pkt_sz = sizeof(struct hfi_msg_session_stop_done_pkt),
	 .done = hfi_session_stop_done,
	},
	{.pkt = HFI_MSG_SYS_SESSION_ABORT,
	 .pkt_sz = sizeof(struct hfi_msg_sys_session_abort_done_pkt),
	 .done = hfi_session_abort_done,
	},
	{.pkt = HFI_MSG_SESSION_EMPTY_BUFFER,
	 .pkt_sz = sizeof(struct hfi_msg_session_empty_buffer_done_pkt),
	 .done = hfi_session_etb_done,
	},
	{.pkt = HFI_MSG_SESSION_FILL_BUFFER,
	 .pkt_sz = sizeof(struct hfi_msg_session_fbd_uncompressed_plane0_pkt),
	 .pkt_sz2 = sizeof(struct hfi_msg_session_fbd_compressed_pkt),
	 .done = hfi_session_ftb_done,
	},
	{.pkt = HFI_MSG_SESSION_FLUSH,
	 .pkt_sz = sizeof(struct hfi_msg_session_flush_done_pkt),
	 .done = hfi_session_flush_done,
	},
	{.pkt = HFI_MSG_SESSION_PROPERTY_INFO,
	 .pkt_sz = sizeof(struct hfi_msg_session_property_info_pkt),
	 .done = hfi_session_prop_info,
	},
	{.pkt = HFI_MSG_SESSION_RELEASE_RESOURCES,
	 .pkt_sz = sizeof(struct hfi_msg_session_release_resources_done_pkt),
	 .done = hfi_session_rel_res_done,
	},
	{.pkt = HFI_MSG_SESSION_GET_SEQUENCE_HEADER,
	 .pkt_sz = sizeof(struct hfi_msg_session_get_sequence_hdr_done_pkt),
	 .done = hfi_session_get_seq_hdr_done,
	},
	{.pkt = HFI_MSG_SESSION_RELEASE_BUFFERS,
	 .pkt_sz = sizeof(struct hfi_msg_session_release_buffers_done_pkt),
	 .done = hfi_session_rel_buf_done,
	},
};

void hfi_process_watchdog_timeout(struct hfi_device *hfi)
{
	event_sys_error(hfi, EVT_SYS_WATCHDOG_TIMEOUT);
}

u32 hfi_process_msg_packet(struct hfi_device *hfi, struct hfi_pkt_hdr *hdr)
{
	const struct hfi_done_handler *handler;
	struct hfi_device_inst *inst;
	struct device *dev = hfi->dev;
	unsigned int i;
	bool found = false;

	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		handler = &handlers[i];

		if (handler->pkt != hdr->pkt_type)
			continue;

		found = true;
		break;
	}

	if (found == false)
		return hdr->pkt_type;

	if (hdr->size && hdr->size < handler->pkt_sz &&
	    hdr->size < handler->pkt_sz2) {
		dev_err(dev, "bad packet size (%d should be %d, pkt type:%x)\n",
			hdr->size, handler->pkt_sz, hdr->pkt_type);

		return hdr->pkt_type;
	}

	if (handler->is_sys_pkt) {
		inst = NULL;
	} else {
		struct hfi_session_pkt *pkt;

		pkt = (struct hfi_session_pkt *)hdr;
		inst = to_hfi_instance(hfi, pkt->shdr.session_id);

		if (!inst)
			dev_warn(dev, "no valid instance(pkt session_id:%x)\n",
				 pkt->shdr.session_id);

		/*
		 * Event of type HFI_EVENT_SYS_ERROR will not have any session
		 * associated with it
		 */
		if (!inst && hdr->pkt_type != HFI_MSG_EVENT_NOTIFY) {
			dev_err(dev, "got invalid session id:%d\n",
				pkt->shdr.session_id);
			goto invalid_session;
		}
	}

	handler->done(hfi, inst, hdr);

invalid_session:
	return hdr->pkt_type;
}
