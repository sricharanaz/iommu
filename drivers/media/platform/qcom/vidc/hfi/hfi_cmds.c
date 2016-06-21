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
#include <linux/errno.h>
#include <linux/log2.h>
#include <linux/hash.h>

#include "hfi_cmds.h"

/*
 * Set up look-up tables to convert HAL_* to HFI_*.
 * The tables below mostly take advantage of the fact that most
 * HAL_* types are defined bitwise. So if we index them normally
 * when declaring the tables, we end up with huge arrays with wasted
 * space.  So before indexing them, we apply log2 to use a more
 * sensible index.
 */
static const u32 profile_table[] = {
	[ilog2(HAL_H264_PROFILE_BASELINE)] = HFI_H264_PROFILE_BASELINE,
	[ilog2(HAL_H264_PROFILE_MAIN)] = HFI_H264_PROFILE_MAIN,
	[ilog2(HAL_H264_PROFILE_HIGH)] = HFI_H264_PROFILE_HIGH,
	[ilog2(HAL_H264_PROFILE_CONSTRAINED_BASE)] =
		HFI_H264_PROFILE_CONSTRAINED_BASE,
	[ilog2(HAL_H264_PROFILE_CONSTRAINED_HIGH)] =
		HFI_H264_PROFILE_CONSTRAINED_HIGH,
	[ilog2(HAL_VPX_PROFILE_VERSION_1)] = HFI_VPX_PROFILE_VERSION_1,
	[ilog2(HAL_MVC_PROFILE_STEREO_HIGH)] = HFI_H264_PROFILE_STEREO_HIGH,
};

static const u32 entropy_mode[] = {
	[ilog2(HAL_H264_ENTROPY_CAVLC)] = HFI_H264_ENTROPY_CAVLC,
	[ilog2(HAL_H264_ENTROPY_CABAC)] = HFI_H264_ENTROPY_CABAC,
};

static const u32 cabac_model[] = {
	[ilog2(HAL_H264_CABAC_MODEL_0)] = HFI_H264_CABAC_MODEL_0,
	[ilog2(HAL_H264_CABAC_MODEL_1)] = HFI_H264_CABAC_MODEL_1,
	[ilog2(HAL_H264_CABAC_MODEL_2)] = HFI_H264_CABAC_MODEL_2,
};

static const u32 color_format[] = {
	[ilog2(HAL_COLOR_FORMAT_MONOCHROME)] = HFI_COLOR_FORMAT_MONOCHROME,
	[ilog2(HAL_COLOR_FORMAT_NV12)] = HFI_COLOR_FORMAT_NV12,
	[ilog2(HAL_COLOR_FORMAT_NV21)] = HFI_COLOR_FORMAT_NV21,
	[ilog2(HAL_COLOR_FORMAT_NV12_4x4TILE)] = HFI_COLOR_FORMAT_NV12_4x4TILE,
	[ilog2(HAL_COLOR_FORMAT_NV21_4x4TILE)] = HFI_COLOR_FORMAT_NV21_4x4TILE,
	[ilog2(HAL_COLOR_FORMAT_YUYV)] = HFI_COLOR_FORMAT_YUYV,
	[ilog2(HAL_COLOR_FORMAT_YVYU)] = HFI_COLOR_FORMAT_YVYU,
	[ilog2(HAL_COLOR_FORMAT_UYVY)] = HFI_COLOR_FORMAT_UYVY,
	[ilog2(HAL_COLOR_FORMAT_VYUY)] = HFI_COLOR_FORMAT_VYUY,
	[ilog2(HAL_COLOR_FORMAT_RGB565)] = HFI_COLOR_FORMAT_RGB565,
	[ilog2(HAL_COLOR_FORMAT_BGR565)] = HFI_COLOR_FORMAT_BGR565,
	[ilog2(HAL_COLOR_FORMAT_RGB888)] = HFI_COLOR_FORMAT_RGB888,
	[ilog2(HAL_COLOR_FORMAT_BGR888)] = HFI_COLOR_FORMAT_BGR888,
	/* UBWC Color formats*/
	[ilog2(HAL_COLOR_FORMAT_NV12_UBWC)] = HFI_COLOR_FORMAT_NV12_UBWC,
	[ilog2(HAL_COLOR_FORMAT_NV12_TP10_UBWC)] =
			HFI_COLOR_FORMAT_YUV420_TP10_UBWC,
};

static const u32 nal_type[] = {
	[ilog2(HAL_NAL_FORMAT_STARTCODES)] = HFI_NAL_FORMAT_STARTCODES,
	[ilog2(HAL_NAL_FORMAT_ONE_NAL_PER_BUFFER)] =
		HFI_NAL_FORMAT_ONE_NAL_PER_BUFFER,
	[ilog2(HAL_NAL_FORMAT_ONE_BYTE_LENGTH)] =
		HFI_NAL_FORMAT_ONE_BYTE_LENGTH,
	[ilog2(HAL_NAL_FORMAT_TWO_BYTE_LENGTH)] =
		HFI_NAL_FORMAT_TWO_BYTE_LENGTH,
	[ilog2(HAL_NAL_FORMAT_FOUR_BYTE_LENGTH)] =
		HFI_NAL_FORMAT_FOUR_BYTE_LENGTH,
};

static inline u32 to_hfi_type(u32 property, u32 hal_type)
{
	if (hal_type && roundup_pow_of_two(hal_type) != hal_type)
		/* Not a power of 2, it's not going to be in any of the
		 * tables anyway
		 */
		return -EINVAL;

	if (hal_type)
		hal_type = ilog2(hal_type);

	switch (property) {
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
		return (hal_type >= ARRAY_SIZE(profile_table)) ? -ENOTSUPP :
				profile_table[hal_type];
	case HAL_PARAM_VENC_H264_ENTROPY_CONTROL:
		return (hal_type >= ARRAY_SIZE(entropy_mode)) ? -ENOTSUPP :
				entropy_mode[hal_type];
	case HAL_PARAM_VENC_H264_ENTROPY_CABAC_MODEL:
		return (hal_type >= ARRAY_SIZE(cabac_model)) ? -ENOTSUPP :
				cabac_model[hal_type];
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT:
		return (hal_type >= ARRAY_SIZE(color_format)) ? -ENOTSUPP :
				color_format[hal_type];
	case HAL_PARAM_NAL_STREAM_FORMAT_SELECT:
		return (hal_type >= ARRAY_SIZE(nal_type)) ? -ENOTSUPP :
				nal_type[hal_type];
	default:
		return -ENOTSUPP;
	}
}

static inline u32 to_hfi_layout(enum hal_buffer_layout_type hal_buf_layout)
{
	switch (hal_buf_layout) {
	case HAL_BUFFER_LAYOUT_TOP_BOTTOM:
		return HFI_MVC_BUFFER_LAYOUT_TOP_BOTTOM;
	case HAL_BUFFER_LAYOUT_SEQ:
		return HFI_MVC_BUFFER_LAYOUT_SEQ;
	default:
		return HFI_MVC_BUFFER_LAYOUT_SEQ;
	}

	return 0;
}

