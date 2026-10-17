#define FEATURE_VRAM_RGBA
#define CONFIG_HELLO_WORLD

#define CONFIG_COMPOSITOR_XCM

// Don't Click Me menu looks to be intended as a place
// for devs to put custom code in debug.c run_test(),
// and allowing triggering from a menu context.
#undef FEATURE_DONT_CLICK_ME

#undef FEATURE_SHOW_SHUTTER_COUNT

// working but incomplete, some allocators don't report
// anything yet as they're faked / not yet found
#undef FEATURE_SHOW_FREE_MEMORY

#undef CONFIG_TSKMON
#undef FEATURE_SHOW_TASKS
#undef FEATURE_SHOW_CPU_USAGE
#undef FEATURE_SHOW_GUI_EVENTS

//#undef CONFIG_CRASH_LOG
#undef CONFIG_PROP_REQUEST_CHANGE
#undef CONFIG_AUTOBACKUP_ROM
