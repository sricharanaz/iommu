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
#ifndef __HFI_H__
#define __HFI_H__

#include <linux/interrupt.h>

#include "resources.h"

#define HAL_BUFFERFLAG_EOS			0x1
#define HAL_BUFFERFLAG_STARTTIME		0x2
#define HAL_BUFFERFLAG_DECODEONLY		0x4
#define HAL_BUFFERFLAG_DATACORRUPT		0x8
#define HAL_BUFFERFLAG_ENDOFFRAME		0x10
#define HAL_BUFFERFLAG_SYNCFRAME		0x20
#define HAL_BUFFERFLAG_EXTRADATA		0x40
#define HAL_BUFFERFLAG_CODECCONFIG		0x80
#define HAL_BUFFERFLAG_TIMESTAMPINVALID		0x100
#define HAL_BUFFERFLAG_READONLY			0x200
#define HAL_BUFFERFLAG_ENDOFSUBFRAME		0x400
#define HAL_BUFFERFLAG_EOSEQ			0x200000
#define HAL_BUFFERFLAG_MBAFF			0x8000000
#define HAL_BUFFERFLAG_YUV_601_709_CSC_CLAMP	0x10000000
#define HAL_BUFFERFLAG_DROP_FRAME		0x20000000
#define HAL_BUFFERFLAG_TS_DISCONTINUITY		0x40000000
#define HAL_BUFFERFLAG_TS_ERROR			0x80000000

#define HAL_DEBUG_MSG_LOW			0x1
#define HAL_DEBUG_MSG_MEDIUM			0x2
#define HAL_DEBUG_MSG_HIGH			0x4
#define HAL_DEBUG_MSG_ERROR			0x8
#define HAL_DEBUG_MSG_FATAL			0x10
#define MAX_PROFILE_COUNT			16

#define HAL_MAX_MATRIX_COEFFS			9
#define HAL_MAX_BIAS_COEFFS			3
#define HAL_MAX_LIMIT_COEFFS			6

enum hal_error {
	HAL_ERR_NONE			= 0x00000000,
	HAL_ERR_FAIL			= 0x80000000,
	HAL_ERR_ALLOC_FAIL		= 0x80000001,
	HAL_ERR_ILLEGAL_OP		= 0x80000002,
	HAL_ERR_BAD_PARAM		= 0x80000003,
	HAL_ERR_BAD_HANDLE		= 0x80000004,
	HAL_ERR_NOT_SUPPORTED		= 0x80000005,
	HAL_ERR_BAD_STATE		= 0x80000006,
	HAL_ERR_MAX_CLIENTS		= 0x80000007,
	HAL_ERR_IFRAME_EXPECTED		= 0x80000008,
	HAL_ERR_HW_FATAL		= 0x80000009,
	HAL_ERR_BITSTREAM_ERR		= 0x8000000a,
	HAL_ERR_INDEX_NOMORE		= 0x8000000b,
	HAL_ERR_SEQHDR_PARSE_FAIL	= 0x8000000c,
	HAL_ERR_INSUFFICIENT_BUFFER	= 0x8000000d,
	HAL_ERR_BAD_POWER_STATE		= 0x8000000e,
	HAL_ERR_NO_VALID_SESSION	= 0x8000000f,
	HAL_ERR_TIMEOUT			= 0x80000010,
	HAL_ERR_CMDQFULL		= 0x80000011,
	HAL_ERR_START_CODE_NOT_FOUND	= 0x80000012,
	HAL_ERR_CLIENT_PRESENT		= 0x90000001,
	HAL_ERR_CLIENT_FATAL		= 0x90000002,
	HAL_ERR_CMD_QUEUE_FULL		= 0x90000003,
	HAL_ERR_UNUSED			= 0x10000000
};

#define HAL_BUFFER_MAX			11

enum hal_buffer_type {
	HAL_BUFFER_NONE			= 0x000,
	HAL_BUFFER_INPUT		= 0x001,
	HAL_BUFFER_OUTPUT		= 0x002,
	HAL_BUFFER_OUTPUT2		= 0x004,
	HAL_BUFFER_EXTRADATA_INPUT	= 0x008,
	HAL_BUFFER_EXTRADATA_OUTPUT	= 0x010,
	HAL_BUFFER_EXTRADATA_OUTPUT2	= 0x020,
	HAL_BUFFER_INTERNAL_SCRATCH	= 0x040,
	HAL_BUFFER_INTERNAL_SCRATCH_1	= 0x080,
	HAL_BUFFER_INTERNAL_SCRATCH_2	= 0x100,
	HAL_BUFFER_INTERNAL_PERSIST	= 0x200,
	HAL_BUFFER_INTERNAL_PERSIST_1	= 0x400,
};

enum hal_extradata_id {
	HAL_EXTRADATA_NONE,
	HAL_EXTRADATA_MB_QUANTIZATION,
	HAL_EXTRADATA_INTERLACE_VIDEO,
	HAL_EXTRADATA_VC1_FRAMEDISP,
	HAL_EXTRADATA_VC1_SEQDISP,
	HAL_EXTRADATA_TIMESTAMP,
	HAL_EXTRADATA_S3D_FRAME_PACKING,
	HAL_EXTRADATA_FRAME_RATE,
	HAL_EXTRADATA_PANSCAN_WINDOW,
	HAL_EXTRADATA_RECOVERY_POINT_SEI,
	HAL_EXTRADATA_MULTISLICE_INFO,
	HAL_EXTRADATA_INDEX,
	HAL_EXTRADATA_NUM_CONCEALED_MB,
	HAL_EXTRADATA_METADATA_FILLER,
	HAL_EXTRADATA_ASPECT_RATIO,
	HAL_EXTRADATA_MPEG2_SEQDISP,
	HAL_EXTRADATA_STREAM_USERDATA,
	HAL_EXTRADATA_FRAME_QP,
	HAL_EXTRADATA_FRAME_BITS_INFO,
	HAL_EXTRADATA_INPUT_CROP,
	HAL_EXTRADATA_DIGITAL_ZOOM,
	HAL_EXTRADATA_LTR_INFO,
	HAL_EXTRADATA_METADATA_MBI,
};

