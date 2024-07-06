MODEL=500D
FW_VERSION=111

TOP_DIR = ../..
PLATFORM_DIR = .

AUTOEXEC_BASE = 0x800000
FIR_BASE = 0x800120

ML_BOOT_OBJ = boot-d45.o

ifeq ($(FW_VERSION),111)
CANON_NAME_FIR = E7KR6111.FIR

MAIN_FIRMWARE_ADDR = 0xFF010000
PLATFORM_ARCH = armv5te

# DryOSmemory map
# RESTARTSTART is selected to be just above the end of the bss
#
RESTARTSTART = 0x4D000

#End of FW_VERSION == 111
endif
#Makefile for 500D

ML_PICSTYLE_AUTO = n

include ../Makefile
.DEFAULT_GOAL := $(BUILD_DIR)/cam_complete