static inline u32 to_hfi_codec(enum hal_video_codec hal_codec)
{
	switch (hal_codec) {
	case HAL_VIDEO_CODEC_MVC:
	case HAL_VIDEO_CODEC_H264:
		return HFI_VIDEO_CODEC_H264;
	case HAL_VIDEO_CODEC_H263:
		return HFI_VIDEO_CODEC_H263;
	case HAL_VIDEO_CODEC_MPEG1:
		return HFI_VIDEO_CODEC_MPEG1;
	case HAL_VIDEO_CODEC_MPEG2:
		return HFI_VIDEO_CODEC_MPEG2;
	case HAL_VIDEO_CODEC_MPEG4:
		return HFI_VIDEO_CODEC_MPEG4;
	case HAL_VIDEO_CODEC_DIVX_311:
		return HFI_VIDEO_CODEC_DIVX_311;
	case HAL_VIDEO_CODEC_DIVX:
		return HFI_VIDEO_CODEC_DIVX;
	case HAL_VIDEO_CODEC_VC1:
		return HFI_VIDEO_CODEC_VC1;
	case HAL_VIDEO_CODEC_SPARK:
		return HFI_VIDEO_CODEC_SPARK;
	case HAL_VIDEO_CODEC_VP8:
		return HFI_VIDEO_CODEC_VP8;
	case HAL_VIDEO_CODEC_HEVC:
		return HFI_VIDEO_CODEC_HEVC;
	case HAL_VIDEO_CODEC_HEVC_HYBRID:
		return HFI_VIDEO_CODEC_HEVC_HYBRID;
	default:
		break;
	}

	return 0;
}

static void pkt_sys_init(struct hfi_sys_init_pkt *pkt, u32 arch_type)
{
	pkt->hdr.size = sizeof(*pkt);
	pkt->hdr.pkt_type = HFI_CMD_SYS_INIT;
	pkt->arch_type = arch_type;
}

static void pkt_sys_pc_prep(struct hfi_sys_pc_prep_pkt *pkt)
{
	pkt->hdr.size = sizeof(*pkt);
	pkt->hdr.pkt_type = HFI_CMD_SYS_PC_PREP;
}

static void pkt_sys_idle_indicator(struct hfi_sys_set_property_pkt *pkt,
				   u32 enable)
{
	struct hfi_enable *hfi;

	pkt->hdr.size = sizeof(*pkt) + sizeof(*hfi) + sizeof(u32);
	pkt->hdr.pkt_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->data[0] = HFI_PROPERTY_SYS_IDLE_INDICATOR;
	hfi = (struct hfi_enable *) &pkt->data[1];
	hfi->enable = enable;
}

static void pkt_sys_debug_config(struct hfi_sys_set_property_pkt *pkt, u32 mode,
				 u32 config)
{
	struct hfi_debug_config *hfi;

	pkt->hdr.size = sizeof(*pkt) + sizeof(*hfi) + sizeof(u32);
	pkt->hdr.pkt_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->data[0] = HFI_PROPERTY_SYS_DEBUG_CONFIG;
	hfi = (struct hfi_debug_config *) &pkt->data[1];
	hfi->config = config;
	hfi->mode = mode;
}

static void pkt_sys_coverage_config(struct hfi_sys_set_property_pkt *pkt,
				    u32 mode)
{
	pkt->hdr.size = sizeof(*pkt) + sizeof(u32);
	pkt->hdr.pkt_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->data[0] = HFI_PROPERTY_SYS_CONFIG_COVERAGE;
	pkt->data[1] = mode;
}

static int pkt_sys_set_resource(struct hfi_sys_set_resource_pkt *pkt,
				struct hal_resource_hdr *resource_hdr,
				void *resource_value)
{
	if (!pkt || !resource_hdr || !resource_value)
		return -EINVAL;

	pkt->hdr.size = sizeof(*pkt);
	pkt->hdr.pkt_type = HFI_CMD_SYS_SET_RESOURCE;
	pkt->resource_handle = hash32_ptr(resource_hdr->resource_handle);

	switch (resource_hdr->resource_id) {
	case VIDC_RESOURCE_OCMEM:
	case VIDC_RESOURCE_VMEM: {
		struct hfi_resource_ocmem *res =
			(struct hfi_resource_ocmem *) &pkt->resource_data[0];

		res->size = resource_hdr->size;
		res->mem = (unsigned long)resource_value;
		pkt->resource_type = HFI_RESOURCE_OCMEM;
		pkt->hdr.size += sizeof(*res) - sizeof(u32);
		break;
	}
	default:
		return -ENOTSUPP;
	}

	return 0;
}

static int
pkt_sys_release_resource(struct hfi_sys_release_resource_pkt *pkt,
			 struct hal_resource_hdr *resource_hdr)
{
	pkt->hdr.size = sizeof(*pkt);
	pkt->hdr.pkt_type = HFI_CMD_SYS_RELEASE_RESOURCE;
	pkt->resource_handle = hash32_ptr(resource_hdr->resource_handle);

	switch (resource_hdr->resource_id) {
	case VIDC_RESOURCE_OCMEM:
	case VIDC_RESOURCE_VMEM:
		pkt->resource_type = HFI_RESOURCE_OCMEM;
		break;
	default:
		return -ENOTSUPP;
	}

	return 0;
}

static void pkt_sys_ping(struct hfi_sys_ping_pkt *pkt, u32 cookie)
{
	pkt->hdr.size = sizeof(*pkt);
	pkt->hdr.pkt_type = HFI_CMD_SYS_PING;
	pkt->client_data = cookie;
}

static int pkt_session_init(struct hfi_session_init_pkt *pkt,
			    struct hfi_device_inst *inst,
			    u32 domain, u32 codec)
{
	u32 session_codec = to_hfi_codec(codec);

	if (!session_codec)
		return -EINVAL;

	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = HFI_CMD_SYS_SESSION_INIT;
	pkt->shdr.session_id = hash32_ptr(inst);
	pkt->session_domain = domain;
	pkt->session_codec = session_codec;

	return 0;
}

static void pkt_session_cmd(struct hfi_session_pkt *pkt, u32 pkt_type,
			    struct hfi_device_inst *inst)
{
	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = pkt_type;
	pkt->shdr.session_id = hash32_ptr(inst);
}

static void pkt_sys_power_control(struct hfi_sys_set_property_pkt *pkt,
				  u32 enable)
{
	struct hfi_enable *hfi;

	pkt->hdr.size = sizeof(*pkt) + sizeof(*hfi) + sizeof(u32);
	pkt->hdr.pkt_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->data[0] = HFI_PROPERTY_SYS_CODEC_POWER_PLANE_CTRL;
	hfi = (struct hfi_enable *) &pkt->data[1];
	hfi->enable = enable;
}

static u32 to_hfi_buffer(int hal_buffer)
{
	switch (hal_buffer) {
	case HAL_BUFFER_INPUT:
		return HFI_BUFFER_INPUT;
	case HAL_BUFFER_OUTPUT:
		return HFI_BUFFER_OUTPUT;
	case HAL_BUFFER_OUTPUT2:
		return HFI_BUFFER_OUTPUT2;
	case HAL_BUFFER_EXTRADATA_INPUT:
		return HFI_BUFFER_EXTRADATA_INPUT;
	case HAL_BUFFER_EXTRADATA_OUTPUT:
		return HFI_BUFFER_EXTRADATA_OUTPUT;
	case HAL_BUFFER_EXTRADATA_OUTPUT2:
		return HFI_BUFFER_EXTRADATA_OUTPUT2;
	case HAL_BUFFER_INTERNAL_SCRATCH:
		return HFI_BUFFER_INTERNAL_SCRATCH;
	case HAL_BUFFER_INTERNAL_SCRATCH_1:
		return HFI_BUFFER_INTERNAL_SCRATCH_1;
	case HAL_BUFFER_INTERNAL_SCRATCH_2:
		return HFI_BUFFER_INTERNAL_SCRATCH_2;
	case HAL_BUFFER_INTERNAL_PERSIST:
		return HFI_BUFFER_INTERNAL_PERSIST;
	case HAL_BUFFER_INTERNAL_PERSIST_1:
		return HFI_BUFFER_INTERNAL_PERSIST_1;
	default:
		break;
	}

	return HAL_BUFFER_NONE;
}

