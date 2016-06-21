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

#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <media/videobuf2-dma-sg.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ctrls.h>

#include "core.h"
#include "helpers.h"
#include "load.h"
#include "venc_ctrls.h"

#define NUM_B_FRAMES_MAX	4

/*
 * Default 601 to 709 conversion coefficients for resolution: 176x144 negative
 * coeffs are converted to s4.9 format (e.g. -22 converted to ((1<<13) - 22)
 * 3x3 transformation matrix coefficients in s4.9 fixed point format
 */
static u32 vpe_csc_601_to_709_matrix_coeff[HAL_MAX_MATRIX_COEFFS] = {
	470, 8170, 8148, 0, 490, 50, 0, 34, 483
};

/* offset coefficients in s9 fixed point format */
static u32 vpe_csc_601_to_709_bias_coeff[HAL_MAX_BIAS_COEFFS] = {
	34, 0, 4
};

/* clamping value for Y/U/V([min,max] for Y/U/V) */
static u32 vpe_csc_601_to_709_limit_coeff[HAL_MAX_LIMIT_COEFFS] = {
	16, 235, 16, 240, 16, 240
};

static u32 get_framesize_nv12(int plane, u32 height, u32 width)
{
	u32 y_stride, uv_stride, y_plane;
	u32 y_sclines, uv_sclines, uv_plane;
	u32 size;

	y_stride = ALIGN(width, 128);
	uv_stride = ALIGN(width, 128);
	y_sclines = ALIGN(height, 32);
	uv_sclines = ALIGN(((height + 1) >> 1), 16);

	y_plane = y_stride * y_sclines;
	uv_plane = uv_stride * uv_sclines + SZ_4K;
	size = y_plane + uv_plane + SZ_8K;
	size = ALIGN(size, SZ_4K);

	return size;
}

static u32 get_framesize_compressed(int plane, u32 height, u32 width)
{
	u32 sz = ALIGN(height, 32) * ALIGN(width, 32) * 3 / 2;

	return ALIGN(sz, SZ_4K);
}

static const struct vidc_format venc_formats[] = {
	{
		.pixfmt = V4L2_PIX_FMT_NV12,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	}, /*{
		.pixfmt = V4L2_PIX_FMT_NV21,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
	}, */{
		.pixfmt = V4L2_PIX_FMT_MPEG4,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_H263,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_H264,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
	}, {
		.pixfmt = V4L2_PIX_FMT_VP8,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
	},
};

static const struct vidc_format *find_format(u32 pixfmt, int type)
{
	const struct vidc_format *fmt = venc_formats;
	unsigned int size = ARRAY_SIZE(venc_formats);
	unsigned int i;

	for (i = 0; i < size; i++) {
		if (fmt[i].pixfmt == pixfmt)
			break;
	}

	if (i == size || fmt[i].type != type)
		return NULL;

	return &fmt[i];
}

static const struct vidc_format *find_format_by_index(int index, int type)
{
	const struct vidc_format *fmt = venc_formats;
	unsigned int size = ARRAY_SIZE(venc_formats);
	int i, k = 0;

	if (index < 0 || index > size)
		return NULL;

	for (i = 0; i < size; i++) {
		if (fmt[i].type != type)
			continue;
		if (k == index)
			break;
		k++;
	}

	if (i == size)
		return NULL;

	return &fmt[i];
}

int venc_toggle_hier_p(struct vidc_inst *inst, u32 layers)
{
	struct hfi_device *hfi = &inst->core->hfi;
	u32 num_enh_layers = 0;
	u32 ptype = HAL_PARAM_VENC_HIER_P_MAX_ENH_LAYERS;

	if (inst->fmt_cap->pixfmt != V4L2_PIX_FMT_VP8 &&
	    inst->fmt_cap->pixfmt != V4L2_PIX_FMT_H264)
		return 0;

	num_enh_layers = layers ? : 0;

	return vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype,
					     &num_enh_layers);
}

int venc_set_bitrate_for_each_layer(struct vidc_inst *inst, u32 num_enh_layers,
				    u32 total_bitrate)
{
	struct hfi_device *hfi = &inst->core->hfi;
	struct hal_bitrate bitrate;
	u32 bitrate_table[3][4] = {
		{50, 50, 0, 0},
		{34, 33, 33, 0},
		{25, 25, 25, 25}
	};
	u32 ptype, i;
	int ret;

	if (!num_enh_layers || num_enh_layers > ARRAY_SIZE(bitrate_table))
		return -EINVAL;

	ptype = HAL_CONFIG_VENC_TARGET_BITRATE;

	for (i = 0; !ret && i <= num_enh_layers; i++) {
		bitrate.bitrate = (total_bitrate *
				bitrate_table[num_enh_layers - 1][i]) / 100;
		bitrate.layer_id = i;

		ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst,
						    ptype, &bitrate);
		if (ret)
			return ret;
	}

	return 0;
}

/* TODO : how to export this to userspace */
static int vidc_vpe_csc_601_to_709 = 0;

static int venc_set_csc(struct vidc_inst *inst)
{
	struct hfi_device *hfi = &inst->core->hfi;
	struct device *dev = inst->core->dev;
	struct hal_vpe_color_space_conversion vpe_csc;
	enum hal_property ptype = HAL_PARAM_VPE_COLOR_SPACE_CONVERSION;
	int ret, count = 0;

	if (!vidc_vpe_csc_601_to_709)
		return 0;

	while (count < HAL_MAX_MATRIX_COEFFS) {
		if (count < HAL_MAX_BIAS_COEFFS)
			vpe_csc.csc_bias[count] =
				vpe_csc_601_to_709_bias_coeff[count];
		if (count < HAL_MAX_LIMIT_COEFFS)
			vpe_csc.csc_limit[count] =
				vpe_csc_601_to_709_limit_coeff[count];
		vpe_csc.csc_matrix[count] =
			vpe_csc_601_to_709_matrix_coeff[count];
		count++;
	}

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype,
					    &vpe_csc);
	if (ret) {
		dev_err(dev, "setting VPE coefficients failed\n");
		return ret;
	}

	return 0;
}

