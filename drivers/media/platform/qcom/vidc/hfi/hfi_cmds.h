/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#ifndef __HFI_CMDS_H__
#define __HFI_CMDS_H__

#include "hfi_helper.h"
#include "hfi.h"

/* commands */
#define HFI_CMD_SYS_COMMON_START	(HFI_DOMAIN_BASE_COMMON +	\
					 HFI_ARCH_COMMON_OFFSET +	\
					 HFI_CMD_START_OFFSET + 0x0000)
#define HFI_CMD_SYS_INIT		(HFI_CMD_SYS_COMMON_START + 0x1)
#define HFI_CMD_SYS_PC_PREP		(HFI_CMD_SYS_COMMON_START + 0x2)
#define HFI_CMD_SYS_SET_RESOURCE	(HFI_CMD_SYS_COMMON_START + 0x3)
#define HFI_CMD_SYS_RELEASE_RESOURCE	(HFI_CMD_SYS_COMMON_START + 0x4)
#define HFI_CMD_SYS_SET_PROPERTY	(HFI_CMD_SYS_COMMON_START + 0x5)
#define HFI_CMD_SYS_GET_PROPERTY	(HFI_CMD_SYS_COMMON_START + 0x6)
#define HFI_CMD_SYS_SESSION_INIT	(HFI_CMD_SYS_COMMON_START + 0x7)
#define HFI_CMD_SYS_SESSION_END		(HFI_CMD_SYS_COMMON_START + 0x8)
#define HFI_CMD_SYS_SET_BUFFERS		(HFI_CMD_SYS_COMMON_START + 0x9)
#define HFI_CMD_SYS_TEST_START		(HFI_CMD_SYS_COMMON_START + 0x100)
#define HFI_CMD_SYS_TEST_SSR		(HFI_CMD_SYS_TEST_START + 0x1)

#define HFI_CMD_SESSION_COMMON_START	(HFI_DOMAIN_BASE_COMMON +	\
					 HFI_ARCH_COMMON_OFFSET +	\
					 HFI_CMD_START_OFFSET + 0x1000)
#define HFI_CMD_SESSION_SET_PROPERTY	(HFI_CMD_SESSION_COMMON_START + 0x1)
#define HFI_CMD_SESSION_SET_BUFFERS	(HFI_CMD_SESSION_COMMON_START + 0x2)
#define HFI_CMD_SESSION_GET_SEQUENCE_HEADER	\
					(HFI_CMD_SESSION_COMMON_START + 0x3)

#define HFI_CMD_SYS_OX_START		(HFI_DOMAIN_BASE_COMMON +	\
					 HFI_ARCH_OX_OFFSET +		\
					 HFI_CMD_START_OFFSET + 0x0000)
#define HFI_CMD_SYS_SESSION_ABORT	(HFI_CMD_SYS_OX_START + 0x1)
#define HFI_CMD_SYS_PING		(HFI_CMD_SYS_OX_START + 0x2)

#define HFI_CMD_SESSION_OX_START	(HFI_DOMAIN_BASE_COMMON +	\
					 HFI_ARCH_OX_OFFSET +		\
					 HFI_CMD_START_OFFSET + 0x1000)
#define HFI_CMD_SESSION_LOAD_RESOURCES	(HFI_CMD_SESSION_OX_START + 0x1)
#define HFI_CMD_SESSION_START		(HFI_CMD_SESSION_OX_START + 0x2)
#define HFI_CMD_SESSION_STOP		(HFI_CMD_SESSION_OX_START + 0x3)
#define HFI_CMD_SESSION_EMPTY_BUFFER	(HFI_CMD_SESSION_OX_START + 0x4)
#define HFI_CMD_SESSION_FILL_BUFFER	(HFI_CMD_SESSION_OX_START + 0x5)
#define HFI_CMD_SESSION_SUSPEND		(HFI_CMD_SESSION_OX_START + 0x6)
#define HFI_CMD_SESSION_RESUME		(HFI_CMD_SESSION_OX_START + 0x7)
#define HFI_CMD_SESSION_FLUSH		(HFI_CMD_SESSION_OX_START + 0x8)
#define HFI_CMD_SESSION_GET_PROPERTY	(HFI_CMD_SESSION_OX_START + 0x9)
#define HFI_CMD_SESSION_PARSE_SEQUENCE_HEADER	\
					(HFI_CMD_SESSION_OX_START + 0xa)