static int to_hfi_extradata_index(enum hal_extradata_id index)
{
	switch (index) {
	case HAL_EXTRADATA_MB_QUANTIZATION:
		return HFI_PROPERTY_PARAM_VDEC_MB_QUANTIZATION;
	case HAL_EXTRADATA_INTERLACE_VIDEO:
		return HFI_PROPERTY_PARAM_VDEC_INTERLACE_VIDEO_EXTRADATA;
	case HAL_EXTRADATA_VC1_FRAMEDISP:
		return HFI_PROPERTY_PARAM_VDEC_VC1_FRAMEDISP_EXTRADATA;
	case HAL_EXTRADATA_VC1_SEQDISP:
		return HFI_PROPERTY_PARAM_VDEC_VC1_SEQDISP_EXTRADATA;
	case HAL_EXTRADATA_TIMESTAMP:
		return HFI_PROPERTY_PARAM_VDEC_TIMESTAMP_EXTRADATA;
	case HAL_EXTRADATA_S3D_FRAME_PACKING:
		return HFI_PROPERTY_PARAM_S3D_FRAME_PACKING_EXTRADATA;
	case HAL_EXTRADATA_FRAME_RATE:
		return HFI_PROPERTY_PARAM_VDEC_FRAME_RATE_EXTRADATA;
	case HAL_EXTRADATA_PANSCAN_WINDOW:
		return HFI_PROPERTY_PARAM_VDEC_PANSCAN_WNDW_EXTRADATA;
	case HAL_EXTRADATA_RECOVERY_POINT_SEI:
		return HFI_PROPERTY_PARAM_VDEC_RECOVERY_POINT_SEI_EXTRADATA;
	case HAL_EXTRADATA_MULTISLICE_INFO:
		return HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_INFO;
	case HAL_EXTRADATA_NUM_CONCEALED_MB:
		return HFI_PROPERTY_PARAM_VDEC_NUM_CONCEALED_MB;
	case HAL_EXTRADATA_ASPECT_RATIO:
	case HAL_EXTRADATA_INPUT_CROP:
	case HAL_EXTRADATA_DIGITAL_ZOOM:
		return HFI_PROPERTY_PARAM_INDEX_EXTRADATA;
	case HAL_EXTRADATA_MPEG2_SEQDISP:
		return HFI_PROPERTY_PARAM_VDEC_MPEG2_SEQDISP_EXTRADATA;
	case HAL_EXTRADATA_STREAM_USERDATA:
		return HFI_PROPERTY_PARAM_VDEC_STREAM_USERDATA_EXTRADATA;
	case HAL_EXTRADATA_FRAME_QP:
		return HFI_PROPERTY_PARAM_VDEC_FRAME_QP_EXTRADATA;
	case HAL_EXTRADATA_FRAME_BITS_INFO:
		return HFI_PROPERTY_PARAM_VDEC_FRAME_BITS_INFO_EXTRADATA;
	case HAL_EXTRADATA_LTR_INFO:
		return HFI_PROPERTY_PARAM_VENC_LTR_INFO;
	case HAL_EXTRADATA_METADATA_MBI:
		return HFI_PROPERTY_PARAM_VENC_MBI_DUMPING;
	default:
		break;
	}

	return 0;
}

static int to_hfi_extradata_id(enum hal_extradata_id index)
{
	switch (index) {
	case HAL_EXTRADATA_ASPECT_RATIO:
		return VIDC_EXTRADATA_ASPECT_RATIO;
	case HAL_EXTRADATA_INPUT_CROP:
		return VIDC_EXTRADATA_INPUT_CROP;
	case HAL_EXTRADATA_DIGITAL_ZOOM:
		return VIDC_EXTRADATA_DIGITAL_ZOOM;
	default:
		return to_hfi_extradata_index(index);
	}

	return 0;
}

static u32 to_hfi_buf_mode(enum hal_buffer_mode_type hal_buf_mode)
{
	switch (hal_buf_mode) {
	case HAL_BUFFER_MODE_STATIC:
		return HFI_BUFFER_MODE_STATIC;
	case HAL_BUFFER_MODE_RING:
		return HFI_BUFFER_MODE_RING;
	case HAL_BUFFER_MODE_DYNAMIC:
		return HFI_BUFFER_MODE_DYNAMIC;
	default:
		/* invalid buffer mode */
		break;
	}

	return 0;
}

static u32 to_hfi_ltr_mode(enum ltr_mode ltr_mode_type)
{
	switch (ltr_mode_type) {
	case HAL_LTR_MODE_DISABLE:
		return HFI_LTR_MODE_DISABLE;
	case HAL_LTR_MODE_MANUAL:
		return HFI_LTR_MODE_MANUAL;
	case HAL_LTR_MODE_PERIODIC:
		return HFI_LTR_MODE_PERIODIC;
	default:
		/* invalid ltr mode */
		return HFI_LTR_MODE_DISABLE;
	}

	return 0;
}

static int pkt_session_set_buffers(struct hfi_session_set_buffers_pkt *pkt,
				   struct hfi_device_inst *inst,
				   struct hal_buffer_addr_info *bai)
{
	int i;

	if (!inst || !pkt || !bai)
		return -EINVAL;

	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_SET_BUFFERS;
	pkt->shdr.session_id = hash32_ptr(inst);
	pkt->buffer_size = bai->buffer_size;
	pkt->min_buffer_size = bai->buffer_size;
	pkt->num_buffers = bai->num_buffers;

	if (bai->buffer_type == HAL_BUFFER_OUTPUT ||
	    bai->buffer_type == HAL_BUFFER_OUTPUT2) {
		struct hfi_buffer_info *bi;

		pkt->extradata_size = bai->extradata_size;
		pkt->shdr.hdr.size = sizeof(*pkt) - sizeof(u32) +
			(bai->num_buffers * sizeof(*bi));
		bi = (struct hfi_buffer_info *) pkt->buffer_info;
		for (i = 0; i < pkt->num_buffers; i++) {
			bi->buffer_addr = bai->device_addr;
			bi->extradata_addr = bai->extradata_addr;
		}
	} else {
		pkt->extradata_size = 0;
		pkt->shdr.hdr.size = sizeof(*pkt) +
			((bai->num_buffers - 1) * sizeof(u32));
		for (i = 0; i < pkt->num_buffers; i++)
			pkt->buffer_info[i] = bai->device_addr;
	}

	pkt->buffer_type = to_hfi_buffer(bai->buffer_type);
	if (!pkt->buffer_type)
		return -EINVAL;

	return 0;
}

static int
pkt_session_release_buffers(struct hfi_session_release_buffer_pkt *pkt,
			    struct hfi_device_inst *inst,
			    struct hal_buffer_addr_info *bai)
{
	int i;

	if (!inst || !pkt || !bai)
		return -EINVAL;

	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_RELEASE_BUFFERS;
	pkt->shdr.session_id = hash32_ptr(inst);
	pkt->buffer_size = bai->buffer_size;
	pkt->num_buffers = bai->num_buffers;