static int venc_v4l2_to_hal(int id, int value)
{
	switch (id) {
	/* MPEG4 */
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		switch (value) {
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_0:
		default:
			return HAL_MPEG4_LEVEL_0;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_0B:
			return HAL_MPEG4_LEVEL_0b;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_1:
			return HAL_MPEG4_LEVEL_1;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_2:
			return HAL_MPEG4_LEVEL_2;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_3:
			return HAL_MPEG4_LEVEL_3;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_4:
			return HAL_MPEG4_LEVEL_4;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_5:
			return HAL_MPEG4_LEVEL_5;
		}
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE:
		default:
			return HAL_MPEG4_PROFILE_SIMPLE;
		case V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE:
			return HAL_MPEG4_PROFILE_ADVANCEDSIMPLE;
		}
	/* H264 */
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
			return HAL_H264_PROFILE_BASELINE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
			return HAL_H264_PROFILE_CONSTRAINED_BASE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
			return HAL_H264_PROFILE_MAIN;
		case V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED:
			return HAL_H264_PROFILE_EXTENDED;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
		default:
			return HAL_H264_PROFILE_HIGH;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10:
			return HAL_H264_PROFILE_HIGH10;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422:
			return HAL_H264_PROFILE_HIGH422;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE:
			return HAL_H264_PROFILE_HIGH444;
		}
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_0:
			return HAL_H264_LEVEL_1;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1B:
			return HAL_H264_LEVEL_1b;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_1:
			return HAL_H264_LEVEL_11;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_2:
			return HAL_H264_LEVEL_12;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_3:
			return HAL_H264_LEVEL_13;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_0:
			return HAL_H264_LEVEL_2;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_1:
			return HAL_H264_LEVEL_21;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_2:
			return HAL_H264_LEVEL_22;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_0:
			return HAL_H264_LEVEL_3;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_1:
			return HAL_H264_LEVEL_31;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_2:
			return HAL_H264_LEVEL_32;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_0:
			return HAL_H264_LEVEL_4;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_1:
			return HAL_H264_LEVEL_41;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_2:
			return HAL_H264_LEVEL_42;
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_0:
		default:
			return HAL_H264_LEVEL_5;
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_1:
			return HAL_H264_LEVEL_51;
		}
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC:
		default:
			return HAL_H264_ENTROPY_CAVLC;
		case V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC:
			return HAL_H264_ENTROPY_CABAC;
		}
	case V4L2_CID_MPEG_VIDEO_VPX_PROFILE:
		switch (value) {
		case 0:
		default:
			return HAL_VPX_PROFILE_VERSION_0;
		case 1:
			return HAL_VPX_PROFILE_VERSION_1;
		case 2:
			return HAL_VPX_PROFILE_VERSION_2;
		case 3:
			return HAL_VPX_PROFILE_VERSION_3;
		}
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED:
		default:
			return HAL_H264_DB_MODE_DISABLE;
		case V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED:
			return HAL_H264_DB_MODE_ALL_BOUNDARY;
		case V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED_AT_SLICE_BOUNDARY:
			return HAL_H264_DB_MODE_SKIP_SLICE_BOUNDARY;
		}
	}

	return 0;
}

