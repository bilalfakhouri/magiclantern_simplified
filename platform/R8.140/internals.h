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

/** R8 acutally has dedicated movie mode, this is temporal until we fix is_movie_mode() on R8, needed for mlv_lite */
#define CONFIG_NO_DEDICATED_MOVIE_MODE

/** state object hooks: LiveView vsync from SensorDriverState (wait_lv_frames etc.),
 *  see platform/R8.140/include/platform/state-object.h */
#define CONFIG_STATE_OBJECT_HOOKS

/** disable SRM for now
 * in current state SRM_AllocateMemoryResourceFor1stJob makes camera crash
 * even if just one buffer is requested.
 */
#define CONFIG_MEMORY_SRM_NOT_WORKING

#define CONFIG_MALLOC_STRUCT_V2

#define CONFIG_TASK_STRUCT_V2_SMP
#define CONFIG_TASK_ATTR_STRUCT_V5

/** RscMgr pools described by an IBus base (0xb, big): GetMemoryAddressOfMemoryChunk
 *  is implemented in exmem.c on top of GetIBusAddressOfMemoryChunk + src/ibus.h */
#define CONFIG_RSCMGR_IBUS

/** shoot_malloc_suite(0) joins the RscMgr pools listed in RSCMGR_POOLS (consts.h) */
#define CONFIG_RSCMGR_POOLS

/** EDMAC mem2mem via Engine::MemoryToMemoryEsub1, see src/edmac-memcpy-dx.c.
 *  (CONFIG_EDMAC_MEMCPY would build the DIGIC 4/5 src/edmac-memcpy.c instead) */
#define CONFIG_EDMAC_MEMCPY_DX