enum hal_property {
	HAL_CONFIG_FRAME_RATE					= 0x04000001,
	HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT			= 0x04000002,
	HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO	= 0x04000003,
	HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_INFO		= 0x04000004,
	HAL_PARAM_EXTRA_DATA_HEADER_CONFIG			= 0x04000005,
	HAL_PARAM_INDEX_EXTRADATA				= 0x04000006,
	HAL_PARAM_FRAME_SIZE					= 0x04000007,
	HAL_CONFIG_REALTIME					= 0x04000008,
	HAL_PARAM_BUFFER_COUNT_ACTUAL				= 0x04000009,
	HAL_PARAM_BUFFER_SIZE_ACTUAL				= 0x0400000a,
	HAL_PARAM_BUFFER_DISPLAY_HOLD_COUNT_ACTUAL		= 0x0400000b,
	HAL_PARAM_NAL_STREAM_FORMAT_SELECT			= 0x0400000c,
	HAL_PARAM_VDEC_OUTPUT_ORDER				= 0x0400000d,
	HAL_PARAM_VDEC_PICTURE_TYPE_DECODE			= 0x0400000e,
	HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO		= 0x0400000f,
	HAL_CONFIG_VDEC_POST_LOOP_DEBLOCKER			= 0x04000010,
	HAL_PARAM_VDEC_MULTI_STREAM				= 0x04000011,
	HAL_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT		= 0x04000012,
	HAL_PARAM_DIVX_FORMAT					= 0x04000013,
	HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING			= 0x04000014,
	HAL_PARAM_VDEC_CONTINUE_DATA_TRANSFER			= 0x04000015,
	HAL_CONFIG_VDEC_MB_ERROR_MAP				= 0x04000016,
	HAL_CONFIG_VENC_REQUEST_IFRAME				= 0x04000017,
	HAL_PARAM_VENC_MPEG4_SHORT_HEADER			= 0x04000018,
	HAL_PARAM_VENC_MPEG4_AC_PREDICTION			= 0x04000019,
	HAL_CONFIG_VENC_TARGET_BITRATE				= 0x0400001a,
	HAL_PARAM_PROFILE_LEVEL_CURRENT				= 0x0400001b,
	HAL_PARAM_VENC_H264_ENTROPY_CONTROL			= 0x0400001c,
	HAL_PARAM_VENC_RATE_CONTROL				= 0x0400001d,
	HAL_PARAM_VENC_MPEG4_TIME_RESOLUTION			= 0x0400001e,
	HAL_PARAM_VENC_MPEG4_HEADER_EXTENSION			= 0x0400001f,
	HAL_PARAM_VENC_H264_DEBLOCK_CONTROL			= 0x04000020,
	HAL_PARAM_VENC_TEMPORAL_SPATIAL_TRADEOFF		= 0x04000021,
	HAL_PARAM_VENC_SESSION_QP				= 0x04000022,
	HAL_PARAM_VENC_SESSION_QP_RANGE				= 0x04000023,
	HAL_CONFIG_VENC_INTRA_PERIOD				= 0x04000024,
	HAL_CONFIG_VENC_IDR_PERIOD				= 0x04000025,
	HAL_CONFIG_VPE_OPERATIONS				= 0x04000026,
	HAL_PARAM_VENC_INTRA_REFRESH				= 0x04000027,
	HAL_PARAM_VENC_MULTI_SLICE_CONTROL			= 0x04000028,
	HAL_CONFIG_VPE_DEINTERLACE				= 0x04000029,
	HAL_SYS_DEBUG_CONFIG					= 0x0400002a,
	HAL_CONFIG_BUFFER_REQUIREMENTS				= 0x0400002b,
	HAL_CONFIG_PRIORITY					= 0x0400002c,
	HAL_CONFIG_BATCH_INFO					= 0x0400002d,
	HAL_PARAM_METADATA_PASS_THROUGH				= 0x0400002e,
	HAL_SYS_IDLE_INDICATOR					= 0x0400002f,
	HAL_PARAM_UNCOMPRESSED_FORMAT_SUPPORTED			= 0x04000030,
	HAL_PARAM_INTERLACE_FORMAT_SUPPORTED			= 0x04000031,
	HAL_PARAM_CHROMA_SITE					= 0x04000032,
	HAL_PARAM_PROPERTIES_SUPPORTED				= 0x04000033,
	HAL_PARAM_PROFILE_LEVEL_SUPPORTED			= 0x04000034,
	HAL_PARAM_CAPABILITY_SUPPORTED				= 0x04000035,
	HAL_PARAM_NAL_STREAM_FORMAT_SUPPORTED			= 0x04000036,
	HAL_PARAM_MULTI_VIEW_FORMAT				= 0x04000037,
	HAL_PARAM_MAX_SEQUENCE_HEADER_SIZE			= 0x04000038,
	HAL_PARAM_CODEC_SUPPORTED				= 0x04000039,
	HAL_PARAM_VDEC_MULTI_VIEW_SELECT			= 0x0400003a,
	HAL_PARAM_VDEC_MB_QUANTIZATION				= 0x0400003b,
	HAL_PARAM_VDEC_NUM_CONCEALED_MB				= 0x0400003c,
	HAL_PARAM_VDEC_H264_ENTROPY_SWITCHING			= 0x0400003d,
	HAL_PARAM_VENC_SLICE_DELIVERY_MODE			= 0x0400003e,
	HAL_PARAM_VENC_MPEG4_DATA_PARTITIONING			= 0x0400003f,
	HAL_CONFIG_BUFFER_COUNT_ACTUAL				= 0x04000040,
	HAL_CONFIG_VDEC_MULTI_STREAM				= 0x04000041,
	HAL_PARAM_VENC_MULTI_SLICE_INFO				= 0x04000042,
	HAL_CONFIG_VENC_TIMESTAMP_SCALE				= 0x04000043,
	HAL_PARAM_VENC_LOW_LATENCY				= 0x04000044,
	HAL_PARAM_VENC_SYNC_FRAME_SEQUENCE_HEADER		= 0x04000045,
	HAL_PARAM_VDEC_SYNC_FRAME_DECODE			= 0x04000046,
	HAL_PARAM_VENC_H264_ENTROPY_CABAC_MODEL			= 0x04000047,
	HAL_CONFIG_VENC_MAX_BITRATE				= 0x04000048,
	HAL_PARAM_VENC_H264_VUI_TIMING_INFO			= 0x04000049,
	HAL_PARAM_VENC_H264_GENERATE_AUDNAL			= 0x0400004a,
	HAL_PARAM_VENC_MAX_NUM_B_FRAMES				= 0x0400004b,
	HAL_PARAM_BUFFER_ALLOC_MODE				= 0x0400004c,
	HAL_PARAM_VDEC_FRAME_ASSEMBLY				= 0x0400004d,
	HAL_PARAM_VENC_H264_VUI_BITSTREAM_RESTRC		= 0x0400004e,
	HAL_PARAM_VENC_PRESERVE_TEXT_QUALITY			= 0x0400004f,
	HAL_PARAM_VDEC_CONCEAL_COLOR				= 0x04000050,
	HAL_PARAM_VDEC_SCS_THRESHOLD				= 0x04000051,
	HAL_PARAM_GET_BUFFER_REQUIREMENTS			= 0x04000052,
	HAL_PARAM_MVC_BUFFER_LAYOUT				= 0x04000053,
	HAL_PARAM_VENC_LTRMODE					= 0x04000054,
	HAL_CONFIG_VENC_MARKLTRFRAME				= 0x04000055,
	HAL_CONFIG_VENC_USELTRFRAME				= 0x04000056,
	HAL_CONFIG_VENC_LTRPERIOD				= 0x04000057,
	HAL_CONFIG_VENC_HIER_P_NUM_FRAMES			= 0x04000058,
	HAL_PARAM_VENC_HIER_P_MAX_ENH_LAYERS			= 0x04000059,
	HAL_PARAM_VENC_DISABLE_RC_TIMESTAMP			= 0x0400005a,
	HAL_PARAM_VENC_ENABLE_INITIAL_QP			= 0x0400005b,
	HAL_PARAM_VENC_SEARCH_RANGE				= 0x0400005c,
	HAL_PARAM_VPE_COLOR_SPACE_CONVERSION			= 0x0400005d,
	HAL_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE		= 0x0400005e,
	HAL_PARAM_VENC_H264_NAL_SVC_EXT				= 0x0400005f,
	HAL_CONFIG_VENC_PERF_MODE				= 0x04000060,
	HAL_PARAM_VENC_HIER_B_MAX_ENH_LAYERS			= 0x04000061,
	HAL_PARAM_VDEC_NON_SECURE_OUTPUT2			= 0x04000062,
	HAL_PARAM_VENC_HIER_P_HYBRID_MODE			= 0x04000063,
};