	if (bai->buffer_type == HAL_BUFFER_OUTPUT ||
	    bai->buffer_type == HAL_BUFFER_OUTPUT2) {
		struct hfi_buffer_info *bi;

		bi = (struct hfi_buffer_info *) pkt->buffer_info;
		for (i = 0; i < pkt->num_buffers; i++) {
			bi->buffer_addr = bai->device_addr;
			bi->extradata_addr = bai->extradata_addr;
		}
		pkt->shdr.hdr.size =
				sizeof(struct hfi_session_set_buffers_pkt) -
				sizeof(u32) + (bai->num_buffers * sizeof(*bi));
	} else {
		for (i = 0; i < pkt->num_buffers; i++)
			pkt->buffer_info[i] = bai->device_addr;

		pkt->extradata_size = 0;
		pkt->shdr.hdr.size =
				sizeof(struct hfi_session_set_buffers_pkt) +
				((bai->num_buffers - 1) * sizeof(u32));
	}

	pkt->response_req = bai->response_required;
	pkt->buffer_type = to_hfi_buffer(bai->buffer_type);
	if (!pkt->buffer_type)
		return -EINVAL;

	return 0;
}

static int
pkt_session_etb_decoder(struct hfi_session_empty_buffer_compressed_pkt *pkt,
			struct hfi_device_inst *inst,
			struct hal_frame_data *in_frame)
{
	if (!inst || !in_frame->device_addr)
		return -EINVAL;

	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_EMPTY_BUFFER;
	pkt->shdr.session_id = hash32_ptr(inst);
	pkt->time_stamp_hi = upper_32_bits(in_frame->timestamp);
	pkt->time_stamp_lo = lower_32_bits(in_frame->timestamp);
	pkt->flags = in_frame->flags;
	pkt->mark_target = in_frame->mark_target;
	pkt->mark_data = in_frame->mark_data;
	pkt->offset = in_frame->offset;
	pkt->alloc_len = in_frame->alloc_len;
	pkt->filled_len = in_frame->filled_len;
	pkt->input_tag = in_frame->clnt_data;
	pkt->packet_buffer = in_frame->device_addr;

	return 0;
}

static int pkt_session_etb_encoder(
		struct hfi_session_empty_buffer_uncompressed_plane0_pkt *pkt,
		struct hfi_device_inst *inst, struct hal_frame_data *in_frame)
{
	if (!inst || !in_frame->device_addr)
		return -EINVAL;

	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_EMPTY_BUFFER;
	pkt->shdr.session_id = hash32_ptr(inst);
	pkt->view_id = 0;
	pkt->time_stamp_hi = upper_32_bits(in_frame->timestamp);
	pkt->time_stamp_lo = lower_32_bits(in_frame->timestamp);
	pkt->flags = in_frame->flags;
	pkt->mark_target = in_frame->mark_target;
	pkt->mark_data = in_frame->mark_data;
	pkt->offset = in_frame->offset;
	pkt->alloc_len = in_frame->alloc_len;
	pkt->filled_len = in_frame->filled_len;
	pkt->input_tag = in_frame->clnt_data;
	pkt->packet_buffer = in_frame->device_addr;
	pkt->extradata_buffer = in_frame->extradata_addr;

	return 0;
}

static int pkt_session_ftb(struct hfi_session_fill_buffer_pkt *pkt,
			   struct hfi_device_inst *inst,
			   struct hal_frame_data *out_frame)
{
	if (!inst || !out_frame || !out_frame->device_addr)
		return -EINVAL;

	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_FILL_BUFFER;
	pkt->shdr.session_id = hash32_ptr(inst);

	if (out_frame->buffer_type == HAL_BUFFER_OUTPUT)
		pkt->stream_id = 0;
	else if (out_frame->buffer_type == HAL_BUFFER_OUTPUT2)
		pkt->stream_id = 1;

	pkt->packet_buffer = out_frame->device_addr;
	pkt->extradata_buffer = out_frame->extradata_addr;
	pkt->alloc_len = out_frame->alloc_len;
	pkt->filled_len = out_frame->filled_len;
	pkt->offset = out_frame->offset;
	pkt->data[0] = out_frame->extradata_size;

	return 0;
}

static int pkt_session_parse_seq_header(
		struct hfi_session_parse_sequence_header_pkt *pkt,
		struct hfi_device_inst *inst, struct hal_seq_hdr *seq_hdr)
{
	if (!inst || !seq_hdr || !seq_hdr->seq_hdr)
		return -EINVAL;

	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_PARSE_SEQUENCE_HEADER;
	pkt->shdr.session_id = hash32_ptr(inst);
	pkt->header_len = seq_hdr->seq_hdr_len;
	pkt->packet_buffer = seq_hdr->seq_hdr;

	return 0;
}

static int
pkt_session_get_seq_hdr(struct hfi_session_get_sequence_header_pkt *pkt,
			struct hfi_device_inst *inst,
			struct hal_seq_hdr *seq_hdr)
{
	if (!inst || !seq_hdr || !seq_hdr->seq_hdr)
		return -EINVAL;

	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_GET_SEQUENCE_HEADER;
	pkt->shdr.session_id = hash32_ptr(inst);
	pkt->buffer_len = seq_hdr->seq_hdr_len;
	pkt->packet_buffer = seq_hdr->seq_hdr;

	return 0;
}

static int pkt_session_flush(struct hfi_session_flush_pkt *pkt,
			     struct hfi_device_inst *inst,
			     enum hal_flush type)
{
	if (!inst)
		return -EINVAL;

	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_FLUSH;
	pkt->shdr.session_id = hash32_ptr(inst);

