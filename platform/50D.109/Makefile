MODEL=50D
FW_VERSION=109

TOP_DIR = ../..

AUTOEXEC_BASE = 0x800000
FIR_BASE = 0x800120
ML_BOOT_OBJ = boot-d45.o

FIRMWARE_ID	= 0x80000261

ifeq ($(FW_VERSION),109)
CANON_NAME_FIR = 50D00109.FIR

MAIN_FIRMWARE_ADDR = 0xFF810000
PLATFORM_ARCH = armv5te

# DryOSmemory map
# RESTARTSTART is selected to be just above the end of the bss
#
RESTARTSTART = 0x4b000
endif
ML_AUDIO_OBJ = n

ML_PICSTYLE_AUTO = n

ML_SRC_EXTRA_OBJS = $(BUILD_DIR)/misc.o $(BUILD_DIR)/afma.o
#	video_hacks.o
#	reloc.o \
#	af_patterns.o

include ../Makefile
.DEFAULT_GOAL := $(BUILD_DIR)/cam_complete