enum hal_session_type {
	HAL_VIDEO_SESSION_TYPE_VPE,
	HAL_VIDEO_SESSION_TYPE_ENCODER,
	HAL_VIDEO_SESSION_TYPE_DECODER,
};

enum hal_multi_stream_type {
	HAL_VIDEO_DECODER_NONE		= 0x0,
	HAL_VIDEO_DECODER_PRIMARY	= 0x1,
	HAL_VIDEO_DECODER_SECONDARY	= 0x2,
	HAL_VIDEO_DECODER_BOTH_OUTPUTS	= 0x4,
};

enum hal_core_capabilities {
	HAL_VIDEO_ENCODER_ROTATION_CAPABILITY		= 0x1,
	HAL_VIDEO_ENCODER_SCALING_CAPABILITY		= 0x2,
	HAL_VIDEO_ENCODER_DEINTERLACE_CAPABILITY	= 0x4,
	HAL_VIDEO_DECODER_MULTI_STREAM_CAPABILITY	= 0x8,
};

enum hal_default_properties {
	HAL_VIDEO_DYNAMIC_BUF_MODE		= 0x1,
	HAL_VIDEO_CONTINUE_DATA_TRANSFER	= 0x2,
};

enum hal_video_codec {
	HAL_VIDEO_CODEC_UNKNOWN		= 0x0,
	HAL_VIDEO_CODEC_MVC		= 0x1,
	HAL_VIDEO_CODEC_H264		= 0x2,
	HAL_VIDEO_CODEC_H263		= 0x4,
	HAL_VIDEO_CODEC_MPEG1		= 0x8,
	HAL_VIDEO_CODEC_MPEG2		= 0x10,
	HAL_VIDEO_CODEC_MPEG4		= 0x20,
	HAL_VIDEO_CODEC_DIVX_311	= 0x40,
	HAL_VIDEO_CODEC_DIVX		= 0x80,
	HAL_VIDEO_CODEC_VC1		= 0x100,
	HAL_VIDEO_CODEC_SPARK		= 0x200,
	HAL_VIDEO_CODEC_VP6		= 0x400,
	HAL_VIDEO_CODEC_VP7		= 0x800,
	HAL_VIDEO_CODEC_VP8		= 0x1000,
	HAL_VIDEO_CODEC_HEVC		= 0x2000,
	HAL_VIDEO_CODEC_HEVC_HYBRID	= 0x4000,
	HAL_UNUSED_CODEC		= 0x10000000,
};

enum hal_h263_profile {
	HAL_H263_PROFILE_BASELINE		= 0x1,
	HAL_H263_PROFILE_H320CODING		= 0x2,
	HAL_H263_PROFILE_BACKWARDCOMPATIBLE	= 0x4,
	HAL_H263_PROFILE_ISWV2			= 0x8,
	HAL_H263_PROFILE_ISWV3			= 0x10,
	HAL_H263_PROFILE_HIGHCOMPRESSION	= 0x20,
	HAL_H263_PROFILE_INTERNET		= 0x40,
	HAL_H263_PROFILE_INTERLACE		= 0x80,
	HAL_H263_PROFILE_HIGHLATENCY		= 0x100,
};

enum hal_h263_level {
	HAL_H263_LEVEL_10	= 0x1,
	HAL_H263_LEVEL_20	= 0x2,
	HAL_H263_LEVEL_30	= 0x4,
	HAL_H263_LEVEL_40	= 0x8,
	HAL_H263_LEVEL_45	= 0x10,
	HAL_H263_LEVEL_50	= 0x20,
	HAL_H263_LEVEL_60	= 0x40,
	HAL_H263_LEVEL_70	= 0x80,
};

enum hal_mpeg2_profile {
	HAL_MPEG2_PROFILE_SIMPLE	= 0x1,
	HAL_MPEG2_PROFILE_MAIN		= 0x2,
	HAL_MPEG2_PROFILE_422		= 0x4,
	HAL_MPEG2_PROFILE_SNR		= 0x8,
	HAL_MPEG2_PROFILE_SPATIAL	= 0x10,
	HAL_MPEG2_PROFILE_HIGH		= 0x20,
};

enum hal_mpeg2_level {
	HAL_MPEG2_LEVEL_LL	= 0x1,
	HAL_MPEG2_LEVEL_ML	= 0x2,
	HAL_MPEG2_LEVEL_H14	= 0x4,
	HAL_MPEG2_LEVEL_HL	= 0x8,
};

enum hal_mpeg4_profile {
	HAL_MPEG4_PROFILE_SIMPLE		= 0x1,
	HAL_MPEG4_PROFILE_ADVANCEDSIMPLE	= 0x2,
	HAL_MPEG4_PROFILE_CORE			= 0x4,
	HAL_MPEG4_PROFILE_MAIN			= 0x8,
	HAL_MPEG4_PROFILE_NBIT			= 0x10,
	HAL_MPEG4_PROFILE_SCALABLETEXTURE	= 0x20,
	HAL_MPEG4_PROFILE_SIMPLEFACE		= 0x40,
	HAL_MPEG4_PROFILE_SIMPLEFBA		= 0x80,
	HAL_MPEG4_PROFILE_BASICANIMATED		= 0x100,
	HAL_MPEG4_PROFILE_HYBRID		= 0x200,
	HAL_MPEG4_PROFILE_ADVANCEDREALTIME	= 0x400,
	HAL_MPEG4_PROFILE_CORESCALABLE		= 0x800,
	HAL_MPEG4_PROFILE_ADVANCEDCODING	= 0x1000,
	HAL_MPEG4_PROFILE_ADVANCEDCORE		= 0x2000,
	HAL_MPEG4_PROFILE_ADVANCEDSCALABLE	= 0x4000,
	HAL_MPEG4_PROFILE_SIMPLESCALABLE	= 0x8000,
};

enum hal_mpeg4_level {
	HAL_MPEG4_LEVEL_0			= 0x1,
	HAL_MPEG4_LEVEL_0b			= 0x2,
	HAL_MPEG4_LEVEL_1			= 0x4,
	HAL_MPEG4_LEVEL_2			= 0x8,
	HAL_MPEG4_LEVEL_3			= 0x10,
	HAL_MPEG4_LEVEL_4			= 0x20,
	HAL_MPEG4_LEVEL_4a			= 0x40,
	HAL_MPEG4_LEVEL_5			= 0x80,
	HAL_MPEG4_LEVEL_VENDOR_START_UNUSED	= 0x7f000000,
	HAL_MPEG4_LEVEL_6			= 0x7f000001,
	HAL_MPEG4_LEVEL_7			= 0x7f000002,
	HAL_MPEG4_LEVEL_8			= 0x7f000003,
	HAL_MPEG4_LEVEL_9			= 0x7f000004,
	HAL_MPEG4_LEVEL_3b			= 0x7f000005,
};

enum hal_h264_profile {
	HAL_H264_PROFILE_BASELINE		= 0x1,
	HAL_H264_PROFILE_MAIN			= 0x2,
	HAL_H264_PROFILE_HIGH			= 0x4,
	HAL_H264_PROFILE_EXTENDED		= 0x8,
	HAL_H264_PROFILE_HIGH10			= 0x10,
	HAL_H264_PROFILE_HIGH422		= 0x20,
	HAL_H264_PROFILE_HIGH444		= 0x40,
	HAL_H264_PROFILE_CONSTRAINED_BASE	= 0x80,
	HAL_H264_PROFILE_CONSTRAINED_HIGH	= 0x100,
};

