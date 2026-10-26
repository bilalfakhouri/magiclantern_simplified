/**
 * Camera internals for EOS R8 1.4.0
 */

/** This camera has a DIGIC X chip */
#define CONFIG_DIGIC_X

/** has inter-core RPC (so far this has always been dependent on SGI, 0xc) */
#define CONFIG_RPC

/** Cam has MMU (by itself, does nothing, see CONFIG_MMU_REMAP) */
#define CONFIG_MMU

/** Digic 6 and up does not have bitmap font in ROM, try to load it from card **/
#define CONFIG_NO_BFNT

/** has LV: enables the PROP_LV_ACTION handler that sets `lv` (without it lv is always 0) **/
#define CONFIG_LIVEVIEW

/** disable SRM for now
 * in current state SRM_AllocateMemoryResourceFor1stJob makes camera crash
 * even if just one buffer is requested.
 */
#define CONFIG_MEMORY_SRM_NOT_WORKING

#define CONFIG_MALLOC_STRUCT_V2

#define CONFIG_TASK_STRUCT_V2_SMP
#define CONFIG_TASK_ATTR_STRUCT_V5
