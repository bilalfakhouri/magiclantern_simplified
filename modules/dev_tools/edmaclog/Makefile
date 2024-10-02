# define the module name - make sure name is max 8 characters
MODULE_NAME = edmaclog
MODULE_OBJS = $(BUILD_DIR)/edmaclog.o \
              $(BUILD_DIR)/hooks_thumb.o
TOP_DIR = ../../..

# include modules environment
include $(TOP_DIR)/modules/Makefile
CFLAGS_ARCHLESS = $(filter-out -march=armv7-r -march=armv7-a -march=armv5te,$(CFLAGS))
.DEFAULT_GOAL := $(BUILD_DIR)/module_complete

# We want the core module to run on any cam.
# We need Thumb instructions for hooks on D678X cams.
# We build the code that will only run on modern cams in this file.
# Do not call anything in this file from D45.
$(BUILD_DIR)/hooks_thumb.o: hooks_thumb.c
	$(CC) $(CFLAGS_ARCHLESS) -mthumb -march=armv7 $(CDEPS_FLAGS) -c -o $@ $<