enum hal_h264_level {
	HAL_H264_LEVEL_1	= 0x1,
	HAL_H264_LEVEL_1b	= 0x2,
	HAL_H264_LEVEL_11	= 0x4,
	HAL_H264_LEVEL_12	= 0x8,
	HAL_H264_LEVEL_13	= 0x10,
	HAL_H264_LEVEL_2	= 0x20,
	HAL_H264_LEVEL_21	= 0x40,
	HAL_H264_LEVEL_22	= 0x80,
	HAL_H264_LEVEL_3	= 0x100,
	HAL_H264_LEVEL_31	= 0x200,
	HAL_H264_LEVEL_32	= 0x400,
	HAL_H264_LEVEL_4	= 0x800,
	HAL_H264_LEVEL_41	= 0x1000,
	HAL_H264_LEVEL_42	= 0x2000,
	HAL_H264_LEVEL_5	= 0x4000,
	HAL_H264_LEVEL_51	= 0x8000,
	HAL_H264_LEVEL_52	= 0x10000,
};

enum hal_hevc_profile {
	HAL_HEVC_PROFILE_MAIN		= 0x1,
	HAL_HEVC_PROFILE_MAIN10		= 0x2,
	HAL_HEVC_PROFILE_MAIN_STILL_PIC	= 0x4,
};

enum hal_hevc_level {
	HAL_HEVC_MAIN_TIER_LEVEL_1	= 0x10000001,
	HAL_HEVC_MAIN_TIER_LEVEL_2	= 0x10000002,
	HAL_HEVC_MAIN_TIER_LEVEL_2_1	= 0x10000004,
	HAL_HEVC_MAIN_TIER_LEVEL_3	= 0x10000008,
	HAL_HEVC_MAIN_TIER_LEVEL_3_1	= 0x10000010,
	HAL_HEVC_MAIN_TIER_LEVEL_4	= 0x10000020,
	HAL_HEVC_MAIN_TIER_LEVEL_4_1	= 0x10000040,
	HAL_HEVC_MAIN_TIER_LEVEL_5	= 0x10000080,
	HAL_HEVC_MAIN_TIER_LEVEL_5_1	= 0x10000100,
	HAL_HEVC_MAIN_TIER_LEVEL_5_2	= 0x10000200,
	HAL_HEVC_MAIN_TIER_LEVEL_6	= 0x10000400,
	HAL_HEVC_MAIN_TIER_LEVEL_6_1	= 0x10000800,
	HAL_HEVC_MAIN_TIER_LEVEL_6_2	= 0x10001000,
	HAL_HEVC_HIGH_TIER_LEVEL_1	= 0x20000001,
	HAL_HEVC_HIGH_TIER_LEVEL_2	= 0x20000002,
	HAL_HEVC_HIGH_TIER_LEVEL_2_1	= 0x20000004,
	HAL_HEVC_HIGH_TIER_LEVEL_3	= 0x20000008,
	HAL_HEVC_HIGH_TIER_LEVEL_3_1	= 0x20000010,
	HAL_HEVC_HIGH_TIER_LEVEL_4	= 0x20000020,
	HAL_HEVC_HIGH_TIER_LEVEL_4_1	= 0x20000040,
	HAL_HEVC_HIGH_TIER_LEVEL_5	= 0x20000080,
	HAL_HEVC_HIGH_TIER_LEVEL_5_1	= 0x20000100,
	HAL_HEVC_HIGH_TIER_LEVEL_5_2	= 0x20000200,
	HAL_HEVC_HIGH_TIER_LEVEL_6	= 0x20000400,
	HAL_HEVC_HIGH_TIER_LEVEL_6_1	= 0x20000800,
	HAL_HEVC_HIGH_TIER_LEVEL_6_2	= 0x20001000,
};

enum hal_hevc_tier {
	HAL_HEVC_TIER_MAIN	= 0x1,
	HAL_HEVC_TIER_HIGH	= 0x2,
};

enum hal_vpx_profile {
	HAL_VPX_PROFILE_SIMPLE		= 0x1,
	HAL_VPX_PROFILE_ADVANCED	= 0x2,
	HAL_VPX_PROFILE_VERSION_0	= 0x4,
	HAL_VPX_PROFILE_VERSION_1	= 0x8,
	HAL_VPX_PROFILE_VERSION_2	= 0x10,
	HAL_VPX_PROFILE_VERSION_3	= 0x20,
	HAL_VPX_PROFILE_UNUSED		= 0x10000000,
};

enum hal_vc1_profile {
	HAL_VC1_PROFILE_SIMPLE		= 0x1,
	HAL_VC1_PROFILE_MAIN		= 0x2,
	HAL_VC1_PROFILE_ADVANCED	= 0x4,
};

enum hal_vc1_level {
	HAL_VC1_LEVEL_LOW	= 0x1,
	HAL_VC1_LEVEL_MEDIUM	= 0x2,
	HAL_VC1_LEVEL_HIGH	= 0x4,
	HAL_VC1_LEVEL_0		= 0x8,
	HAL_VC1_LEVEL_1		= 0x10,
	HAL_VC1_LEVEL_2		= 0x20,
	HAL_VC1_LEVEL_3		= 0x40,
	HAL_VC1_LEVEL_4		= 0x80,
};

enum hal_divx_format {
	HAL_DIVX_FORMAT_4,
	HAL_DIVX_FORMAT_5,
	HAL_DIVX_FORMAT_6,
};

enum hal_divx_profile {
	HAL_DIVX_PROFILE_QMOBILE	= 0x1,
	HAL_DIVX_PROFILE_MOBILE		= 0x2,
	HAL_DIVX_PROFILE_MT		= 0x4,
	HAL_DIVX_PROFILE_HT		= 0x8,
	HAL_DIVX_PROFILE_HD		= 0x10,
};

enum hal_mvc_profile {
	HAL_MVC_PROFILE_STEREO_HIGH	= 0x1000,
};

enum hal_mvc_level {
	HAL_MVC_LEVEL_1		= 0x1,
	HAL_MVC_LEVEL_1b	= 0x2,
	HAL_MVC_LEVEL_11	= 0x4,
	HAL_MVC_LEVEL_12	= 0x8,
	HAL_MVC_LEVEL_13	= 0x10,
	HAL_MVC_LEVEL_2		= 0x20,
	HAL_MVC_LEVEL_21	= 0x40,
	HAL_MVC_LEVEL_22	= 0x80,
	HAL_MVC_LEVEL_3		= 0x100,
	HAL_MVC_LEVEL_31	= 0x200,
	HAL_MVC_LEVEL_32	= 0x400,
	HAL_MVC_LEVEL_4		= 0x800,
	HAL_MVC_LEVEL_41	= 0x1000,
	HAL_MVC_LEVEL_42	= 0x2000,
	HAL_MVC_LEVEL_5		= 0x4000,
	HAL_MVC_LEVEL_51	= 0x8000,
};

struct hal_framerate {
	enum hal_buffer_type buffer_type;
	u32 framerate;
};