static int venc_set_properties(struct vidc_inst *inst)
{
	struct hfi_device *hfi = &inst->core->hfi;
	struct venc_controls *ctr = &inst->controls.enc;
	struct device *dev = inst->core->dev;
	struct hal_framerate framerate;
	struct hal_bitrate brate;
	struct hal_profile_level pl;
	struct hal_intra_period intra_period;
	struct hal_idr_period idrp;
	u32 ptype, rate_control, bitrate, profile = 0, level = 0;
	int ret;

	ptype = HAL_CONFIG_FRAME_RATE;
	framerate.buffer_type = HAL_BUFFER_OUTPUT;
	framerate.framerate = inst->fps * (1 << 16);

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype,
					    &framerate);
	if (ret) {
		dev_err(dev, "set framerate failed (%d)\n", ret);
		return ret;
	}

	dev_dbg(dev, "framerate %u\n", framerate.framerate);

	if (inst->fmt_cap->pixfmt == V4L2_PIX_FMT_H264) {
		struct hal_h264_vui_timing_info info;

		ptype = HAL_PARAM_VENC_H264_VUI_TIMING_INFO;
		info.enable = 1;
		info.fixed_framerate = 1;
		info.time_scale = NSEC_PER_SEC;

		ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype,
						    &info);
		if (ret) {
			dev_err(dev, "set timing info failed (%d)\n", ret);
			return ret;
		}
	}

	ptype = HAL_CONFIG_VENC_IDR_PERIOD;
	idrp.idr_period = ctr->gop_size;
	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype, &idrp);
	if (ret) {
		dev_err(dev, "set IDR period failed (%d)\n", ret);
		return ret;
	}

	dev_dbg(dev, "IDR period (GOP size): %u\n", idrp.idr_period);

	if (ctr->num_b_frames) {
		u32 max_num_b_frames = NUM_B_FRAMES_MAX;

		ptype = HAL_PARAM_VENC_MAX_NUM_B_FRAMES;
		ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst,
						    ptype, &max_num_b_frames);
		if (ret) {
			dev_err(dev, "set max bframes failed (%d)\n", ret);
			return ret;
		}
	}

	/* intra_period = pframes + bframes + 1 */
	if (!ctr->num_p_frames)
		ctr->num_p_frames = 2 * 15 - 1,

	ptype = HAL_CONFIG_VENC_INTRA_PERIOD;
	intra_period.pframes = ctr->num_p_frames;
	intra_period.bframes = ctr->num_b_frames;

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype,
					    &intra_period);
	if (ret) {
		dev_err(dev, "set intra period failed (%d)\n", ret);
		return ret;
	}

	dev_dbg(dev, "intra: bframe:%u, pframes:%u\n", intra_period.bframes,
		intra_period.pframes);

	if (ctr->bitrate_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR)
		rate_control = HAL_RATE_CONTROL_VBR_CFR;
	else
		rate_control = HAL_RATE_CONTROL_CBR_CFR;

	ptype = HAL_PARAM_VENC_RATE_CONTROL;
	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype,
					    &rate_control);
	if (ret) {
		dev_err(dev, "set rate control failed (%d)\n", ret);
		return ret;
	}

	if (!ctr->bitrate)
		bitrate = 64000;
	else
		bitrate = ctr->bitrate;

	ptype = HAL_CONFIG_VENC_TARGET_BITRATE;
	brate.bitrate = bitrate;
	brate.layer_id = 0;

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype, &brate);
	if (ret) {
		dev_err(dev, "set bitrate failed (%d)\n", ret);
		return ret;
	}

	dev_dbg(dev, "bitrate:%u\n", bitrate);

	if (!ctr->bitrate_peak)
		bitrate *= 2;
	else
		bitrate = ctr->bitrate_peak;

	ptype = HAL_CONFIG_VENC_MAX_BITRATE;
	brate.bitrate = bitrate;
	brate.layer_id = 0;

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype, &brate);
	if (ret) {
		dev_err(dev, "set max bitrate failed (%d)\n", ret);
		return ret;
	}

	dev_dbg(dev, "max bitrate:%u\n", bitrate);

	if (inst->fmt_cap->pixfmt == V4L2_PIX_FMT_H264) {
		profile = venc_v4l2_to_hal(V4L2_CID_MPEG_VIDEO_H264_PROFILE,
					   ctr->profile);
		level = venc_v4l2_to_hal(V4L2_CID_MPEG_VIDEO_H264_LEVEL,
					 ctr->level);
	} else if (inst->fmt_cap->pixfmt == V4L2_PIX_FMT_VP8) {
		profile = venc_v4l2_to_hal(V4L2_CID_MPEG_VIDEO_VPX_PROFILE,
					   ctr->profile);
		level = HAL_VPX_PROFILE_UNUSED;
	} else if (inst->fmt_cap->pixfmt == V4L2_PIX_FMT_MPEG4) {
		profile = venc_v4l2_to_hal(V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE,
					   ctr->profile);
		level = venc_v4l2_to_hal(V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL,
					 ctr->level);
	} else if (inst->fmt_cap->pixfmt == V4L2_PIX_FMT_H263) {
		profile = 0;
		level = 0;
	}

	ptype = HAL_PARAM_PROFILE_LEVEL_CURRENT;
	pl.profile = profile;
	pl.level = level;

	dev_dbg(dev, "profile:%x, level:%x\n", pl.profile, pl.level);

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype, &pl);
	if (ret) {
		dev_err(dev, "set profile and level failed (%d)\n", ret);
		return ret;
	}

	ret = venc_set_csc(inst);
	if (ret)
		return ret;

	return 0;
}

static int
venc_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	strlcpy(cap->driver, VIDC_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, "video encoder", sizeof(cap->card));
	strlcpy(cap->bus_info, "platform:vidc", sizeof(cap->bus_info));

	cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int venc_enum_fmt(struct file *file, void *fh, struct v4l2_fmtdesc *f)
{
	const struct vidc_format *fmt;

	fmt = find_format_by_index(f->index, f->type);

	memset(f->reserved, 0 , sizeof(f->reserved));

	if (!fmt)
		return -EINVAL;

	f->pixelformat = fmt->pixfmt;

	return 0;
}

static const struct vidc_format *
venc_try_fmt_common(struct vidc_inst *inst, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pixmp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = pixmp->plane_fmt;
	struct hal_session_init_done *cap = &inst->hfi_inst->caps;
	const struct vidc_format *fmt;
	unsigned int p;

	memset(pfmt[0].reserved, 0, sizeof(pfmt[0].reserved));
	memset(pixmp->reserved, 0, sizeof(pixmp->reserved));

	fmt = find_format(pixmp->pixelformat, f->type);
	if (!fmt) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			pixmp->pixelformat = V4L2_PIX_FMT_H264;
		else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			pixmp->pixelformat = V4L2_PIX_FMT_NV12;
		else
			return NULL;
		fmt = find_format(pixmp->pixelformat, f->type);
		pixmp->width = 1280;
		pixmp->height = 720;
	}

//	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		pixmp->height = ALIGN(pixmp->height, 32);

	pixmp->width = clamp(pixmp->width, cap->width.min, cap->width.max);
	pixmp->height = clamp(pixmp->height, cap->height.min, cap->height.max);
	if (pixmp->field == V4L2_FIELD_ANY)
		pixmp->field = V4L2_FIELD_NONE;
	if (pixmp->width >= 1280)
		pixmp->colorspace = V4L2_COLORSPACE_REC709;
	else
		pixmp->colorspace = V4L2_COLORSPACE_SMPTE170M;
	pixmp->ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(pixmp->colorspace);
	pixmp->quantization = V4L2_MAP_QUANTIZATION_DEFAULT(0,
							pixmp->colorspace,
							pixmp->ycbcr_enc);
	pixmp->xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(pixmp->colorspace);
	pixmp->num_planes = fmt->num_planes;
	pixmp->flags = 0;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		for (p = 0; p < pixmp->num_planes; p++) {
			pfmt[p].sizeimage = get_framesize_nv12(p, pixmp->height,
							       pixmp->width);

			pfmt[p].bytesperline = ALIGN(pixmp->width, 128);
		}
	} else {
		pfmt[0].sizeimage = get_framesize_compressed(0, pixmp->height,
							     pixmp->width);
		pfmt[0].bytesperline = 0;
	}

	return fmt;
}