#define HFI_CMD_SESSION_RELEASE_BUFFERS		\
					(HFI_CMD_SESSION_OX_START + 0xb)
#define HFI_CMD_SESSION_RELEASE_RESOURCES	\
					(HFI_CMD_SESSION_OX_START + 0xc)

/* command packets */
struct hfi_sys_init_pkt {
	struct hfi_pkt_hdr hdr;
	u32 arch_type;
};

struct hfi_sys_pc_prep_pkt {
	struct hfi_pkt_hdr hdr;
};

struct hfi_sys_set_resource_pkt {
	struct hfi_pkt_hdr hdr;
	u32 resource_handle;
	u32 resource_type;
	u32 resource_data[1];
};

struct hfi_sys_release_resource_pkt {
	struct hfi_pkt_hdr hdr;
	u32 resource_type;
	u32 resource_handle;
};

struct hfi_sys_set_property_pkt {
	struct hfi_pkt_hdr hdr;
	u32 num_properties;
	u32 data[1];
};

struct hfi_sys_get_property_pkt {
	struct hfi_pkt_hdr hdr;
	u32 num_properties;
	u32 data[1];
};

struct hfi_sys_set_buffers_pkt {
	struct hfi_pkt_hdr hdr;
	u32 buffer_type;
	u32 buffer_size;
	u32 num_buffers;
	u32 buffer_addr[1];
};

struct hfi_sys_ping_pkt {
	struct hfi_pkt_hdr hdr;
	u32 client_data;
};

struct hfi_session_init_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 session_domain;
	u32 session_codec;
};

struct hfi_session_end_pkt {
	struct hfi_session_hdr_pkt shdr;
};

struct hfi_session_abort_pkt {
	struct hfi_session_hdr_pkt shdr;
};

struct hfi_session_set_property_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 num_properties;
	u32 data[0];
};

struct hfi_session_set_buffers_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 buffer_type;
	u32 buffer_size;
	u32 extradata_size;
	u32 min_buffer_size;
	u32 num_buffers;
	u32 buffer_info[1];
};

struct hfi_session_get_sequence_header_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 buffer_len;
	u32 packet_buffer;
};

struct hfi_session_load_resources_pkt {
	struct hfi_session_hdr_pkt shdr;
};

struct hfi_session_start_pkt {
	struct hfi_session_hdr_pkt shdr;
};

struct hfi_session_stop_pkt {
	struct hfi_session_hdr_pkt shdr;
};

struct hfi_session_empty_buffer_compressed_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 input_tag;
	u32 packet_buffer;
	u32 extradata_buffer;
	u32 data[1];
};

struct hfi_session_empty_buffer_uncompressed_plane0_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 view_id;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 input_tag;
	u32 packet_buffer;
	u32 extradata_buffer;
	u32 data[1];
};

struct hfi_session_empty_buffer_uncompressed_plane1_pkt {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 packet_buffer2;
	u32 data[1];
};

struct hfi_session_empty_buffer_uncompressed_plane2_pkt {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 packet_buffer3;
	u32 data[1];
};

struct hfi_session_fill_buffer_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 stream_id;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 output_tag;
	u32 packet_buffer;
	u32 extradata_buffer;
	u32 data[1];
};

struct hfi_session_flush_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 flush_type;
};

struct hfi_session_suspend_pkt {
	struct hfi_session_hdr_pkt shdr;
};

struct hfi_session_resume_pkt {
	struct hfi_session_hdr_pkt shdr;
};

struct hfi_session_get_property_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 num_properties;
	u32 data[1];
};

struct hfi_session_release_buffer_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 buffer_type;
	u32 buffer_size;
	u32 extradata_size;
	u32 response_req;
	u32 num_buffers;
	u32 buffer_info[1];
};

struct hfi_session_release_resources_pkt {
	struct hfi_session_hdr_pkt shdr;
};