enum hal_uncompressed_format {
	HAL_COLOR_FORMAT_MONOCHROME	= 0x1,
	HAL_COLOR_FORMAT_NV12		= 0x2,
	HAL_COLOR_FORMAT_NV21		= 0x4,
	HAL_COLOR_FORMAT_NV12_4x4TILE	= 0x8,
	HAL_COLOR_FORMAT_NV21_4x4TILE	= 0x10,
	HAL_COLOR_FORMAT_YUYV		= 0x20,
	HAL_COLOR_FORMAT_YVYU		= 0x40,
	HAL_COLOR_FORMAT_UYVY		= 0x80,
	HAL_COLOR_FORMAT_VYUY		= 0x100,
	HAL_COLOR_FORMAT_RGB565		= 0x200,
	HAL_COLOR_FORMAT_BGR565		= 0x400,
	HAL_COLOR_FORMAT_RGB888		= 0x800,
	HAL_COLOR_FORMAT_BGR888		= 0x1000,
	HAL_COLOR_FORMAT_NV12_UBWC	= 0x2000,
	HAL_COLOR_FORMAT_NV12_TP10_UBWC	= 0x4000,
};

enum hal_ssr_trigger_type {
	SSR_ERR_FATAL		= 1,
	SSR_SW_DIV_BY_ZERO,
	SSR_HW_WDOG_IRQ,
};

enum vidc_extradata_type {
	VIDC_EXTRADATA_NONE			= 0x0,
	VIDC_EXTRADATA_MB_QUANTIZATION		= 0x1,
	VIDC_EXTRADATA_INTERLACE_VIDEO		= 0x2,
	VIDC_EXTRADATA_VC1_FRAMEDISP		= 0x3,
	VIDC_EXTRADATA_VC1_SEQDISP		= 0x4,
	VIDC_EXTRADATA_TIMESTAMP		= 0x5,
	VIDC_EXTRADATA_S3D_FRAME_PACKING	= 0x6,
	VIDC_EXTRADATA_FRAME_RATE		= 0x7,
	VIDC_EXTRADATA_PANSCAN_WINDOW		= 0x8,
	VIDC_EXTRADATA_RECOVERY_POINT_SEI	= 0x9,
	VIDC_EXTRADATA_MPEG2_SEQDISP		= 0xd,
	VIDC_EXTRADATA_STREAM_USERDATA		= 0xe,
	VIDC_EXTRADATA_FRAME_QP			= 0xf,
	VIDC_EXTRADATA_FRAME_BITS_INFO		= 0x10,
	VIDC_EXTRADATA_INPUT_CROP		= 0x0700000e,
	VIDC_EXTRADATA_DIGITAL_ZOOM		= 0x07000010,
	VIDC_EXTRADATA_MULTISLICE_INFO		= 0x7f100000,
	VIDC_EXTRADATA_NUM_CONCEALED_MB		= 0x7f100001,
	VIDC_EXTRADATA_INDEX			= 0x7f100002,
	VIDC_EXTRADATA_ASPECT_RATIO		= 0x7f100003,
	VIDC_EXTRADATA_METADATA_LTR		= 0x7f100004,
	VIDC_EXTRADATA_METADATA_FILLER		= 0x7fe00002,
	VIDC_EXTRADATA_METADATA_MBI		= 0x7f100005,
};

struct hal_uncompressed_format_select {
	enum hal_buffer_type buffer_type;
	enum hal_uncompressed_format format;
};

struct hal_uncompressed_plane_actual {
	u32 actual_stride;
	u32 actual_plane_buffer_height;
};

struct hal_uncompressed_plane_actual_info {
	enum hal_buffer_type buffer_type;
	u32 num_planes;
	struct hal_uncompressed_plane_actual plane_format[1];
};

struct hal_uncompressed_plane_constraints {
	u32 stride_multiples;
	u32 max_stride;
	u32 min_plane_buffer_height_multiple;
	u32 buffer_alignment;
};

struct hal_uncompressed_plane_actual_constraints_info {
	enum hal_buffer_type buffer_type;
	u32 num_planes;
	struct hal_uncompressed_plane_constraints plane_format[1];
};

struct hal_extradata_header_config {
	u32 type;
	enum hal_buffer_type buffer_type;
	u32 version;
	u32 port_index;
	u32 client_extradata_id;
};

struct hal_framesize {
	enum hal_buffer_type buffer_type;
	u32 width;
	u32 height;
};

struct hal_enable {
	u32 enable;
};

struct hal_buffer_count_actual {
	enum hal_buffer_type type;
	u32 count_actual;
};

struct hal_buffer_size_actual {
	enum hal_buffer_type type;
	u32 size;
};

struct hal_buffer_display_hold_count_actual {
	enum hal_buffer_type buffer_type;
	u32 hold_count;
};

enum hal_nal_stream_format {
	HAL_NAL_FORMAT_STARTCODES		= 0x1,
	HAL_NAL_FORMAT_ONE_NAL_PER_BUFFER	= 0x2,
	HAL_NAL_FORMAT_ONE_BYTE_LENGTH		= 0x4,
	HAL_NAL_FORMAT_TWO_BYTE_LENGTH		= 0x8,
	HAL_NAL_FORMAT_FOUR_BYTE_LENGTH		= 0x10,
};

enum hal_output_order {
	HAL_OUTPUT_ORDER_DISPLAY,
	HAL_OUTPUT_ORDER_DECODE,
};

enum hal_picture {
	HAL_PICTURE_I		= 0x01,
	HAL_PICTURE_P		= 0x02,
	HAL_PICTURE_B		= 0x04,
	HAL_PICTURE_IDR		= 0x08,
	HAL_FRAME_NOTCODED	= 0x7f002000,
	HAL_FRAME_YUV		= 0x7f004000,
	HAL_UNUSED_PICT		= 0x10000000,
};

struct hal_extradata_enable {
	u32 enable;
	enum hal_extradata_id index;
};

struct hal_enable_picture {
	u32 picture_type;
};

struct hal_multi_stream {
	enum hal_buffer_type buffer_type;
	u32 enable;
	u32 width;
	u32 height;
};

struct hal_display_picture_buffer_count {
	u32 enable;
	u32 count;
};

struct hal_mb_error_map {
	u32 error_map_size;
	u8 error_map[1];
};

struct hal_request_iframe {
	u32 enable;
};

struct hal_bitrate {
	u32 bitrate;
	u32 layer_id;
};

struct hal_profile_level {
	u32 profile;
	u32 level;
};

struct hal_profile_level_supported {
	u32 profile_count;
	struct hal_profile_level profile_level[MAX_PROFILE_COUNT];
};

enum hal_h264_entropy {
	HAL_H264_ENTROPY_CAVLC	= 1,
	HAL_H264_ENTROPY_CABAC	= 2,
};

enum hal_h264_cabac_model {
	HAL_H264_CABAC_MODEL_0	= 1,
	HAL_H264_CABAC_MODEL_1	= 2,
	HAL_H264_CABAC_MODEL_2	= 4,
};

struct hal_h264_entropy_control {
	enum hal_h264_entropy entropy_mode;
	enum hal_h264_cabac_model cabac_model;
};

enum hal_rate_control {
	HAL_RATE_CONTROL_OFF,
	HAL_RATE_CONTROL_VBR_VFR,
	HAL_RATE_CONTROL_VBR_CFR,
	HAL_RATE_CONTROL_CBR_VFR,
	HAL_RATE_CONTROL_CBR_CFR,
};

struct hal_mpeg4_time_resolution {
	u32 time_increment_resolution;
};

struct hal_mpeg4_header_extension {
	u32 header_extension;
};