static int venc_try_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct vidc_inst *inst = to_inst(file);
	const struct vidc_format *fmt;

	fmt = venc_try_fmt_common(inst, f);
	if (!fmt)
		return -EINVAL;

	return 0;
}

static int venc_s_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct vidc_inst *inst = to_inst(file);
	struct device *dev = inst->core->dev;
	struct v4l2_pix_format_mplane *pixmp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = pixmp->plane_fmt;
	struct v4l2_pix_format_mplane orig_pixmp;
	const struct vidc_format *fmt;
	struct v4l2_format format;
	u32 pixfmt_out = 0, pixfmt_cap = 0;

	dev_dbg(dev, "%s: type:%u, %ux%u, pixfmt:%08x\n", __func__, f->type,
		pixmp->width, pixmp->height, pixmp->pixelformat);

	orig_pixmp = *pixmp;

	fmt = venc_try_fmt_common(inst, f);
	if (!fmt)
		return -EINVAL;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		pixfmt_out = pixmp->pixelformat;
		pixfmt_cap = inst->fmt_cap->pixfmt;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pixfmt_cap = pixmp->pixelformat;
		pixfmt_out = inst->fmt_out->pixfmt;
	}

	memset(&format, 0, sizeof(format));

	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	format.fmt.pix_mp.pixelformat = pixfmt_out;
	format.fmt.pix_mp.width = orig_pixmp.width;
	format.fmt.pix_mp.height = orig_pixmp.height;
	venc_try_fmt_common(inst, &format);
	inst->out_width = format.fmt.pix_mp.width;
	inst->out_height = format.fmt.pix_mp.height;

	memset(&format, 0, sizeof(format));

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	format.fmt.pix_mp.pixelformat = pixfmt_cap;
	format.fmt.pix_mp.width = orig_pixmp.width;
	format.fmt.pix_mp.height = orig_pixmp.height;
	venc_try_fmt_common(inst, &format);
	inst->width = format.fmt.pix_mp.width;
	inst->height = format.fmt.pix_mp.height;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		inst->fmt_out = fmt;
	else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		inst->fmt_cap = fmt;

	dev_dbg(dev, "%s: got type:%u, %ux%u, pixfmt:%08x (size %u, bpl %u)\n",
		__func__, f->type, pixmp->width, pixmp->height,
		pixmp->pixelformat, pfmt[0].sizeimage, pfmt[0].bytesperline);

	dev_dbg(dev, "%s: instance fmts: input %ux%u, output %ux%u\n", __func__,
		inst->out_width, inst->out_height, inst->width, inst->height);

	return 0;
}

static int venc_g_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct vidc_inst *inst = to_inst(file);
	struct device *dev = inst->core->dev;
	struct v4l2_pix_format_mplane *pixmp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = pixmp->plane_fmt;
	const struct vidc_format *fmt;

	dev_dbg(dev, "%s: type:%u, %ux%u, pixfmt:%08x\n", __func__, f->type,
		pixmp->width, pixmp->height, pixmp->pixelformat);

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		fmt = inst->fmt_cap;
	else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		fmt = inst->fmt_out;
	else
		return -EINVAL;

	pixmp->pixelformat = fmt->pixfmt;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pixmp->width = inst->width;
		pixmp->height = inst->height;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		pixmp->width = inst->out_width;
		pixmp->height = inst->out_height;
	}

	venc_try_fmt_common(inst, f);

	dev_dbg(dev, "%s: got type:%u, %ux%u, pixfmt:%08x (size %u, bpl %u)\n",
		__func__, f->type, pixmp->width, pixmp->height,
		pixmp->pixelformat, pfmt[0].sizeimage, pfmt[0].bytesperline);

	return 0;
}

static int
venc_reqbufs(struct file *file, void *fh, struct v4l2_requestbuffers *b)
{
	struct vidc_inst *inst = to_inst(file);
	struct vb2_queue *queue = vidc_to_vb2q(inst, b->type);

	if (!queue)
		return -EINVAL;

	if (!b->count)
		vb2_core_queue_release(queue);

	return vb2_reqbufs(queue, b);
}

static int venc_querybuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct vidc_inst *inst = to_inst(file);
	struct vb2_queue *queue = vidc_to_vb2q(inst, b->type);
	unsigned int p;
	int ret;

	ret = vb2_querybuf(queue, b);
	if (ret)
		return ret;

	if (b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
	    b->memory == V4L2_MEMORY_MMAP) {
		for (p = 0; p < b->length; p++)
			b->m.planes[p].m.mem_offset += DST_QUEUE_OFF_BASE;
	}

	return 0;
}

static int venc_create_bufs(struct file *file, void *fh,
			    struct v4l2_create_buffers *b)
{
	struct vidc_inst *inst = to_inst(file);
	struct vb2_queue *queue = vidc_to_vb2q(inst, b->format.type);

	if (!queue)
		return -EINVAL;

	return vb2_create_bufs(queue, b);
}