	switch (type) {
	case HAL_FLUSH_INPUT:
		pkt->flush_type = HFI_FLUSH_INPUT;
		break;
	case HAL_FLUSH_OUTPUT:
		pkt->flush_type = HFI_FLUSH_OUTPUT;
		break;
	case HAL_FLUSH_OUTPUT2:
		pkt->flush_type = HFI_FLUSH_OUTPUT2;
		break;
	case HAL_FLUSH_ALL:
		pkt->flush_type = HFI_FLUSH_ALL;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int pkt_session_get_property(struct hfi_session_get_property_pkt *pkt,
				    struct hfi_device_inst *inst,
				    enum hal_property ptype)
{
	if (!inst)
		return -EINVAL;

	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_GET_PROPERTY;
	pkt->shdr.session_id = hash32_ptr(inst);
	pkt->num_properties = 1;

	switch (ptype) {
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
		pkt->data[0] = HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT;
		break;
	case HAL_PARAM_GET_BUFFER_REQUIREMENTS:
		pkt->data[0] = HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS;
		break;
	default:
		return -ENOTSUPP;
	}

	return 0;
}

static int pkt_session_set_property(struct hfi_session_set_property_pkt *pkt,
				    struct hfi_device_inst *inst,
				    enum hal_property ptype, void *pdata)
{
	void *prop_data = &pkt->data[1];
	int ret = 0;

	if (!pkt || !inst || !pdata)
		return -EINVAL;

	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_SET_PROPERTY;
	pkt->shdr.session_id = hash32_ptr(inst);
	pkt->num_properties = 1;

	switch (ptype) {
	case HAL_CONFIG_FRAME_RATE: {
		struct hal_framerate *hal = pdata;
		struct hfi_framerate *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_FRAME_RATE;
		hfi->buffer_type = to_hfi_buffer(hal->buffer_type);
		if (!hfi->buffer_type)
			ret = -EINVAL;
		hfi->framerate = hal->framerate;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT: {
		struct hal_uncompressed_format_select *hal = pdata;
		struct hfi_uncompressed_format_select *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_UNCOMPRESSED_FORMAT_SELECT;
		hfi->buffer_type = to_hfi_buffer(hal->buffer_type);
		if (!hfi->buffer_type)
			ret = -EINVAL;
		hfi->format = to_hfi_type(HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT,
					  hal->format);
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO:
		break;
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_INFO:
		break;
	case HAL_PARAM_EXTRA_DATA_HEADER_CONFIG:
		break;
	case HAL_PARAM_FRAME_SIZE: {
		struct hal_framesize *hal = pdata;
		struct hfi_framesize *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_FRAME_SIZE;
		hfi->buffer_type = to_hfi_buffer(hal->buffer_type);
		if (!hfi->buffer_type)
			ret = -EINVAL;
		hfi->height = hal->height;
		hfi->width = hal->width;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_REALTIME: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_REALTIME;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_BUFFER_COUNT_ACTUAL: {
		struct hal_buffer_count_actual *hal = pdata;
		struct hfi_buffer_count_actual *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_BUFFER_COUNT_ACTUAL;
		hfi->count_actual = hal->count_actual;
		hfi->type = to_hfi_buffer(hal->type);
		if (!hfi->type)
			ret = -EINVAL;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_BUFFER_SIZE_ACTUAL: {
		struct hal_buffer_size_actual *hal = pdata;
		struct hfi_buffer_size_actual *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_BUFFER_SIZE_ACTUAL;
		hfi->size = hal->size;
		hfi->type = to_hfi_buffer(hal->type);
		if (!hfi->type)
			ret = -EINVAL;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_BUFFER_DISPLAY_HOLD_COUNT_ACTUAL: {
		struct hal_buffer_display_hold_count_actual *hal = pdata;
		struct hfi_buffer_display_hold_count_actual *hfi = prop_data;

		pkt->data[0] =
			HFI_PROPERTY_PARAM_BUFFER_DISPLAY_HOLD_COUNT_ACTUAL;
		hfi->hold_count = hal->hold_count;
		hfi->type = to_hfi_buffer(hal->buffer_type);
		if (!hfi->type)
			ret = -EINVAL;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_NAL_STREAM_FORMAT_SELECT: {
		struct hal_nal_stream_format_select *hal = pdata;
		struct hfi_nal_stream_format_select *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SELECT;
		hfi->format = to_hfi_type(HAL_PARAM_NAL_STREAM_FORMAT_SELECT,
					  hal->format);
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VDEC_OUTPUT_ORDER: {
		u32 *data = pdata;

		pkt->data[0] = HFI_PROPERTY_PARAM_VDEC_OUTPUT_ORDER;
		switch (*data) {
		case HAL_OUTPUT_ORDER_DECODE:
			pkt->data[1] = HFI_OUTPUT_ORDER_DECODE;
			break;
		case HAL_OUTPUT_ORDER_DISPLAY:
			pkt->data[1] = HFI_OUTPUT_ORDER_DISPLAY;
			break;
		default:
			/* invalid output order */
			ret = -EINVAL;
			break;
		}
		pkt->shdr.hdr.size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_PICTURE_TYPE_DECODE: {
		struct hal_enable_picture *hal = pdata;
		struct hfi_enable_picture *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VDEC_PICTURE_TYPE_DECODE;
		hfi->picture_type = hal->picture_type;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] =
			HFI_PROPERTY_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_VDEC_POST_LOOP_DEBLOCKER: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VDEC_POST_LOOP_DEBLOCKER;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VDEC_MULTI_STREAM: {
		struct hal_multi_stream *hal = pdata;
		struct hfi_multi_stream *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM;
		hfi->buffer_type = to_hfi_buffer(hal->buffer_type);
		if (!hfi->buffer_type)
			return -EINVAL;
		hfi->enable = hal->enable;
		hfi->width = hal->width;
		hfi->height = hal->height;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT: {
		struct hal_display_picture_buffer_count *hal = pdata;
		struct hfi_display_picture_buffer_count *hfi = prop_data;

		pkt->data[0] =
			HFI_PROPERTY_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT;
		hfi->count = hal->count;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_DIVX_FORMAT: {
		u32 *data = pdata;

		pkt->data[0] = HFI_PROPERTY_PARAM_DIVX_FORMAT;
		switch (*data) {
		case HAL_DIVX_FORMAT_4:
			pkt->data[1] = HFI_DIVX_FORMAT_4;
			break;
		case HAL_DIVX_FORMAT_5:
			pkt->data[1] = HFI_DIVX_FORMAT_5;
			break;
		case HAL_DIVX_FORMAT_6:
			pkt->data[1] = HFI_DIVX_FORMAT_6;
			break;
		default:
			/* Invalid divx format */
			ret = -EINVAL;
			break;
		}
		pkt->shdr.hdr.size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VDEC_MB_ERROR_MAP_REPORTING;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VDEC_CONTINUE_DATA_TRANSFER: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VDEC_CONTINUE_DATA_TRANSFER;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VDEC_SYNC_FRAME_DECODE: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VDEC_THUMBNAIL_MODE;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_SYNC_FRAME_SEQUENCE_HEADER: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] =
			HFI_PROPERTY_CONFIG_VENC_SYNC_FRAME_SEQUENCE_HEADER;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_VENC_REQUEST_IFRAME:
		pkt->data[0] = HFI_PROPERTY_CONFIG_VENC_REQUEST_SYNC_FRAME;
		pkt->shdr.hdr.size += sizeof(u32);
		break;
	case HAL_PARAM_VENC_MPEG4_SHORT_HEADER:
		break;
	case HAL_PARAM_VENC_MPEG4_AC_PREDICTION:
		break;
	case HAL_CONFIG_VENC_TARGET_BITRATE: {
		struct hal_bitrate *hal = pdata;
		struct hfi_bitrate *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VENC_TARGET_BITRATE;
		hfi->bitrate = hal->bitrate;
		hfi->layer_id = hal->layer_id;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_VENC_MAX_BITRATE: {
		struct hal_bitrate *hal = pdata;
		struct hfi_bitrate *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VENC_MAX_BITRATE;
		hfi->bitrate = hal->bitrate;
		hfi->layer_id = hal->layer_id;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_PROFILE_LEVEL_CURRENT: {
		struct hal_profile_level *hal = pdata;
		struct hfi_profile_level *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT;
		hfi->level = hal->level;
		hfi->profile = to_hfi_type(HAL_PARAM_PROFILE_LEVEL_CURRENT,
					   hal->profile);
		if (hfi->profile <= 0)
			/* Profile not supported, falling back to high */
			hfi->profile = HFI_H264_PROFILE_HIGH;

		if (!hfi->level)
			/* Level not supported, falling back to 1 */
			hfi->level = 1;

		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_H264_ENTROPY_CONTROL: {
		struct hal_h264_entropy_control *hal = pdata;
		struct hfi_h264_entropy_control *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_H264_ENTROPY_CONTROL;
		hfi->entropy_mode = to_hfi_type(
					HAL_PARAM_VENC_H264_ENTROPY_CONTROL,
					hal->entropy_mode);
		if (hfi->entropy_mode == HAL_H264_ENTROPY_CABAC)
			hfi->cabac_model = to_hfi_type(
					HAL_PARAM_VENC_H264_ENTROPY_CABAC_MODEL,
					hal->cabac_model);
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_RATE_CONTROL: {
		enum hal_rate_control *rc = pdata;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_RATE_CONTROL;
		switch (*rc) {
		case HAL_RATE_CONTROL_OFF:
			pkt->data[1] = HFI_RATE_CONTROL_OFF;
			break;
		case HAL_RATE_CONTROL_CBR_CFR:
			pkt->data[1] = HFI_RATE_CONTROL_CBR_CFR;
			break;
		case HAL_RATE_CONTROL_CBR_VFR:
			pkt->data[1] = HFI_RATE_CONTROL_CBR_VFR;
			break;
		case HAL_RATE_CONTROL_VBR_CFR:
			pkt->data[1] = HFI_RATE_CONTROL_VBR_CFR;
			break;
		case HAL_RATE_CONTROL_VBR_VFR:
			pkt->data[1] = HFI_RATE_CONTROL_VBR_VFR;
			break;
		default:
			/* Invalid Rate control setting */
			ret = -EINVAL;
			break;
		}
		pkt->shdr.hdr.size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_MPEG4_TIME_RESOLUTION: {
		struct hal_mpeg4_time_resolution *hal = pdata;
		struct hfi_mpeg4_time_resolution *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_MPEG4_TIME_RESOLUTION;
		hfi->time_increment_resolution =
			hal->time_increment_resolution;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_MPEG4_HEADER_EXTENSION: {
		struct hal_mpeg4_header_extension *hal = pdata;
		struct hfi_mpeg4_header_extension *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_MPEG4_HEADER_EXTENSION;
		hfi->header_extension = hal->header_extension;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_H264_DEBLOCK_CONTROL: {
		struct hal_h264_db_control *hal = pdata;
		struct hfi_h264_db_control *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_H264_DEBLOCK_CONTROL;
		switch (hal->mode) {
		case HAL_H264_DB_MODE_DISABLE:
			hfi->mode = HFI_H264_DB_MODE_DISABLE;
			break;
		case HAL_H264_DB_MODE_SKIP_SLICE_BOUNDARY:
			hfi->mode = HFI_H264_DB_MODE_SKIP_SLICE_BOUNDARY;
			break;
		case HAL_H264_DB_MODE_ALL_BOUNDARY:
			hfi->mode = HFI_H264_DB_MODE_ALL_BOUNDARY;
			break;
		default:
			/* Invalid deblocking mode */
			ret = -EINVAL;
			break;
		}
		hfi->slice_alpha_offset = hal->slice_alpha_offset;
		hfi->slice_beta_offset = hal->slice_beta_offset;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_SESSION_QP: {
		struct hal_quantization *hal = pdata;
		struct hfi_quantization *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_SESSION_QP;
		hfi->qp_i = hal->qpi;
		hfi->qp_p = hal->qpp;
		hfi->qp_b = hal->qpb;
		hfi->layer_id = hal->layer_id;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_SESSION_QP_RANGE: {
		struct hal_quantization_range *hal = pdata;
		struct hfi_quantization_range *hfi = prop_data;
		u32 min_qp, max_qp;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_SESSION_QP_RANGE;
		min_qp = hal->min_qp;
		max_qp = hal->max_qp;

		/* We'll be packing in the qp, so make sure we
		 * won't be losing data when masking */
		if (min_qp > 0xff || max_qp > 0xff) {
			ret = -ERANGE;
			break;
		}

		/* When creating the packet, pack the qp value as
		 * 0xiippbb, where ii = qp range for I-frames,
		 * pp = qp range for P-frames, etc. */
		hfi->min_qp = min_qp | min_qp << 8 | min_qp << 16;
		hfi->max_qp = max_qp | max_qp << 8 | max_qp << 16;
		hfi->layer_id = hal->layer_id;

		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_SEARCH_RANGE: {
		struct hal_vc1e_perf_cfg_type *hal = pdata;
		struct hfi_vc1e_perf_cfg_type *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_VC1_PERF_CFG;
		hfi->search_range_x_subsampled[0] = hal->i_frame.x_subsampled;
		hfi->search_range_x_subsampled[1] = hal->p_frame.x_subsampled;
		hfi->search_range_x_subsampled[2] = hal->b_frame.x_subsampled;
		hfi->search_range_y_subsampled[0] = hal->i_frame.y_subsampled;
		hfi->search_range_y_subsampled[1] = hal->p_frame.y_subsampled;
		hfi->search_range_y_subsampled[2] = hal->b_frame.y_subsampled;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_MAX_NUM_B_FRAMES: {
		u32 *hal = pdata;
		struct hfi_max_num_b_frames *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_MAX_NUM_B_FRAMES;
		hfi->max_num_b_frames = *hal;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_VENC_INTRA_PERIOD: {
		struct hal_intra_period *hal = pdata;
		struct hfi_intra_period *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VENC_INTRA_PERIOD;
		hfi->pframes = hal->pframes;
		hfi->bframes = hal->bframes;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_VENC_IDR_PERIOD: {
		struct hal_idr_period *hal = pdata;
		struct hfi_idr_period *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VENC_IDR_PERIOD;
		hfi->idr_period = hal->idr_period;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VDEC_CONCEAL_COLOR: {
		u32 *hal = pdata;
		struct hfi_conceal_color *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VDEC_CONCEAL_COLOR;
		hfi->conceal_color = *hal;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_VPE_OPERATIONS: {
		struct hal_operations *hal = pdata;
		struct hfi_operations_type *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VPE_OPERATIONS;

		switch (hal->rotate) {
		case HAL_ROTATE_NONE:
			hfi->rotation = HFI_ROTATE_NONE;
			break;
		case HAL_ROTATE_90:
			hfi->rotation = HFI_ROTATE_90;
			break;
		case HAL_ROTATE_180:
			hfi->rotation = HFI_ROTATE_180;
			break;
		case HAL_ROTATE_270:
			hfi->rotation = HFI_ROTATE_270;
			break;
		default:
			ret = -EINVAL;
			break;
		}

		switch (hal->flip) {
		case HAL_FLIP_NONE:
			hfi->flip = HFI_FLIP_NONE;
			break;
		case HAL_FLIP_HORIZONTAL:
			hfi->flip = HFI_FLIP_HORIZONTAL;
			break;
		case HAL_FLIP_VERTICAL:
			hfi->flip = HFI_FLIP_VERTICAL;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_INTRA_REFRESH: {
		struct hal_intra_refresh *hal = pdata;
		struct hfi_intra_refresh *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_INTRA_REFRESH;
		switch (hal->mode) {
		case HAL_INTRA_REFRESH_NONE:
			hfi->mode = HFI_INTRA_REFRESH_NONE;
			break;
		case HAL_INTRA_REFRESH_ADAPTIVE:
			hfi->mode = HFI_INTRA_REFRESH_ADAPTIVE;
			break;
		case HAL_INTRA_REFRESH_CYCLIC:
			hfi->mode = HFI_INTRA_REFRESH_CYCLIC;
			break;
		case HAL_INTRA_REFRESH_CYCLIC_ADAPTIVE:
			hfi->mode = HFI_INTRA_REFRESH_CYCLIC_ADAPTIVE;
			break;
		case HAL_INTRA_REFRESH_RANDOM:
			hfi->mode = HFI_INTRA_REFRESH_RANDOM;
			break;
		default:
			/* Invalid intra refresh setting */
			ret = -EINVAL;
			break;
		}
		hfi->air_mbs = hal->air_mbs;
		hfi->air_ref = hal->air_ref;
		hfi->cir_mbs = hal->cir_mbs;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_MULTI_SLICE_CONTROL: {
		struct hal_multi_slice_control *hal = pdata;
		struct hfi_multi_slice_control *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_CONTROL;

		switch (hal->multi_slice) {
		case HAL_MULTI_SLICE_OFF:
			hfi->multi_slice = HFI_MULTI_SLICE_OFF;
			break;
		case HAL_MULTI_SLICE_GOB:
			hfi->multi_slice = HFI_MULTI_SLICE_GOB;
			break;
		case HAL_MULTI_SLICE_BY_MB_COUNT:
			hfi->multi_slice = HFI_MULTI_SLICE_BY_MB_COUNT;
			break;
		case HAL_MULTI_SLICE_BY_BYTE_COUNT:
			hfi->multi_slice = HFI_MULTI_SLICE_BY_BYTE_COUNT;
			break;
		default:
			/* Invalid slice settings */
			ret = -EINVAL;
			break;
		}
		hfi->slice_size = hal->slice_size;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_INDEX_EXTRADATA: {
		struct hal_extradata_enable *hal = pdata;
		struct hfi_index_extradata_config *hfi = prop_data;
		int id = 0;

		pkt->data[0] = to_hfi_extradata_index(hal->index);
		hfi->enable = hal->enable;
		id = to_hfi_extradata_id(hal->index);
		if (id)
			hfi->index_extra_data_id = id;
		else
			ret = -EINVAL;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_SLICE_DELIVERY_MODE: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_SLICE_DELIVERY_MODE;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_H264_VUI_TIMING_INFO: {
		struct hal_h264_vui_timing_info *hal = pdata;
		struct hfi_h264_vui_timing_info *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_H264_VUI_TIMING_INFO;
		hfi->enable = hal->enable;
		hfi->fixed_framerate = hal->fixed_framerate;
		hfi->time_scale = hal->time_scale;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_VPE_DEINTERLACE: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VPE_DEINTERLACE;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_H264_GENERATE_AUDNAL: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_H264_GENERATE_AUDNAL;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_BUFFER_ALLOC_MODE: {
		struct hal_buffer_alloc_mode *hal = pdata;
		struct hfi_buffer_alloc_mode *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_BUFFER_ALLOC_MODE;
		hfi->type = to_hfi_buffer(hal->type);
		if (!hfi->type)
			return -EINVAL;
		hfi->mode = to_hfi_buf_mode(hal->mode);
		if (!hfi->mode)
			return -EINVAL;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VDEC_FRAME_ASSEMBLY: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VDEC_FRAME_ASSEMBLY;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_H264_VUI_BITSTREAM_RESTRC: {
		struct hal_h264_vui_bitstream_restrc *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] =
			HFI_PROPERTY_PARAM_VENC_H264_VUI_BITSTREAM_RESTRC;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_PRESERVE_TEXT_QUALITY: {
		struct hal_preserve_text_quality *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_PRESERVE_TEXT_QUALITY;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VDEC_SCS_THRESHOLD: {
		u32 *hal = pdata;
		struct hfi_scs_threshold *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VDEC_SCS_THRESHOLD;
		hfi->threshold_value = *hal;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_MVC_BUFFER_LAYOUT: {
		struct hal_mvc_buffer_layout *hal = pdata;
		struct hfi_mvc_buffer_layout_descp_type *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_MVC_BUFFER_LAYOUT;
		hfi->layout_type = to_hfi_layout(hal->layout_type);
		hfi->bright_view_first = hal->bright_view_first;
		hfi->ngap = hal->ngap;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_LTRMODE: {
		struct hal_ltr_mode *hal = pdata;
		struct hfi_ltr_mode *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_LTRMODE;
		hfi->ltr_mode = to_hfi_ltr_mode(hal->mode);
		hfi->ltr_count = hal->count;
		hfi->trust_mode = hal->trust_mode;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_VENC_USELTRFRAME: {
		struct hal_ltr_use *hal = pdata;
		struct hfi_ltr_use *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VENC_USELTRFRAME;
		hfi->frames = hal->frames;
		hfi->ref_ltr = hal->ref_ltr;
		hfi->use_constrnt = hal->use_constraint;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_VENC_MARKLTRFRAME: {
		struct hal_ltr_mark *hal = pdata;
		struct hfi_ltr_mark *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VENC_MARKLTRFRAME;
		hfi->mark_frame = hal->mark_frame;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_HIER_P_MAX_ENH_LAYERS: {
		u32 *hal = pdata;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_HIER_P_MAX_NUM_ENH_LAYER;
		pkt->data[1] = *hal;
		pkt->shdr.hdr.size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VENC_HIER_P_NUM_FRAMES: {
		u32 *hal = pdata;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VENC_HIER_P_ENH_LAYER;
		pkt->data[1] = *hal;
		pkt->shdr.hdr.size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_DISABLE_RC_TIMESTAMP: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_DISABLE_RC_TIMESTAMP;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_ENABLE_INITIAL_QP: {
		struct hal_initial_quantization *hal = pdata;
		struct hfi_initial_quantization *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_INITIAL_QP;
		hfi->init_qp_enable = hal->init_qp_enable;
		hfi->qp_i = hal->qpi;
		hfi->qp_p = hal->qpp;
		hfi->qp_b = hal->qpb;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VPE_COLOR_SPACE_CONVERSION: {
		struct hal_vpe_color_space_conversion *hal = pdata;
		struct hfi_vpe_color_space_conversion *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VPE_COLOR_SPACE_CONVERSION;
		memcpy(hfi->csc_matrix, hal->csc_matrix,
			sizeof(hfi->csc_matrix));
		memcpy(hfi->csc_bias, hal->csc_bias, sizeof(hfi->csc_bias));
		memcpy(hfi->csc_limit, hal->csc_limit, sizeof(hfi->csc_limit));
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] =
			HFI_PROPERTY_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_H264_NAL_SVC_EXT: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_H264_NAL_SVC_EXT;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_CONFIG_VENC_PERF_MODE: {
		u32 *hal = pdata;

		pkt->data[0] = HFI_PROPERTY_CONFIG_VENC_PERF_MODE;
		pkt->data[1] = *hal;
		pkt->shdr.hdr.size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_HIER_B_MAX_ENH_LAYERS: {
		u32 *hal = pdata;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_HIER_B_MAX_NUM_ENH_LAYER;
		pkt->data[1] = *hal;
		pkt->shdr.hdr.size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_NON_SECURE_OUTPUT2: {
		struct hal_enable *hal = pdata;
		struct hfi_enable *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VDEC_NONCP_OUTPUT2;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_HIER_P_HYBRID_MODE: {
		struct hal_hybrid_hierp *hal = pdata;
		struct hfi_hybrid_hierp *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_HIER_P_HYBRID_MODE;
		hfi->layers = hal->layers;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}

	/* FOLLOWING PROPERTIES ARE NOT IMPLEMENTED IN CORE YET */
	case HAL_CONFIG_BUFFER_REQUIREMENTS:
	case HAL_CONFIG_PRIORITY:
	case HAL_CONFIG_BATCH_INFO:
	case HAL_PARAM_METADATA_PASS_THROUGH:
	case HAL_SYS_IDLE_INDICATOR:
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SUPPORTED:
	case HAL_PARAM_INTERLACE_FORMAT_SUPPORTED:
	case HAL_PARAM_CHROMA_SITE:
	case HAL_PARAM_PROPERTIES_SUPPORTED:
	case HAL_PARAM_PROFILE_LEVEL_SUPPORTED:
	case HAL_PARAM_CAPABILITY_SUPPORTED:
	case HAL_PARAM_NAL_STREAM_FORMAT_SUPPORTED:
	case HAL_PARAM_MULTI_VIEW_FORMAT:
	case HAL_PARAM_MAX_SEQUENCE_HEADER_SIZE:
	case HAL_PARAM_CODEC_SUPPORTED:
	case HAL_PARAM_VDEC_MULTI_VIEW_SELECT:
	case HAL_PARAM_VDEC_MB_QUANTIZATION:
	case HAL_PARAM_VDEC_NUM_CONCEALED_MB:
	case HAL_PARAM_VDEC_H264_ENTROPY_SWITCHING:
	case HAL_PARAM_VENC_MPEG4_DATA_PARTITIONING:
	case HAL_CONFIG_BUFFER_COUNT_ACTUAL:
	case HAL_CONFIG_VDEC_MULTI_STREAM:
	case HAL_PARAM_VENC_MULTI_SLICE_INFO:
	case HAL_CONFIG_VENC_TIMESTAMP_SCALE:
	case HAL_PARAM_VENC_LOW_LATENCY:
	default:
		return -ENOTSUPP;
	}

	return ret;
}

static int
pkt_session_set_property_3xx(struct hfi_session_set_property_pkt *pkt,
			     struct hfi_device_inst *inst,
			     enum hal_property ptype, void *pdata)
{
	void *prop_data = &pkt->data[1];
	int ret = 0;

	if (!pkt || !inst || !pdata)
		return -EINVAL;

	pkt->shdr.hdr.size = sizeof(*pkt);
	pkt->shdr.hdr.pkt_type = HFI_CMD_SESSION_SET_PROPERTY;
	pkt->shdr.session_id = hash32_ptr(inst);
	pkt->num_properties = 1;

	/*
	 * Any session set property which is different in 3XX packetization
	 * should be added as a new case below. All unchanged session set
	 * properties will be handled in the default case.
	 */
	switch (ptype) {
	case HAL_PARAM_VDEC_MULTI_STREAM: {
		struct hal_multi_stream *hal = pdata;
		struct hfi_multi_stream_3x *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM;
		hfi->buffer_type = to_hfi_buffer(hal->buffer_type);
		if (!hfi->buffer_type)
			return -EINVAL;
		hfi->enable = hal->enable;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	case HAL_PARAM_VENC_INTRA_REFRESH: {
		struct hal_intra_refresh *hal = pdata;
		struct hfi_intra_refresh_3x *hfi = prop_data;

		pkt->data[0] = HFI_PROPERTY_PARAM_VENC_INTRA_REFRESH;
		switch (hal->mode) {
		case HAL_INTRA_REFRESH_NONE:
			hfi->mode = HFI_INTRA_REFRESH_NONE;
			break;
		case HAL_INTRA_REFRESH_ADAPTIVE:
			hfi->mode = HFI_INTRA_REFRESH_ADAPTIVE;
			break;
		case HAL_INTRA_REFRESH_CYCLIC:
			hfi->mode = HFI_INTRA_REFRESH_CYCLIC;
			break;
		case HAL_INTRA_REFRESH_CYCLIC_ADAPTIVE:
			hfi->mode = HFI_INTRA_REFRESH_CYCLIC_ADAPTIVE;
			break;
		case HAL_INTRA_REFRESH_RANDOM:
			hfi->mode = HFI_INTRA_REFRESH_RANDOM;
			break;
		default:
			/* invalid intra refresh setting */
			break;
		}
		hfi->mbs = hal->cir_mbs;
		pkt->shdr.hdr.size += sizeof(u32) + sizeof(*hfi);
		break;
	}
	default:
		ret = pkt_session_set_property(pkt, inst, ptype, pdata);
		break;
	}

	return ret;
}

static u32 to_hfi_ssr_type(enum hal_ssr_trigger_type type)
{
	switch (type) {
	case SSR_ERR_FATAL:
		return HFI_TEST_SSR_SW_ERR_FATAL;
	case SSR_SW_DIV_BY_ZERO:
		return HFI_TEST_SSR_SW_DIV_BY_ZERO;
	case SSR_HW_WDOG_IRQ:
		return HFI_TEST_SSR_HW_WDOG_IRQ;
	default:
		break;
	}

	return HFI_TEST_SSR_HW_WDOG_IRQ;
}

static void pkt_ssr_cmd(enum hal_ssr_trigger_type type,
		        struct hfi_sys_test_ssr_pkt *pkt)
{
	pkt->hdr.size = sizeof(*pkt);
	pkt->hdr.pkt_type = HFI_CMD_SYS_TEST_SSR;
	pkt->trigger_type = to_hfi_ssr_type(type);
}

static void pkt_sys_image_version(struct hfi_sys_get_property_pkt *pkt)
{
	pkt->hdr.size = sizeof(*pkt);
	pkt->hdr.pkt_type = HFI_CMD_SYS_GET_PROPERTY;
	pkt->num_properties = 1;
	pkt->data[0] = HFI_PROPERTY_SYS_IMAGE_VERSION;
}

static const struct hfi_packetization_ops hfi_default = {
	.sys_init = pkt_sys_init,
	.sys_pc_prep = pkt_sys_pc_prep,
	.sys_idle_indicator = pkt_sys_idle_indicator,
	.sys_power_control = pkt_sys_power_control,
	.sys_set_resource = pkt_sys_set_resource,
	.sys_debug_config = pkt_sys_debug_config,
	.sys_coverage_config = pkt_sys_coverage_config,
	.sys_release_resource = pkt_sys_release_resource,
	.sys_ping = pkt_sys_ping,
	.sys_image_version = pkt_sys_image_version,
	.ssr_cmd = pkt_ssr_cmd,
	.session_init = pkt_session_init,
	.session_cmd = pkt_session_cmd,
	.session_set_buffers = pkt_session_set_buffers,
	.session_release_buffers = pkt_session_release_buffers,
	.session_etb_decoder = pkt_session_etb_decoder,
	.session_etb_encoder = pkt_session_etb_encoder,
	.session_ftb = pkt_session_ftb,
	.session_parse_seq_header = pkt_session_parse_seq_header,
	.session_get_seq_hdr = pkt_session_get_seq_hdr,
	.session_flush = pkt_session_flush,
	.session_get_property = pkt_session_get_property,
	.session_set_property = pkt_session_set_property,
};

static const struct hfi_packetization_ops *get_3xx_ops(void)
{
	static struct hfi_packetization_ops hfi_3xx;

	hfi_3xx = hfi_default;
	hfi_3xx.session_set_property = pkt_session_set_property_3xx;

	return &hfi_3xx;
}

const struct hfi_packetization_ops *
hfi_get_pkt_ops(enum hfi_packetization_type type)
{
	switch (type) {
	case HFI_PACKETIZATION_LEGACY:
		return &hfi_default;
	case HFI_PACKETIZATION_3XX:
		return get_3xx_ops();
	}

	return NULL;
}
