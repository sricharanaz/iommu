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
#ifndef __HFI_MSGS_H__
#define __HFI_MSGS_H__

#include "hfi_helper.h"

/* message calls */
#define HFI_MSG_SYS_COMMON_START	(HFI_DOMAIN_BASE_COMMON +	\
					 HFI_ARCH_COMMON_OFFSET +	\
					 HFI_MSG_START_OFFSET + 0x0000)
#define HFI_MSG_SYS_INIT		(HFI_MSG_SYS_COMMON_START + 0x1)
#define HFI_MSG_SYS_PC_PREP		(HFI_MSG_SYS_COMMON_START + 0x2)
#define HFI_MSG_SYS_RELEASE_RESOURCE	(HFI_MSG_SYS_COMMON_START + 0x3)
#define HFI_MSG_SYS_DEBUG		(HFI_MSG_SYS_COMMON_START + 0x4)
#define HFI_MSG_SYS_SESSION_INIT	(HFI_MSG_SYS_COMMON_START + 0x6)
#define HFI_MSG_SYS_SESSION_END		(HFI_MSG_SYS_COMMON_START + 0x7)
#define HFI_MSG_SYS_IDLE		(HFI_MSG_SYS_COMMON_START + 0x8)
#define HFI_MSG_SYS_COV                 (HFI_MSG_SYS_COMMON_START + 0x9)
#define HFI_MSG_SYS_PROPERTY_INFO	(HFI_MSG_SYS_COMMON_START + 0xa)

#define HFI_MSG_SESSION_COMMON_START	(HFI_DOMAIN_BASE_COMMON +	\
					 HFI_ARCH_COMMON_OFFSET +	\
					 HFI_MSG_START_OFFSET + 0x1000)
#define HFI_MSG_EVENT_NOTIFY		(HFI_MSG_SESSION_COMMON_START + 0x1)
#define HFI_MSG_SESSION_GET_SEQUENCE_HEADER \
					(HFI_MSG_SESSION_COMMON_START + 0x2)

#define HFI_MSG_SYS_OX_START		(HFI_DOMAIN_BASE_COMMON +	\
					 HFI_ARCH_OX_OFFSET +		\
					 HFI_MSG_START_OFFSET + 0x0000)
#define HFI_MSG_SYS_PING_ACK		(HFI_MSG_SYS_OX_START + 0x2)
#define HFI_MSG_SYS_SESSION_ABORT	(HFI_MSG_SYS_OX_START + 0x4)

#define HFI_MSG_SESSION_OX_START	(HFI_DOMAIN_BASE_COMMON +	\
					 HFI_ARCH_OX_OFFSET +	\
					 HFI_MSG_START_OFFSET + 0x1000)
#define HFI_MSG_SESSION_LOAD_RESOURCES	(HFI_MSG_SESSION_OX_START + 0x1)
#define HFI_MSG_SESSION_START		(HFI_MSG_SESSION_OX_START + 0x2)
#define HFI_MSG_SESSION_STOP		(HFI_MSG_SESSION_OX_START + 0x3)
#define HFI_MSG_SESSION_SUSPEND		(HFI_MSG_SESSION_OX_START + 0x4)
#define HFI_MSG_SESSION_RESUME		(HFI_MSG_SESSION_OX_START + 0x5)
#define HFI_MSG_SESSION_FLUSH		(HFI_MSG_SESSION_OX_START + 0x6)
#define HFI_MSG_SESSION_EMPTY_BUFFER	(HFI_MSG_SESSION_OX_START + 0x7)
#define HFI_MSG_SESSION_FILL_BUFFER	(HFI_MSG_SESSION_OX_START + 0x8)
#define HFI_MSG_SESSION_PROPERTY_INFO		(HFI_MSG_SESSION_OX_START + 0x9)
#define HFI_MSG_SESSION_RELEASE_RESOURCES	(HFI_MSG_SESSION_OX_START + 0xa)
#define HFI_MSG_SESSION_PARSE_SEQUENCE_HEADER	(HFI_MSG_SESSION_OX_START + 0xb)
#define HFI_MSG_SESSION_RELEASE_BUFFERS		(HFI_MSG_SESSION_OX_START + 0xc)