static int venc_prepare_buf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct vidc_inst *inst = to_inst(file);
	struct vb2_queue *queue = vidc_to_vb2q(inst, b->type);

	if (!queue)
		return -EINVAL;

	return vb2_prepare_buf(queue, b);
}

static int venc_qbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct vidc_inst *inst = to_inst(file);
	struct vb2_queue *queue = vidc_to_vb2q(inst, b->type);

	if (!queue)
		return -EINVAL;

	return vb2_qbuf(queue, b);
}

static int
venc_exportbuf(struct file *file, void *fh, struct v4l2_exportbuffer *b)
{
	struct vidc_inst *inst = to_inst(file);
	struct vb2_queue *queue = vidc_to_vb2q(inst, b->type);

	if (!queue)
		return -EINVAL;

	return vb2_expbuf(queue, b);
}

static int venc_dqbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct vidc_inst *inst = to_inst(file);
	struct vb2_queue *queue = vidc_to_vb2q(inst, b->type);

	if (!queue)
		return -EINVAL;

	return vb2_dqbuf(queue, b, file->f_flags & O_NONBLOCK);
}

static int venc_streamon(struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct vidc_inst *inst = to_inst(file);
	struct vb2_queue *queue = vidc_to_vb2q(inst, type);

	if (!queue)
		return -EINVAL;

	return vb2_streamon(queue, type);
}

static int venc_streamoff(struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct vidc_inst *inst = to_inst(file);
	struct vb2_queue *queue = vidc_to_vb2q(inst, type);

	if (!queue)
		return -EINVAL;

	return vb2_streamoff(queue, type);
}

static int venc_s_parm(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct vidc_inst *inst = to_inst(file);
	struct v4l2_outputparm *out = &a->parm.output;
	struct v4l2_fract *timeperframe = &out->timeperframe;
	u64 us_per_frame, fps;

	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	memset(out->reserved, 0, sizeof(out->reserved));
	if (!timeperframe->denominator)
		timeperframe->denominator = inst->timeperframe.denominator;
	if (!timeperframe->numerator)
		timeperframe->numerator = inst->timeperframe.numerator;
	out->writebuffers = 0;
	out->extendedmode = 0;
	out->capability = V4L2_CAP_TIMEPERFRAME;
	us_per_frame = timeperframe->numerator * (u64)USEC_PER_SEC;
	do_div(us_per_frame, timeperframe->denominator);

	if (!us_per_frame)
		return -EINVAL;

	fps = (u64)USEC_PER_SEC;
	do_div(fps, us_per_frame);

	inst->fps = fps;
	inst->timeperframe = *timeperframe;

	dev_dbg(inst->core->dev, "s_parm %llu\n", inst->fps);

	return 0;
}

static int venc_g_parm(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct vidc_inst *inst = to_inst(file);

	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	memset(a, 0, sizeof(*a));
	a->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	a->parm.output.capability = V4L2_CAP_TIMEPERFRAME;
	a->parm.output.timeperframe = inst->timeperframe;

	return 0;
}

static int venc_enum_framesizes(struct file *file, void *fh,
				struct v4l2_frmsizeenum *fsize)
{
	struct vidc_inst *inst = to_inst(file);
	struct hal_session_init_done *cap = &inst->hfi_inst->caps;
	const struct vidc_format *fmt;

	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;