enum hal_h264_db_mode {
	HAL_H264_DB_MODE_DISABLE,
	HAL_H264_DB_MODE_SKIP_SLICE_BOUNDARY,
	HAL_H264_DB_MODE_ALL_BOUNDARY,
};

struct hal_h264_db_control {
	enum hal_h264_db_mode mode;
	int slice_alpha_offset;
	int slice_beta_offset;
};

struct hal_temporal_spatial_tradeoff {
	u32 ts_factor;
};

struct hal_quantization {
	u32 qpi;
	u32 qpp;
	u32 qpb;
	u32 layer_id;
};

struct hal_initial_quantization {
	u32 qpi;
	u32 qpp;
	u32 qpb;
	u32 init_qp_enable;
};

struct hal_quantization_range {
	u32 min_qp;
	u32 max_qp;
	u32 layer_id;
};

struct hal_intra_period {
	u32 pframes;
	u32 bframes;
};

struct hal_idr_period {
	u32 idr_period;
};

enum hal_rotate {
	HAL_ROTATE_NONE,
	HAL_ROTATE_90,
	HAL_ROTATE_180,
	HAL_ROTATE_270,
};

enum hal_flip {
	HAL_FLIP_NONE,
	HAL_FLIP_HORIZONTAL,
	HAL_FLIP_VERTICAL,
};

struct hal_operations {
	enum hal_rotate rotate;
	enum hal_flip flip;
};

enum hal_intra_refresh_mode {
	HAL_INTRA_REFRESH_NONE,
	HAL_INTRA_REFRESH_CYCLIC,
	HAL_INTRA_REFRESH_ADAPTIVE,
	HAL_INTRA_REFRESH_CYCLIC_ADAPTIVE,
	HAL_INTRA_REFRESH_RANDOM,
};

struct hal_intra_refresh {
	enum hal_intra_refresh_mode mode;
	u32 air_mbs;
	u32 air_ref;
	u32 cir_mbs;
};

enum hal_multi_slice {
	HAL_MULTI_SLICE_OFF,
	HAL_MULTI_SLICE_BY_MB_COUNT,
	HAL_MULTI_SLICE_BY_BYTE_COUNT,
	HAL_MULTI_SLICE_GOB,
};

struct hal_multi_slice_control {
	enum hal_multi_slice multi_slice;
	u32 slice_size;
};

struct hal_debug_config {
	u32 debug_config;
};

struct hal_buffer_requirements {
	enum hal_buffer_type type;
	u32 size;
	u32 region_size;
	u32 hold_count;
	u32 count_min;
	u32 count_actual;
	u32 contiguous;
	u32 alignment;
};

/* Priority increases with number */
enum hal_priority {
	HAL_PRIORITY_LOW	= 10,
	HAL_PRIOIRTY_MEDIUM	= 20,
	HAL_PRIORITY_HIGH	= 30,
};

struct hal_batch_info {
	u32 input_batch_count;
	u32 output_batch_count;
};

struct hal_metadata_pass_through {
	u32 enable;
	u32 size;
};

struct hal_uncompressed_format_supported {
	enum hal_buffer_type buffer_type;
	u32 format_entries;
	u32 format_info[1];
};

enum hal_interlace_format {
	HAL_INTERLACE_FRAME_PROGRESSIVE			= 0x01,
	HAL_INTERLACE_INTERLEAVE_FRAME_TOPFIELDFIRST	= 0x02,
	HAL_INTERLACE_INTERLEAVE_FRAME_BOTTOMFIELDFIRST	= 0x04,
	HAL_INTERLACE_FRAME_TOPFIELDFIRST		= 0x08,
	HAL_INTERLACE_FRAME_BOTTOMFIELDFIRST		= 0x10,
};

struct hal_interlace_format_supported {
	enum hal_buffer_type buffer_type;
	enum hal_interlace_format format;
};

enum hal_chroma_site {
	HAL_CHROMA_SITE_0,
	HAL_CHROMA_SITE_1,
};

struct hal_properties_supported {
	u32 num_properties;
	u32 properties[1];
};

enum hal_capability_type {
	HAL_CAPABILITY_FRAME_WIDTH,
	HAL_CAPABILITY_FRAME_HEIGHT,
	HAL_CAPABILITY_MBS_PER_FRAME,
	HAL_CAPABILITY_MBS_PER_SECOND,
	HAL_CAPABILITY_FRAMERATE,
	HAL_CAPABILITY_SCALE_X,
	HAL_CAPABILITY_SCALE_Y,
	HAL_CAPABILITY_BITRATE,
	HAL_CAPABILITY_SECURE_OUTPUT2_THRESHOLD,
};

struct hal_capability  {
	enum hal_capability_type type;
	u32 min;
	u32 max;
	u32 step_size;
};

struct hal_capabilities {
	u32 num_capabilities;
	struct hal_capability data[1];
};

struct hal_nal_stream_format_supported {
	u32 format;
};

struct hal_nal_stream_format_select {
	u32 format;
};

struct hal_multi_view_format {
	u32 views;
	u32 view_order[1];
};

enum hal_buffer_layout_type {
	HAL_BUFFER_LAYOUT_TOP_BOTTOM,
	HAL_BUFFER_LAYOUT_SEQ,
};

struct hal_mvc_buffer_layout {
	enum hal_buffer_layout_type layout_type;
	u32 bright_view_first;
	u32 ngap;
};

struct hal_seq_header_info {
	u32 nax_header_len;
};

struct hal_codec_supported {
	u32 decoder_codec_supported;
	u32 encoder_codec_supported;
};

struct hal_multi_view_select {
	u32 view_index;
};

struct hal_timestamp_scale {
	u32 time_stamp_scale;
};


struct hal_h264_vui_timing_info {
	u32 enable;
	u32 fixed_framerate;
	u32 time_scale;
};

struct hal_h264_vui_bitstream_restrc {
	u32 enable;
};

struct hal_preserve_text_quality {
	u32 enable;
};

struct hal_vc1e_perf_cfg_type {
	struct {
		u32 x_subsampled;
		u32 y_subsampled;
	} i_frame, p_frame, b_frame;
};

struct hal_vpe_color_space_conversion {
	u32 csc_matrix[HAL_MAX_MATRIX_COEFFS];
	u32 csc_bias[HAL_MAX_BIAS_COEFFS];
	u32 csc_limit[HAL_MAX_LIMIT_COEFFS];
};

enum hal_resource_id {
	VIDC_RESOURCE_NONE,
	VIDC_RESOURCE_OCMEM,
	VIDC_RESOURCE_VMEM,
};

struct hal_resource_hdr {
	enum hal_resource_id resource_id;
	void *resource_handle;
	u32 size;
};

struct hal_buffer_addr_info {
	enum hal_buffer_type buffer_type;
	u32 buffer_size;
	u32 num_buffers;
	u32 device_addr;
	u32 extradata_addr;
	u32 extradata_size;
	u32 response_required;
};

/* Needs to be exactly the same as hfi_buffer_info */
struct hal_buffer_info {
	u32 buffer_addr;
	u32 extradata_addr;
};

struct hal_frame_plane_config {
	u32 left;
	u32 top;
	u32 width;
	u32 height;
	u32 stride;
	u32 scan_lines;
};

struct hal_uncompressed_frame_config {
	struct hal_frame_plane_config luma_plane;
	struct hal_frame_plane_config chroma_plane;
};

