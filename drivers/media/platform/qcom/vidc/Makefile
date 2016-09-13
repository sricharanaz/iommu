# Makefile for Qualcomm vidc driver

ccflags-y += -Idrivers/media/platform/qcom/vidc
vidc-objs += \
		core.o \
		int_bufs.o \
		helpers.o \
		vdec.o \
		vdec_ctrls.o \
		venc.o \
		venc_ctrls.o \
		mem.o \
		resources.o \
		load.o \
		hfi/venus/venus_hfi.o \
		hfi/hfi_msgs.o \
		hfi/hfi_cmds.o \
		hfi/hfi.o \
		vmem.o \
		vmem_debugfs.o \

obj-$(CONFIG_QCOM_VIDC_V4L2) += vidc.o