	fmt = find_format(fsize->pixel_format,
			  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (!fmt) {
		fmt = find_format(fsize->pixel_format,
				  V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (!fmt)
			return -EINVAL;
	}

	if (fsize->index)
		return -EINVAL;

	fsize->stepwise.min_width = cap->width.min;
	fsize->stepwise.max_width = cap->width.max;
	fsize->stepwise.step_width = cap->width.step_size;
	fsize->stepwise.min_height = cap->height.min;
	fsize->stepwise.max_height = cap->height.max;
	fsize->stepwise.step_height = cap->height.step_size;

	return 0;
}

static int venc_enum_frameintervals(struct file *file, void *fh,
				    struct v4l2_frmivalenum *fival)
{
	struct vidc_inst *inst = to_inst(file);
	struct hal_session_init_done *cap = &inst->hfi_inst->caps;
	const struct vidc_format *fmt;

	fival->type = V4L2_FRMIVAL_TYPE_STEPWISE;

	fmt = find_format(fival->pixel_format,
			  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (!fmt) {
		fmt = find_format(fival->pixel_format,
				  V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (!fmt)
			return -EINVAL;
	}

	if (fival->index)
		return -EINVAL;

	if (!fival->width || !fival->height)
		return -EINVAL;

	if (fival->width > cap->width.max || fival->width < cap->width.min)
		return -EINVAL;

	if (fival->height > cap->height.max ||
	    fival->height < cap->height.min)
		return -EINVAL;

	fival->stepwise.min.numerator = 1;
	fival->stepwise.min.denominator = cap->frame_rate.max;

	fival->stepwise.max.numerator = 1;
	fival->stepwise.max.denominator = cap->frame_rate.min;

	fival->stepwise.step.numerator = 1;
	fival->stepwise.step.denominator = cap->frame_rate.max;

	return 0;
}

static int venc_subscribe_event(struct v4l2_fh *fh,
				const struct  v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subscribe_event(fh, sub);
	default:
		return -EINVAL;
	}
}

static const struct v4l2_ioctl_ops venc_ioctl_ops = {
	.vidioc_querycap = venc_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = venc_enum_fmt,
	.vidioc_enum_fmt_vid_out_mplane = venc_enum_fmt,
	.vidioc_s_fmt_vid_cap_mplane = venc_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = venc_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane = venc_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = venc_g_fmt,
	.vidioc_try_fmt_vid_cap_mplane = venc_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = venc_try_fmt,
	.vidioc_reqbufs = venc_reqbufs,
	.vidioc_querybuf = venc_querybuf,
	.vidioc_create_bufs = venc_create_bufs,
	.vidioc_prepare_buf = venc_prepare_buf,
	.vidioc_qbuf = venc_qbuf,
	.vidioc_expbuf = venc_exportbuf,
	.vidioc_dqbuf = venc_dqbuf,
	.vidioc_streamon = venc_streamon,
	.vidioc_streamoff = venc_streamoff,
	.vidioc_s_parm = venc_s_parm,
	.vidioc_g_parm = venc_g_parm,
	.vidioc_enum_framesizes = venc_enum_framesizes,
	.vidioc_enum_frameintervals = venc_enum_frameintervals,
	.vidioc_subscribe_event = venc_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

static int venc_init_session(struct vidc_inst *inst)
{
	struct hfi_device *hfi = &inst->core->hfi;
	struct device *dev = inst->core->dev;
	u32 pixfmt = inst->fmt_cap->pixfmt;
	struct hal_framesize fs;
	enum hal_property ptype = HAL_PARAM_FRAME_SIZE;
	int ret;

	ret = vidc_hfi_session_init(hfi, inst->hfi_inst, pixfmt, VIDC_ENCODER);
	if (ret) {
		dev_err(dev, "%s: session init failed (%d)\n", __func__, ret);
		return ret;
	}

	fs.buffer_type = HAL_BUFFER_INPUT;
	fs.width = inst->out_width;
	fs.height = inst->out_height;

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype, &fs);
	if (ret) {
		dev_err(dev, "set framesize for input failed (%d)\n", ret);
		goto err;
	}

	fs.buffer_type = HAL_BUFFER_OUTPUT;
	fs.width = inst->width;
	fs.height = inst->height;

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype, &fs);
	if (ret) {
		dev_err(dev, "set framesize for output failed (%d)\n", ret);
		goto err;
	}

	pixfmt = inst->fmt_out->pixfmt;

	ret = vidc_set_color_format(inst, HAL_BUFFER_INPUT, pixfmt);
	if (ret)
		goto err;

	return 0;
err:
	vidc_hfi_session_deinit(hfi, inst->hfi_inst);
	return ret;
}

static int venc_cap_num_buffers(struct vidc_inst *inst,
				struct hal_buffer_requirements *bufreq)
{
	struct hfi_device *hfi = &inst->core->hfi;
	struct device *dev = inst->core->dev;
	int ret, ret2;

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(dev, "%s: pm_runtime_get_sync (%d)\n", __func__, ret);
		return ret;
	}

	ret = venc_init_session(inst);
	if (ret)
		goto put_sync;

	ret = vidc_bufrequirements(inst, HAL_BUFFER_OUTPUT, bufreq);

	vidc_hfi_session_deinit(hfi, inst->hfi_inst);

put_sync:
	ret2 = pm_runtime_put_sync(dev);
	if (ret2)
		dev_err(dev, "%s: pm_runtime_put_sync (%d)\n", __func__, ret);

	return ret ? ret : ret2;
}

static int venc_queue_setup(struct vb2_queue *q,
			    unsigned int *num_buffers,
			    unsigned int *num_planes, unsigned int sizes[],
			    void *alloc_ctxs[])
{
	struct vidc_inst *inst = vb2_get_drv_priv(q);
	struct device *dev = inst->core->dev;
	struct hal_buffer_requirements bufreq;
	unsigned int p;
	int ret = 0;

	dev_dbg(dev, "%s: type:%u\n", __func__, q->type);

	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*num_planes = inst->fmt_out->num_planes;
		*num_buffers = clamp_val(*num_buffers, 5, VIDEO_MAX_FRAME);
		inst->num_input_bufs = *num_buffers;

		for (p = 0; p < *num_planes; ++p) {
			sizes[p] = get_framesize_nv12(p, inst->height,
						      inst->width);
			alloc_ctxs[p] = inst->vb2_ctx_out;
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		*num_planes = inst->fmt_cap->num_planes;

		ret = venc_cap_num_buffers(inst, &bufreq);
		if (ret)
			break;

		*num_buffers = max(*num_buffers, bufreq.count_actual);
		inst->num_output_bufs = *num_buffers;

		sizes[0] = get_framesize_compressed(0, inst->height,
						    inst->width);
		alloc_ctxs[0] = inst->vb2_ctx_cap;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int venc_check_configuration(struct vidc_inst *inst)
{
	struct hal_buffer_requirements bufreq;
	struct device *dev = inst->core->dev;
	int ret;

	ret = vidc_bufrequirements(inst, HAL_BUFFER_OUTPUT, &bufreq);
	if (ret)
		return ret;

	if (inst->num_output_bufs < bufreq.count_actual ||
	    inst->num_output_bufs < bufreq.count_min) {
		dev_err(dev, "%s: output buffer expectation (%u - %u min %u)\n",
			__func__, inst->num_output_bufs, bufreq.count_actual,
			bufreq.count_min);
		ret = -EINVAL;
	}

	ret = vidc_bufrequirements(inst, HAL_BUFFER_INPUT, &bufreq);
	if (ret)
		return ret;

	if (inst->num_input_bufs < bufreq.count_actual ||
	    inst->num_input_bufs < bufreq.count_min) {
		dev_err(dev, "%s: input buffer expectation (%u - %u min %u)\n",
			__func__, inst->num_input_bufs, bufreq.count_actual,
			bufreq.count_min);
		ret = -EINVAL;
	}

	return 0;
}

static int venc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct vidc_inst *inst = vb2_get_drv_priv(q);
	struct hfi_device *hfi = &inst->core->hfi;
	struct device *dev = inst->core->dev;
	struct hal_buffer_count_actual buf_count;
	enum hal_property ptype = HAL_PARAM_BUFFER_COUNT_ACTUAL;
	struct vb2_queue *queue;
	int ret;

	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		queue = &inst->bufq_cap;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		queue = &inst->bufq_out;
		break;
	default:
		return -EINVAL;
	}

	if (!vb2_is_streaming(queue))
		return 0;

	inst->sequence = 0;

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(dev, "%s: pm_runtime_get_sync (%d)\n", __func__, ret);
		return ret;
	}

