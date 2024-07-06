MODEL = 6D
FW_VERSION = 116

include ../Makefile.installer

# Load ML at user_mem_start (aka heap start / DRY_HEAP_START / malloc memory pool)
# The malloc memory pool will be pushed to the right, so it will start after our BSS.
# user_mem_start: ff0c1c64: 000d7868
RESTARTSTART = 0xD7900
ML_BOOT_OBJ = boot-d45.o
