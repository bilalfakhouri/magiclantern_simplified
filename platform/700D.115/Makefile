MODEL = 700D
FW_VERSION = 115

TOP_DIR = ../..

ML_BOOT_OBJ = boot-d45.o

AUTOEXEC_BASE   = 0x40800000
FIR_BASE        = 0x40800120

ifeq ($(FW_VERSION),115)
CANON_NAME_FIR = CCF14115.FIR
PLATFORM_ARCH = armv5te

# DryOSmemory map
# RESTARTSTART is selected to be just above the end of the bss
#
MAIN_FIRMWARE_ADDR = 0xFF0C0000
RESTARTSTART    = 0x0007EC00

endif

include ../Makefile
.DEFAULT_GOAL := $(BUILD_DIR)/cam_complete