struct hfi_session_parse_sequence_header_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 header_len;
	u32 packet_buffer;
};

struct hfi_sfr {
	u32 buf_size;
	u8 data[1];
};

struct hfi_sys_test_ssr_pkt {
	struct hfi_pkt_hdr hdr;
	u32 trigger_type;
};

#define call_hfi_pkt_op(hfi, op, args...)			\
	(((hfi) && (hfi)->pkt_ops && (hfi)->pkt_ops->op) ?	\
	((hfi)->pkt_ops->op(args)) : -EINVAL)

struct hfi_packetization_ops {
	void (*sys_init)(struct hfi_sys_init_pkt *pkt, u32 arch_type);
	void (*sys_pc_prep)(struct hfi_sys_pc_prep_pkt *pkt);
	void (*sys_idle_indicator)(struct hfi_sys_set_property_pkt *pkt,
				   u32 enable);
	void (*sys_power_control)(struct hfi_sys_set_property_pkt *pkt,
				  u32 enable);
	int (*sys_set_resource)(struct hfi_sys_set_resource_pkt *pkt,
				struct hal_resource_hdr *resource_hdr,
				void *resource_value);
	int (*sys_release_resource)(struct hfi_sys_release_resource_pkt *pkt,
				    struct hal_resource_hdr *resource_hdr);
	void (*sys_debug_config)(struct hfi_sys_set_property_pkt *pkt, u32 mode,
				 u32 config);
	void (*sys_coverage_config)(struct hfi_sys_set_property_pkt *pkt,
				    u32 mode);
	void (*sys_ping)(struct hfi_sys_ping_pkt *pkt, u32 cookie);
	void (*sys_image_version)(struct hfi_sys_get_property_pkt *pkt);
	void (*ssr_cmd)(enum hal_ssr_trigger_type type,
		        struct hfi_sys_test_ssr_pkt *pkt);
	int (*session_init)(struct hfi_session_init_pkt *pkt,
			    struct hfi_device_inst *inst,
			    u32 session_domain, u32 session_codec);
	void (*session_cmd)(struct hfi_session_pkt *pkt, u32 pkt_type,
			    struct hfi_device_inst *inst);
	int (*session_set_buffers)(struct hfi_session_set_buffers_pkt *pkt,
				   struct hfi_device_inst *inst,
				   struct hal_buffer_addr_info *bai);
	int (*session_release_buffers)(
		struct hfi_session_release_buffer_pkt *pkt,
		struct hfi_device_inst *inst,
		struct hal_buffer_addr_info *bai);
	int (*session_etb_decoder)(
		struct hfi_session_empty_buffer_compressed_pkt *pkt,
		struct hfi_device_inst *inst,
		struct hal_frame_data *input_frame);
	int (*session_etb_encoder)(
		struct hfi_session_empty_buffer_uncompressed_plane0_pkt *pkt,
		struct hfi_device_inst *inst,
		struct hal_frame_data *input_frame);
	int (*session_ftb)(struct hfi_session_fill_buffer_pkt *pkt,
		struct hfi_device_inst *inst,
		struct hal_frame_data *output_frame);
	int (*session_parse_seq_header)(
		struct hfi_session_parse_sequence_header_pkt *pkt,
		struct hfi_device_inst *inst, struct hal_seq_hdr *seq_hdr);
	int (*session_get_seq_hdr)(
		struct hfi_session_get_sequence_header_pkt *pkt,
		struct hfi_device_inst *inst, struct hal_seq_hdr *seq_hdr);
	int (*session_flush)(struct hfi_session_flush_pkt *pkt,
		struct hfi_device_inst *inst, enum hal_flush flush_mode);
	int (*session_get_property)(
		struct hfi_session_get_property_pkt *pkt,
		struct hfi_device_inst *inst, enum hal_property ptype);
	int (*session_set_property)(struct hfi_session_set_property_pkt *pkt,
				    struct hfi_device_inst *inst,
				    enum hal_property ptype, void *pdata);
};

const struct hfi_packetization_ops *
hfi_get_pkt_ops(enum hfi_packetization_type);

#endif