/* message packets */
struct hfi_msg_event_notify_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 event_id;
	u32 event_data1;
	u32 event_data2;
	u32 ext_event_data[1];
};

struct hfi_msg_event_release_buffer_ref_pkt {
	u32 packet_buffer;
	u32 extradata_buffer;
	u32 output_tag;
};

struct hfi_msg_sys_init_done_pkt {
	struct hfi_pkt_hdr hdr;
	u32 error_type;
	u32 num_properties;
	u32 data[1];
};

struct hfi_msg_sys_pc_prep_done_pkt {
	struct hfi_pkt_hdr hdr;
	u32 error_type;
};

struct hfi_msg_sys_release_resource_done_pkt {
	struct hfi_pkt_hdr hdr;
	u32 resource_handle;
	u32 error_type;
};

struct hfi_msg_session_init_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
	u32 num_properties;
	u32 data[1];
};

struct hfi_msg_session_end_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
};

struct hfi_msg_session_get_sequence_hdr_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
	u32 header_len;
	u32 sequence_header;
};

struct hfi_msg_sys_session_abort_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
};

struct hfi_msg_sys_idle_pkt {
	struct hfi_pkt_hdr hdr;
};

struct hfi_msg_sys_ping_ack_pkt {
	struct hfi_pkt_hdr hdr;
	u32 client_data;
};

struct hfi_msg_sys_property_info_pkt {
	struct hfi_pkt_hdr hdr;
	u32 num_properties;
	u32 data[1];
};

struct hfi_msg_session_load_resources_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
};

struct hfi_msg_session_start_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
};

struct hfi_msg_session_stop_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
};

struct hfi_msg_session_suspend_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
};

struct hfi_msg_session_resume_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
};

struct hfi_msg_session_flush_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
	u32 flush_type;
};

struct hfi_msg_session_empty_buffer_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
	u32 offset;
	u32 filled_len;
	u32 input_tag;
	u32 packet_buffer;
	u32 extradata_buffer;
	u32 data[0];
};

struct hfi_msg_session_fbd_compressed_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u32 error_type;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 stats;
	u32 offset;
	u32 alloc_len;
	u32 filled_len;
	u32 input_tag;
	u32 output_tag;
	u32 picture_type;
	u32 packet_buffer;
	u32 extradata_buffer;
	u32 data[0];
};

struct hfi_msg_session_fbd_uncompressed_plane0_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 stream_id;
	u32 view_id;
	u32 error_type;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u32 flags;
	u32 mark_target;
	u32 mark_data;
	u32 stats;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 frame_width;
	u32 frame_height;
	u32 start_x_coord;
	u32 start_y_coord;
	u32 input_tag;
	u32 input_tag2;
	u32 output_tag;
	u32 picture_type;
	u32 packet_buffer;
	u32 extradata_buffer;
	u32 data[0];
};

struct hfi_msg_session_fbd_uncompressed_plane1_pkt {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 packet_buffer2;
	u32 data[0];
};

struct hfi_msg_session_fbd_uncompressed_plane2_pkt {
	u32 flags;
	u32 alloc_len;
	u32 filled_len;
	u32 offset;
	u32 packet_buffer3;
	u32 data[0];
};

struct hfi_msg_session_parse_sequence_header_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
	u32 num_properties;
	u32 data[1];
};

struct hfi_msg_session_property_info_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 num_properties;
	u32 data[1];
};

struct hfi_msg_session_release_resources_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
};

struct hfi_msg_session_release_buffers_done_pkt {
	struct hfi_session_hdr_pkt shdr;
	u32 error_type;
	u32 num_buffers;
	u32 buffer_info[1];
};

struct hfi_msg_sys_debug_pkt {
	struct hfi_pkt_hdr hdr;
	u32 msg_type;
	u32 msg_size;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u8 msg_data[1];
};

struct hfi_msg_sys_coverage_pkt {
	struct hfi_pkt_hdr hdr;
	u32 msg_size;
	u32 time_stamp_hi;
	u32 time_stamp_lo;
	u8 msg_data[1];
};

struct hfi_device;
struct hfi_pkt_hdr;

void hfi_process_watchdog_timeout(struct hfi_device *hfi);
u32 hfi_process_msg_packet(struct hfi_device *hfi, struct hfi_pkt_hdr *hdr);

#endif