	ret = venc_init_session(inst);
	if (ret)
		goto put_sync;

	ret = venc_set_properties(inst);
	if (ret) {
		dev_err(dev, "set properties (%d)\n", ret);
		goto deinit_sess;
	}

	ret = venc_check_configuration(inst);
	if (ret)
		goto deinit_sess;

	buf_count.type = HAL_BUFFER_OUTPUT;
	buf_count.count_actual = inst->num_output_bufs;

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype,
					    &buf_count);
	if (ret) {
		dev_err(dev, "set output buf count failed (%d)", ret);
		goto deinit_sess;
	}

	buf_count.type = HAL_BUFFER_INPUT;
	buf_count.count_actual = inst->num_input_bufs;

	ret = vidc_hfi_session_set_property(hfi, inst->hfi_inst, ptype,
					    &buf_count);
	if (ret) {
		dev_err(dev, "set input buf count failed (%d)", ret);
		goto deinit_sess;
	}

	ret = vidc_vb2_start_streaming(inst);
	if (ret) {
		dev_err(dev, "start streaming fail (%d)\n", ret);
		goto deinit_sess;
	}

	return 0;

deinit_sess:
	vidc_hfi_session_deinit(hfi, inst->hfi_inst);
put_sync:
	pm_runtime_put_sync(dev);
	return ret;
}

static const struct vb2_ops venc_vb2_ops = {
	.queue_setup = venc_queue_setup,
	.buf_init = vidc_vb2_buf_init,
	.buf_prepare = vidc_vb2_buf_prepare,
	.start_streaming = venc_start_streaming,
	.stop_streaming = vidc_vb2_stop_streaming,
	.buf_queue = vidc_vb2_buf_queue,
};

static int venc_empty_buf_done(struct hfi_device_inst *hfi_inst, u32 addr,
			       u32 bytesused, u32 data_offset, u32 flags)
{
	struct vidc_inst *inst = hfi_inst->ops_priv;
	struct device *dev = inst->core->dev;
	struct vb2_v4l2_buffer *vbuf;
	struct vb2_buffer *vb;

	vbuf = vidc_vb2_find_buf(inst, addr);
	if (!vbuf)
		return -EINVAL;

	vb = &vbuf->vb2_buf;
	vb->planes[0].bytesused = bytesused;
	vb->planes[0].data_offset = data_offset;
	vbuf->flags = flags;

	if (data_offset > vb->planes[0].length)
		dev_dbg(dev, "data_offset overflow length\n");

	if (bytesused > vb->planes[0].length)
		dev_dbg(dev, "bytesused overflow length\n");

	if (flags & HAL_ERR_NOT_SUPPORTED)
		dev_dbg(dev, "unsupported input stream\n");

	if (flags & HAL_ERR_BITSTREAM_ERR)
		dev_dbg(dev, "corrupted input stream\n");

	if (flags & HAL_ERR_START_CODE_NOT_FOUND)
		dev_dbg(dev, "start code not found\n");

	vb2_buffer_done(vb, VB2_BUF_STATE_DONE);

	dev_dbg(dev, "%s: vb:%p, type:%d, addr:%x\n", __func__, vb,
		vb->vb2_queue->type, addr);

	return 0;
}

static int venc_fill_buf_done(struct hfi_device_inst *hfi_inst, u32 addr,
			      u32 bytesused, u32 data_offset, u32 flags,
			      struct timeval *timestamp)
{
	struct vidc_inst *inst = hfi_inst->ops_priv;
	struct device *dev = inst->core->dev;
	struct vb2_v4l2_buffer *vbuf;
	struct vb2_buffer *vb;

	vbuf = vidc_vb2_find_buf(inst, addr);
	if (!vbuf)
		return -EINVAL;

	vb = &vbuf->vb2_buf;
	vb->planes[0].bytesused = bytesused;
	vb->planes[0].data_offset = data_offset;
	vb->timestamp = timeval_to_ns(timestamp);
	vbuf->flags = flags;
	vbuf->sequence = inst->sequence++;

	if (data_offset > vb->planes[0].length)
		dev_warn(dev, "overflow data_offset:%d, length:%d\n",
			 vb->planes[0].data_offset, vb->planes[0].length);

	if (bytesused > vb->planes[0].length)
		dev_warn(dev, "overflow bytesused:%d, length:%d\n",
			 vb->planes[0].bytesused, vb->planes[0].length);

	vb2_buffer_done(vb, VB2_BUF_STATE_DONE);

	dev_dbg(dev, "%s: vb:%p, type:%d, addr:%x, bytesused:%u, data_off:%u\n",
		__func__, vb, vb->vb2_queue->type, addr, bytesused,
		data_offset);

	return 0;
}

static int venc_event_notify(struct hfi_device_inst *hfi_inst, u32 event,
			     struct hal_cb_event *data)
{
	struct vidc_inst *inst = hfi_inst->ops_priv;
	struct device *dev = inst->core->dev;