struct hal_frame_data {
	enum hal_buffer_type buffer_type;
	u32 device_addr;
	u32 extradata_addr;
	int64_t timestamp;
	u32 flags;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 mark_target;
	u32 mark_data;
	u32 clnt_data;
	u32 extradata_size;
};

struct hal_seq_hdr {
	u32 seq_hdr;
	u32 seq_hdr_len;
};

enum hal_flush {
	HAL_FLUSH_INPUT,
	HAL_FLUSH_OUTPUT,
	HAL_FLUSH_OUTPUT2,
	HAL_FLUSH_ALL,
};

enum hal_event_type {
	HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES,
	HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES,
	HAL_EVENT_RELEASE_BUFFER_REFERENCE,
};

enum hal_buffer_mode_type {
	HAL_BUFFER_MODE_STATIC	= 0x1,
	HAL_BUFFER_MODE_RING	= 0x10,
	HAL_BUFFER_MODE_DYNAMIC	= 0x100,
};

struct hal_buffer_alloc_mode {
	enum hal_buffer_type type;
	enum hal_buffer_mode_type mode;
};

enum ltr_mode {
	HAL_LTR_MODE_DISABLE,
	HAL_LTR_MODE_MANUAL,
	HAL_LTR_MODE_PERIODIC,
};

struct hal_ltr_mode {
	enum ltr_mode mode;
	u32 count;
	u32 trust_mode;
};

struct hal_ltr_use {
	u32 ref_ltr;
	u32 use_constraint;
	u32 frames;
};

struct hal_ltr_mark {
	u32 mark_frame;
};

struct hal_venc_perf_mode {
	u32 mode;
};

struct hal_hybrid_hierp {
	u32 layers;
};

union hal_get_property {
	struct hal_framerate framerate;
	struct hal_uncompressed_format_select format_select;
	struct hal_uncompressed_plane_actual plane_actual;
	struct hal_uncompressed_plane_actual_info plane_actual_info;
	struct hal_uncompressed_plane_constraints plane_constraints;
	struct hal_uncompressed_plane_actual_constraints_info
						plane_constraints_info;
	struct hal_extradata_header_config extradata_header_config;
	struct hal_framesize framesize;
	struct hal_enable enable;
	struct hal_buffer_count_actual buffer_count_actual;
	struct hal_extradata_enable extradata_enable;
	struct hal_enable_picture enable_picture;
	struct hal_multi_stream multi_stream;
	struct hal_display_picture_buffer_count display_picture_buffer_count;
	struct hal_mb_error_map mb_error_map;
	struct hal_request_iframe request_iframe;
	struct hal_bitrate bitrate;
	struct hal_profile_level profile_level;
	struct hal_profile_level_supported profile_level_supported;
	struct hal_h264_entropy_control h264_entropy_control;
	struct hal_mpeg4_time_resolution mpeg4_time_resolution;
	struct hal_mpeg4_header_extension mpeg4_header_extension;
	struct hal_h264_db_control h264_db_control;
	struct hal_temporal_spatial_tradeoff temporal_spatial_tradeoff;
	struct hal_quantization quantization;
	struct hal_quantization_range quantization_range;
	struct hal_intra_period intra_period;
	struct hal_idr_period idr_period;
	struct hal_operations operations;
	struct hal_intra_refresh intra_refresh;
	struct hal_multi_slice_control multi_slice_control;
	struct hal_debug_config debug_config;
	struct hal_batch_info batch_info;
	struct hal_metadata_pass_through metadata_pass_through;
	struct hal_uncompressed_format_supported uncompressed_format;
	struct hal_interlace_format_supported interlace_format;
	struct hal_properties_supported properties;
	struct hal_capability capability;
	struct hal_capabilities capabilities;
	struct hal_nal_stream_format_supported nal_stream_format;
	struct hal_nal_stream_format_select nal_stream_format_select;
	struct hal_multi_view_format multi_view_format;
	struct hal_seq_header_info seq_header_info;
	struct hal_codec_supported codec_supported;
	struct hal_multi_view_select multi_view_select;
	struct hal_timestamp_scale timestamp_scale;
	struct hal_h264_vui_timing_info h264_vui_timing_info;
	struct hal_h264_vui_bitstream_restrc h264_vui_bitstream_restrc;
	struct hal_preserve_text_quality preserve_text_quality;
	struct hal_buffer_info buffer_info;
	struct hal_buffer_alloc_mode buffer_alloc_mode;
	struct hal_buffer_requirements bufreq[HAL_BUFFER_MAX];
};

/* HAL Response */

/* Events */
#define EVT_SYS_EVENT_CHANGE		1
#define EVT_SYS_WATCHDOG_TIMEOUT	2
#define EVT_SYS_ERROR			3
#define EVT_SESSION_ERROR		4

/* Event callback structure */

struct hal_cb_event {
	enum hal_error error;
	u32 height;
	u32 width;
	enum hal_event_type event_type;
	u32 packet_buffer;
	u32 extradata_buffer;
	u32 profile;
	u32 level;
};

/* Data callback structure */

struct hal_fbd {
	u32 stream_id;
	u32 view_id;
	u32 timestamp_hi;
	u32 timestamp_lo;
	u32 flags1;
	u32 mark_target;
	u32 mark_data;
	u32 stats;
	u32 alloc_len1;
	u32 filled_len1;
	u32 offset1;
	u32 frame_width;
	u32 frame_height;
	u32 start_x_coord;
	u32 start_y_coord;
	u32 input_tag;
	u32 input_tag1;
	enum hal_picture picture_type;
	u32 packet_buffer1;
	u32 extradata_buffer;
	u32 flags2;
	u32 alloc_len2;
	u32 filled_len2;
	u32 offset2;
	u32 packet_buffer2;
	u32 flags3;
	u32 alloc_len3;
	u32 filled_len3;
	u32 offset3;
	u32 packet_buffer3;
	enum hal_buffer_type buffer_type;
};

struct hal_sys_init_done {
	u32 enc_codecs;
	u32 dec_codecs;
};

struct hal_session_init_done {
	struct hal_capability width;
	struct hal_capability height;
	struct hal_capability mbs_per_frame;
	struct hal_capability mbs_per_sec;
	struct hal_capability frame_rate;
	struct hal_capability scale_x;
	struct hal_capability scale_y;
	struct hal_capability bitrate;
	struct hal_capability hier_p;
	struct hal_capability ltr_count;
	struct hal_capability secure_output2_threshold;
	struct hal_uncompressed_format_supported uncomp_format;
	struct hal_interlace_format_supported interlace_format;
	struct hal_nal_stream_format_supported nal_stream_format;
	struct hal_profile_level_supported profile_level;
	struct hal_intra_refresh intra_refresh;
	struct hal_seq_header_info seq_hdr_info;
	enum hal_buffer_mode_type alloc_mode_out;
};

enum session_type {
	VIDC_ENCODER = 0,
	VIDC_DECODER,
};

/* define core states */
#define CORE_UNINIT	0
#define CORE_INIT	1
#define CORE_INVALID	2

/* define instance states */
#define INST_INVALID			1
#define INST_UNINIT			2
#define INST_INIT			3
#define INST_LOAD_RESOURCES		4
#define INST_START			5
#define INST_STOP			6
#define INST_RELEASE_RESOURCES		7

enum vidc_hal_type {
	VIDC_VENUS,
	VIDC_Q6,
};

enum hfi_packetization_type {
	HFI_PACKETIZATION_LEGACY,
	HFI_PACKETIZATION_3XX,
};

