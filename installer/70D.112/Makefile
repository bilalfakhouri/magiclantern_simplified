MODEL=70D
include ../Makefile.installer

# TODO the following are lines cut from platform/70D/Makefile,
# the intention being to remove ML_SRC_PROFILE and not include
# that file (currently, indirectly) from this one.

# Load ML at user_mem_start (aka heap start / DRY_HEAP_START / malloc memory pool)
# The malloc memory pool will be pushed to the right, so it will start after our BSS.
# user_mem_start: FF0C1C64: 0xFAE30
RESTARTSTART = 0xFAF00
ML_BOOT_OBJ = boot-d45.o