	switch (event) {
	case EVT_SESSION_ERROR:
		if (hfi_inst) {
			mutex_lock(&hfi_inst->lock);
			inst->hfi_inst->state = INST_INVALID;
			mutex_unlock(&hfi_inst->lock);
		}
		dev_warn(dev, "enc: event session error (inst:%p)\n", hfi_inst);
		dev_warn(dev, "enc: last ptype %x\n", hfi_inst->last_ptype);
		break;
	case EVT_SYS_EVENT_CHANGE:
		switch (data->event_type) {
		case HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES:
			dev_dbg(dev, "event sufficient resources\n");
			break;
		case HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES:
			inst->reconfig_height = data->height;
			inst->reconfig_width = data->width;
			inst->in_reconfig = true;
			dev_dbg(dev, "event not sufficient resources\n");
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return 0;
}

static const struct hfi_inst_ops venc_hfi_ops = {
	.empty_buf_done = venc_empty_buf_done,
	.fill_buf_done = venc_fill_buf_done,
	.event_notify = venc_event_notify,
};

static void venc_inst_init(struct vidc_inst *inst)
{
	struct hfi_device_inst *hfi_inst = inst->hfi_inst;
	struct hal_session_init_done *caps = &hfi_inst->caps;

	inst->fmt_cap = &venc_formats[2];
	inst->fmt_out = &venc_formats[0];
	inst->width = 1280;
	inst->height = ALIGN(720, 32);
	inst->fps = 15;
	inst->timeperframe.numerator = 1;
	inst->timeperframe.denominator = 15;

	caps->width.min = 64;
	caps->width.max = 1920;
	caps->width.step_size = 1;
	caps->height.min = 64;
	caps->height.max = ALIGN(1080, 32);
	caps->height.step_size = 1;
	caps->frame_rate.min = 1;
	caps->frame_rate.max = 30;
	caps->frame_rate.step_size = 1;
	caps->mbs_per_frame.min = 16;
	caps->mbs_per_frame.max = 8160;
}

int venc_init(struct vidc_core *core, struct video_device *enc)
{
	struct device *dev = core->dev;
	int ret;

	/* setup the decoder device */
	enc->release = video_device_release_empty;
	enc->fops = &vidc_fops;
	enc->ioctl_ops = &venc_ioctl_ops;
	enc->vfl_dir = VFL_DIR_M2M;
	enc->v4l2_dev = &core->v4l2_dev;

	ret = video_register_device(enc, VFL_TYPE_GRABBER, 33);
	if (ret) {
		dev_err(dev, "failed to register video encoder device");
		return ret;
	}

	video_set_drvdata(enc, core);

	return 0;
}

void venc_deinit(struct vidc_core *core, struct video_device *enc)
{
	video_unregister_device(enc);
}

int venc_open(struct vidc_inst *inst)
{
	struct device *dev = inst->core->dev;
	struct hfi_device *hfi = &inst->core->hfi;
	struct vb2_queue *q;
	int ret;

	ret = venc_ctrl_init(inst);
	if (ret)
		return ret;

	inst->hfi_inst = vidc_hfi_session_create(hfi, &venc_hfi_ops, inst);
	if (IS_ERR(inst->hfi_inst)) {
		ret = PTR_ERR(inst->hfi_inst);
		goto err_ctrl_deinit;
	}

	venc_inst_init(inst);

	inst->vb2_ctx_cap = vb2_dma_sg_init_ctx(dev);
	if (IS_ERR(inst->vb2_ctx_cap)) {
		ret = PTR_ERR(inst->vb2_ctx_cap);
		goto err_session_destroy;
	}

	inst->vb2_ctx_out = vb2_dma_sg_init_ctx(dev);
	if (IS_ERR(inst->vb2_ctx_out)) {
		ret = PTR_ERR(inst->vb2_ctx_out);
		goto err_cleanup_cap;
	}

	q = &inst->bufq_cap;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_MMAP | VB2_DMABUF;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	q->ops = &venc_vb2_ops;
	q->mem_ops = &vb2_dma_sg_memops;
	q->drv_priv = inst;
	q->buf_struct_size = sizeof(struct vidc_buffer);
	q->allow_zero_bytesused = 1;
	ret = vb2_queue_init(q);
	if (ret)
		goto err_cleanup_out;

	q = &inst->bufq_out;
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	q->io_modes = VB2_MMAP | VB2_DMABUF;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	q->ops = &venc_vb2_ops;
	q->mem_ops = &vb2_dma_sg_memops;
	q->drv_priv = inst;
	q->buf_struct_size = sizeof(struct vidc_buffer);
	q->allow_zero_bytesused = 1;
	ret = vb2_queue_init(q);
	if (ret)
		goto err_cap_queue_release;

	return 0;

err_cap_queue_release:
	vb2_queue_release(&inst->bufq_cap);
err_cleanup_out:
	vb2_dma_sg_cleanup_ctx(inst->vb2_ctx_out);
err_cleanup_cap:
	vb2_dma_sg_cleanup_ctx(inst->vb2_ctx_cap);
err_session_destroy:
	vidc_hfi_session_destroy(hfi, inst->hfi_inst);
	inst->hfi_inst = NULL;
err_ctrl_deinit:
	venc_ctrl_deinit(inst);
	return ret;
}

void venc_close(struct vidc_inst *inst)
{
	struct hfi_device *hfi = &inst->core->hfi;

	vb2_queue_release(&inst->bufq_out);
	vb2_queue_release(&inst->bufq_cap);

	vb2_dma_sg_cleanup_ctx(inst->vb2_ctx_out);
	vb2_dma_sg_cleanup_ctx(inst->vb2_ctx_cap);

	venc_ctrl_deinit(inst);
	vidc_hfi_session_destroy(hfi, inst->hfi_inst);
	inst->hfi_inst = NULL;
}