#define call_hfi_op(hfi, op, args...)	\
	(((hfi) && (hfi)->ops && (hfi)->ops->op) ?	\
	((hfi)->ops->op(args)) : 0)

struct hfi_device;

struct hfi_core_ops {
	int (*event_notify)(struct hfi_device *hfi, u32 event);
};

struct hfi_device_inst;

struct hfi_inst_ops {
	int (*empty_buf_done)(struct hfi_device_inst *inst, u32 addr,
			      u32 bytesused, u32 data_offset, u32 flags);
	int (*fill_buf_done)(struct hfi_device_inst *inst, u32 addr,
			     u32 bytesused, u32 data_offset, u32 flags,
			     struct timeval *ts);
	int (*event_notify)(struct hfi_device_inst *inst, u32 event,
			    struct hal_cb_event *data);
};

struct hfi_device_inst {
	struct list_head list;
	void *device;
	struct mutex lock;
	unsigned int state;
	struct completion done;
	unsigned int error;

	/* instance operations passed by outside world */
	const struct hfi_inst_ops *ops;
	void *ops_priv;

	enum hal_session_type session_type;
	enum hal_video_codec codec;
	union hal_get_property hprop;

	/* filled by session_init */
	struct hal_session_init_done caps;

	/* buffer requirements */
	struct hal_buffer_requirements bufreq[HAL_BUFFER_MAX];

	/* debug */
	enum hal_property last_ptype;
};

struct hfi_device {
	struct device *dev;	/* mostly used for dev_xxx */
	enum vidc_hal_type hfi_type;

	struct mutex lock;
	unsigned int state;
	struct completion done;
	unsigned int error;

	/*
	 * list of 'struct hfi_device_inst' instances which belong to
	 * this hfi core device
	 */
	struct list_head instances;

	/* core operations passed by outside world */
	const struct hfi_core_ops *core_ops;

	/* filled by sys core init */
	u32 enc_codecs;
	u32 dec_codecs;

	/* core capabilities */
	enum hal_core_capabilities core_caps;

	/* internal hfi operations */
	void *priv;
	const struct hfi_ops *ops;
	const struct hfi_packetization_ops *pkt_ops;
	enum hfi_packetization_type packetization_type;
};

struct hfi_ops {
	int (*core_init)(struct hfi_device *hfi);
	int (*core_deinit)(struct hfi_device *hfi);
	int (*core_ping)(struct hfi_device *hfi, u32 cookie);
	int (*core_trigger_ssr)(struct hfi_device *hfi,
				enum hal_ssr_trigger_type);

	int(*session_init)(struct hfi_device *hfi,
			   struct hfi_device_inst *inst,
			   enum hal_session_type session_type,
			   enum hal_video_codec codec);
	int (*session_end)(struct hfi_device_inst *inst);
	int (*session_abort)(struct hfi_device_inst *inst);
	int (*session_flush)(struct hfi_device_inst *inst,
			     enum hal_flush flush_mode);
	int (*session_start)(struct hfi_device_inst *inst);
	int (*session_stop)(struct hfi_device_inst *inst);
	int (*session_etb)(struct hfi_device_inst *inst,
			   struct hal_frame_data *input_frame);
	int (*session_ftb)(struct hfi_device_inst *inst,
			   struct hal_frame_data *output_frame);
	int (*session_set_buffers)(struct hfi_device_inst *inst,
				   struct hal_buffer_addr_info *bai);
	int (*session_release_buffers)(struct hfi_device_inst *inst,
				       struct hal_buffer_addr_info *bai);
	int (*session_load_res)(struct hfi_device_inst *inst);
	int (*session_release_res)(struct hfi_device_inst *inst);
	int (*session_parse_seq_hdr)(struct hfi_device_inst *inst,
				     struct hal_seq_hdr *seq_hdr);
	int (*session_get_seq_hdr)(struct hfi_device_inst *inst,
				   struct hal_seq_hdr *seq_hdr);
	int (*session_set_property)(struct hfi_device_inst *inst,
				    enum hal_property ptype, void *pdata);
	int (*session_get_property)(struct hfi_device_inst *inst,
				    enum hal_property ptype);

	int (*resume)(struct hfi_device *hfi);
	int (*suspend)(struct hfi_device *hfi);

	/* interrupt operations */
	irqreturn_t (*isr)(int irq, struct hfi_device *hfi);
	irqreturn_t (*isr_thread)(int irq, struct hfi_device *hfi);
};

static inline void *to_hfi_priv(struct hfi_device *hfi)
{
	return hfi->priv;
}

int vidc_hfi_create(struct hfi_device *hfi, struct vidc_resources *res);
void vidc_hfi_destroy(struct hfi_device *hfi);

int vidc_hfi_core_init(struct hfi_device *hfi);
int vidc_hfi_core_deinit(struct hfi_device *hfi);
int vidc_hfi_core_suspend(struct hfi_device *hfi);
int vidc_hfi_core_resume(struct hfi_device *hfi);
int vidc_hfi_core_trigger_ssr(struct hfi_device *hfi,
			      enum hal_ssr_trigger_type type);
int vidc_hfi_core_ping(struct hfi_device *hfi);

struct hfi_device_inst *vidc_hfi_session_create(struct hfi_device *hfi,
						const struct hfi_inst_ops *ops,
						void *ops_priv);
void vidc_hfi_session_destroy(struct hfi_device *hfi,
			      struct hfi_device_inst *inst);
int vidc_hfi_session_init(struct hfi_device *hfi, struct hfi_device_inst *inst,
			  u32 pixfmt, enum session_type type);
int vidc_hfi_session_deinit(struct hfi_device *hfi,
			    struct hfi_device_inst *inst);
int vidc_hfi_session_start(struct hfi_device *hfi,
			   struct hfi_device_inst *inst);
int vidc_hfi_session_stop(struct hfi_device *hfi, struct hfi_device_inst *inst);
int vidc_hfi_session_abort(struct hfi_device *hfi,
			   struct hfi_device_inst *inst);
int vidc_hfi_session_load_res(struct hfi_device *hfi,
			      struct hfi_device_inst *inst);
int vidc_hfi_session_unload_res(struct hfi_device *hfi,
				struct hfi_device_inst *inst);
int vidc_hfi_session_flush(struct hfi_device *hfi,
			   struct hfi_device_inst *inst);
int vidc_hfi_session_set_buffers(struct hfi_device *hfi,
				 struct hfi_device_inst *inst,
				 struct hal_buffer_addr_info *bai);
int vidc_hfi_session_unset_buffers(struct hfi_device *hfi,
				   struct hfi_device_inst *inst,
				   struct hal_buffer_addr_info *bai);
int vidc_hfi_session_get_property(struct hfi_device *hfi,
				  struct hfi_device_inst *inst,
				  enum hal_property ptype,
				  union hal_get_property *hprop);
int vidc_hfi_session_set_property(struct hfi_device *hfi,
				  struct hfi_device_inst *inst,
				  enum hal_property ptype, void *pdata);
int vidc_hfi_session_etb(struct hfi_device *hfi, struct hfi_device_inst *inst,
			 struct hal_frame_data *fdata);
int vidc_hfi_session_ftb(struct hfi_device *hfi, struct hfi_device_inst *inst,
			 struct hal_frame_data *fdata);
irqreturn_t vidc_hfi_isr_thread(int irq, void *dev_id);
irqreturn_t vidc_hfi_isr(int irq, void *dev);

#endif
