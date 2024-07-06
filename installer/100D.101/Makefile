MODEL = 100D
FW_VERSION = 101

include ../Makefile.installer

# Load ML at user_mem_start (aka heap start / DRY_HEAP_START / malloc memory pool)
# The malloc memory pool will be pushed to the right, so it will start after our BSS.
# user_mem_start: FF0C1C7C: 000C5388
RESTARTSTART = 0xc53a0
ML_BOOT_OBJ = boot-d45.o
