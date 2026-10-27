/** \file
 * VRAM Compositor interface
 */

/**
 * There's a hardware accelerated compositor running on Digic 6+ cameras.
 * It has couple of variants (pure Ximr, XCM) - see further comments.
 *
 * For more detailed explanation on how this was discovered, look at forums:
 * https://www.magiclantern.fm/forum/index.php?topic=26024
 */

/**
 * Terminology:
 * Zico core    - Xtensa core, running DryOS, used for graphics acceeleration,
 *                GUI rendering and compositing. Connected with GPU (Takumi
 *                GV550 IP core)
 * mzrm         - Marius - Zico remote messaging (?). RPC used to talk between
 *                main ARM core (named Marius on D6) and Zico.
 * Ximr         - Render MIXer (?). Low level compositor on D6 and up.
 * Ximr Context - Data structure representing Ximr configuration, sent via mzrm
 *                using XimrExe() to Zico for render.
 * Input layer  - Layer (described by MARV struct) to be included in mix.
 * XOC          - Ximr Output Chunk. Buffer where final image is rendered.
 * XCM          - Ximr Context Maker. "Userspace" tool to deal with Ximr setup.
 *                Exists on D8 and up, from EOS R (M50 uses pure Ximr)
 */

#include "dryos.h"
#include "bmp.h"
#include "compositor.h"

#ifdef CONFIG_COMPOSITOR_DEDICATED_LAYER
/**
 * refreshVrmsSurface (renamed to VMIX_TransferRectangleToVram somewhere around 850D)
 * is used by Canon code to render Input layers into XOC.
 *
 * On pure XIMR cameras it uses XIMR functions to setup Ximr Context
 */
extern void      refreshVrmsSurface();
extern uint32_t  display_refresh_needed;

/**
 * Stores our layer ID. Defaults to Canon GUI layer, so in case
 * alocation failed - everything falls back to non-compositing behaviour.
 */
int _rgb_vram_layer_id = CANON_GUI_LAYER_ID;
struct MARV * pNewLayer;

/**
 * Not sure if sync_caches() call is needed. It was when I was drawing
 * over Canon buffers, but now when we have our own may be unnecessary.
 */
void _compositor_force_redraw()
{
    display_refresh_needed = 1;
    ml_refresh_display_needed = 1;
    sync_caches();
    //refreshVrmsSurface(); // not needed as we do XimrExe() in bmp.c
}

// Align to 0x100 and use uncacheable region.
// This is expected by some Zico functions (Canon's own layer heap uses 0x100 alignment too).
#define LAYER_BUF_ALIGN(p) UNCACHEABLE((uint8_t*)((((uintptr_t)(p) + 0x100) >> 8) << 8))
#define LAYER_BUF_SIZE     ((BMP_VRAM_SIZE * 4) + 0x100)

#ifdef CONFIG_DIGIC_X
#ifndef CONFIG_COMPOSITOR_XCM
#error "DIGIC X dedicated layer needs CONFIG_COMPOSITOR_XCM (uses XCM_GetSourceSurface)"
#endif
/**
 * DIGIC X: IBus / DRAM channel handling
 *
 * DIGIC X has (at least) two DRAM channels. Every bus master addresses memory
 * via a 64-bit "IBus" address, ARM addresses are translated by a table of
 * MEMIF windows, one per 256MB of ARM address space (regs 0xd2044080..ac,
 * read by MemifWindow_GetIBusAddress()). On R8 the boot code (0xe000a880) sets:
 *
 *   0x0xxxxxxx, 0x1xxxxxxx (+ uncached 0x4/0x5) -> DRAM channel 1
 *   0x2xxxxxxx, 0x3xxxxxxx (+ uncached 0x6/0x7) -> DRAM channel 2
 *
 * MemifWindow_GetIBusAddress() returns:
 *   lo32 = (window & ~7) | (addr & 0x0FFFFFFF)
 *   hi32 = window & 7 = DRAM channel
 *
 * Ximr checks MARV->ibus_addr in XimrSetInputLayerMARV() (R8: 0xe01a8c9e):
 *   - channel must be 1..3 ("Invalid IBUS Address!!" assert)
 *   - Ximr master (MemiArbiter pid 0x12) must be registered for that channel
 *     ("pid(%d) is not registered in DRAM-Ch%d" assert, MemiArbiter.c)
 * So the bitmap buffer can't come from "just any" allocator - this is the
 * "buffers need to be on specific IBus" problem.
 *
 * Strategy: allocate, ask MemifWindow which channel we landed on and accept
 * the buffer only if Ximr can read it. A channel is accepted if:
 *   - it is the same channel as Canon's own GUI layer (Ximr already reads
 *     that one on every refresh, so it's proven), or
 *   - MemiArbiter's pid table says Ximr is registered there (if the platform
 *     provides MEMIARB_PID_TABLE).
 * Otherwise fall back to RscMgr allocations from the types listed in
 * COMPOSITOR_RSC_TYPES (platform consts.h), which land on other channels.
 */
extern uint64_t MemifWindow_GetIBusAddress(void *buf);

#ifndef XIMR_MEMIARB_PID
#define XIMR_MEMIARB_PID 0x12   // pid passed by Ximr to MemiArbiter check
#endif

static uint32_t compositor_buf_channel(void *buf)
{
    // MemifWindow_GetIBusAddress asserts on >= 0xC0000000; RAM is far below that
    return (uint32_t)(MemifWindow_GetIBusAddress(CACHEABLE(buf)) >> 32);
}

static int compositor_channel_ok(uint32_t ch, uint32_t canon_ch)
{
    if (ch < 1 || ch > 3)
        return 0;

    if (ch == canon_ch)
        return 1;

#ifdef MEMIARB_PID_TABLE
    /*
     * Same lookup as MemiArbiter check (R8: 0xe09225bc):
     * 253 bytes per channel, starting at channel 1. 1 = pid registered.
     * First byte 0xff = table not populated yet (Canon then asks HW).
     */
    uint8_t *tbl = (uint8_t *)MEMIARB_PID_TABLE;
    if (tbl[0] != 0xff)
        return tbl[(ch - 1) * 0xfd + XIMR_MEMIARB_PID] == 1;
#endif

    // If we don't know where Canon's layer is (canon_ch == 0) and can't ask
    // MemiArbiter, take any valid channel - same as previous behaviour.
    return canon_ch == 0;
}

/*
 * Minimal synchronous wrapper around RscMgr contiguous allocation, with
 * a selectable type ID. exmem.c uses a fixed TYPE_ID, we need to pick
 * types that land on a specific DRAM channel.
 */
#ifdef COMPOSITOR_RSC_TYPES
typedef struct
{
    struct memSuite *ret;
    struct semaphore *sem;
    volatile uint32_t timed_out;
} compositor_rsc_msg_t;

static void compositor_rsc_free_cbr(unsigned int ctx) { }

static void compositor_rsc_alloc_cbr(unsigned int ctx, struct memSuite *hSuite)
{
    compositor_rsc_msg_t *msg = (compositor_rsc_msg_t *)ctx;
    if (msg->timed_out)
    {
        // caller gave up, don't leak
        if (hSuite) FreeMemoryResource(hSuite, compositor_rsc_free_cbr, 0);
        return;
    }
    msg->ret = hSuite;
    give_semaphore(msg->sem);
}

static compositor_rsc_msg_t compositor_rsc_msg; // must outlive a timed out request, see cbr

/* returns suite, NULL on failure; *timed_out set if RscMgr didn't answer */
static struct memSuite *compositor_rsc_alloc_contig(size_t size, int type, int *timed_out)
{
    compositor_rsc_msg_t *msg = &compositor_rsc_msg;
    msg->ret = NULL;
    msg->timed_out = 0;
    if (!msg->sem) msg->sem = create_named_semaphore("compositor_rsc", 0);

    AllocateContinuousMemoryResource(size, compositor_rsc_alloc_cbr, (unsigned int)msg, type);
    if (take_semaphore(msg->sem, 1000))
    {
        // late callback will free the suite; msg can't be reused safely
        msg->timed_out = 1;
        *timed_out = 1;
        return NULL;
    }
    *timed_out = 0;
    return msg->ret;
}
#endif // COMPOSITOR_RSC_TYPES

/*
 * Allocate bitmap buffer that Ximr can read.
 * Returns aligned, uncacheable pointer or NULL.
 */
static uint8_t *_compositor_alloc_bitmap(uint32_t canon_ch)
{
    // 1. ML heap. On R8 this is DRAM channel 1.
    void *raw = malloc(LAYER_BUF_SIZE);
    if (raw)
    {
        uint8_t *buf = LAYER_BUF_ALIGN(raw);
        uint32_t ch = compositor_buf_channel(buf);
        DryosDebugMsg(0, 15, "malloc layer buf 0x%08x -> DRAM-Ch%d (Canon: Ch%d)", buf, ch, canon_ch);
        if (compositor_channel_ok(ch, canon_ch))
            return buf;
        free(raw);
    }

#ifdef COMPOSITOR_RSC_TYPES
    // 2. RscMgr pools. Kept forever (as is ML's layer).
    static const int rsc_types[] = COMPOSITOR_RSC_TYPES;
    for (int i = 0; i < COUNT(rsc_types); i++)
    {
        int timed_out = 0;
        struct memSuite *suite = compositor_rsc_alloc_contig(LAYER_BUF_SIZE, rsc_types[i], &timed_out);
        if (timed_out)
            break;
        if (!suite)
            continue;

        struct memChunk *chunk = GetFirstChunkFromSuite(suite);
        void *addr = chunk ? GetMemoryAddressOfMemoryChunk(chunk) : NULL;
        if (addr)
        {
            uint8_t *buf = LAYER_BUF_ALIGN(addr);
            uint32_t ch = compositor_buf_channel(buf);
            DryosDebugMsg(0, 15, "RscMgr type 0x%x layer buf 0x%08x -> DRAM-Ch%d", rsc_types[i], buf, ch);
            if (compositor_channel_ok(ch, canon_ch))
                return buf;
        }
        FreeMemoryResource(suite, compositor_rsc_free_cbr, 0);
    }
#endif

    return NULL;
}
#else // !CONFIG_DIGIC_X
static uint8_t *_compositor_alloc_bitmap()
{
    uint8_t *raw = malloc(LAYER_BUF_SIZE);
    return raw ? LAYER_BUF_ALIGN(raw) : NULL;
}
#endif // CONFIG_DIGIC_X

struct MARV *_compositor_create_layer(uint32_t bmp_w, uint32_t bmp_h)
{
    // buffer for MARV structure
    struct MARV* pNewLayer = malloc(sizeof(struct MARV));
    if(pNewLayer == NULL) return NULL;

#ifdef CONFIG_DIGIC_X
    // Find DRAM channel of Canon GUI layer; Ximr is known to accept it.
    uint32_t canon_ch = 0;
    struct MARV *canon = XCM_GetSourceSurface(_pXCM, CANON_GUI_LAYER_ID);
    if (canon && canon->signature == 0x5652414D)
        canon_ch = (uint32_t)(canon->ibus_addr >> 32);

    uint8_t* pBitmapData = _compositor_alloc_bitmap(canon_ch);
#else
    uint8_t* pBitmapData = _compositor_alloc_bitmap();
#endif
    if(pBitmapData == NULL) {
        free(pNewLayer); // cleanup
        return NULL;
    }

    // fill MARV data
    pNewLayer->signature    = 0x5652414D;  // MARV
    pNewLayer->bitmap_data  = pBitmapData;
    pNewLayer->opacity_data = 0x0;
    pNewLayer->flags        = XIMR_FLAGS_LAYER_RGBA; // see compositor.h
#ifdef CONFIG_DIGIC_X
    // Mirrors Canon CreateMARV (R8: 0xe0a042e8)
    pNewLayer->pad_0c       = 0;
    pNewLayer->ibus_addr    = MemifWindow_GetIBusAddress(CACHEABLE(pBitmapData));
    pNewLayer->req_width    = (uint32_t)bmp_w;
    pNewLayer->unk_2c       = 0;
    // Canon rounds width (= pitch) up to 32 px; our pitch is fixed, so it must already be aligned.
    #if ((BMP_W_PLUS - BMP_W_MINUS) % 32) != 0
    #error "DIGIC X MARV width must be multiple of 32"
    #endif
#endif
    pNewLayer->width        = (uint32_t)bmp_w;
    pNewLayer->height       = (uint32_t)bmp_h;
    // Probably pmem is not needed. No issues observed so far.
    pNewLayer->pmem         = 0x0;

    DryosDebugMsg(0, 15, "MARV 0x%08x, bitmap_data 0x%08x", pNewLayer, pBitmapData);

    return pNewLayer;
}

#ifdef CONFIG_COMPOSITOR_XCM
/**
 * Implementation specific to models with Ximr Context Maker (XCM)
 */

#if !defined(CONFIG_COMPOSITOR_XCM_V1) && !defined(CONFIG_COMPOSITOR_XCM_V2)
#error "CONFIG_COMPOSITOR_XCM enabled, but no XCM type was defined!"
#endif

/*
 * DIGIC X (verified on R8 1.4.0) uses the same XCM_V2 API as DIGIC 8.
 * Differences are handled in _compositor_create_layer():
 *  - MARV is 0x30 bytes, has 64-bit IBus address of bitmap data
 *  - bitmap buffer must sit on a DRAM channel Ximr is registered on
 */

#ifdef CONFIG_COMPOSITOR_XCM_V1
/*
 * Structures specific to EOS R / PowerShot SX740
 * XCM_SetSourceSurface(), XCM_SetSourceArea(), XOC_SetLayerEnable() calls are
 * done on each redraw (in refreshVrmsSurface() ) based on structures below.
 *
 * VMIX_Layers        List of *MARV for all existing layers
 * VMIX_LayersEnable  Controls if layer with given ID is displayed or not.
 */
extern struct MARV *VMIX_Layers[XIMR_MAX_LAYERS];
#ifdef CONFIG_R
extern uint32_t VMIX_LayersEnable[XIMR_MAX_LAYERS]; // R only, DNE on SX740
#endif // CONFIG_R
#endif // CONFIG_COMPOSITOR_XCM_V1

#ifdef CONFIG_COMPOSITOR_XCM_V2
/*
 * Pure XCM API is used
 */
extern uint32_t XCM_SetSourceSurface(void * pXCM, uint layer, struct MARV * pMARV);
extern uint32_t XCM_SetSourceArea(void * pXCM, uint layer, uint16_t srcX, uint16_t srcY, uint16_t srcW, uint16_t srcH);
extern uint32_t XOC_SetLayerEnable(int p1, int p2, uint layer, int p4);
#endif // CONFIG_COMPOSITOR_XCM_V2

/*
 * CONFIG_COMPOSITOR_XCM_V2 models use a single array of structures, and won't
 * override settings of the layers that we've created.
 * Left for reference only:
 *
 * struct RENDERER_LayerMedatata
 * {
 *   struct MARV* pMARV;  // pointer to layer MARV
 *    uint32_t w;   // layer width
 *    uint32_t h;   // layer height
 * };
 * extern struct RENDERER_LayerMedatata RENDERER_LayersMetadata[XIMR_MAX_LAYERS];
 *
 * ======
 *
 * On models since 850D(?), XCM_SetColorMatrixType() was added to XCM API.
 * extern uint32_t XCM_SetColorMatrixType(void * pXCM, uint layer, uint matrixType);
 * As tested on 850D: matrixType 0, 1, 2
 * 0: RGB->BT709FULL
 * 1: RGB->BT709VIDEO
 * 2: RGB->BT709FULL (?!) WINSYS_getMixColorMatrixIndex supports only 0-1
 *
 * Since XCM defaults to 0 on a new layer, we ommit this call.
 */

/*
 * Create a new VRAM (MARV) structure, alloc buffer for VRAM.
 * Call compositor to enable newly created layer.
 */
int compositor_layer_setup()
{
    // So far it seems that default GUI layer is always 0
    int newLayerID = 0;

    // Find first available, non used layer slot.
    // XCM itself has only XCM_MAX_LAYERS slots - for index >= 4 it prints
    // "XCM_GetSourceSurface layerIndex" error and returns NULL, which would
    // look like a free slot.
    while( newLayerID < XCM_MAX_LAYERS
      && XCM_GetSourceSurface(_pXCM, newLayerID) != NULL)
    {
        newLayerID++;
    }

    DryosDebugMsg(0, 15, "Found %d layers", newLayerID);
    if(newLayerID >= XCM_MAX_LAYERS)
    {
        DryosDebugMsg(0, 15, "Too many layers: %d/%d, aborting!",
                newLayerID, XCM_MAX_LAYERS);
        return 1;
    }

    // compute our layer size
    uint32_t bmp_w = BMP_W_PLUS - BMP_W_MINUS;
    uint32_t bmp_h = BMP_H_PLUS - BMP_H_MINUS;

    // create layer
    pNewLayer = _compositor_create_layer(bmp_w, bmp_h);
    if(pNewLayer == NULL){
        DryosDebugMsg(0, 15, "Failed to create a new layer. Falling back to _rgb_vram_info");
        return 1;
    }
    /*
     * The code below seems to be R specific. RP/R6 seems to use single struct
     * for that.
     */

#ifdef CONFIG_COMPOSITOR_XCM_V1
    VMIX_Layers[newLayerID] = pNewLayer;
#ifdef CONFIG_R
    VMIX_LayersEnable[newLayerID] = 1;
#endif // CONFIG_R
#endif // CONFIG_COMPOSITOR_XCM_V1

#ifdef CONFIG_COMPOSITOR_XCM_V2
    XCM_SetSourceSurface(_pXCM, newLayerID, pNewLayer);
    XCM_SetSourceArea(_pXCM, newLayerID, -BMP_W_MINUS, -BMP_H_MINUS, BMP_W_PLUS + BMP_W_MINUS, BMP_H_PLUS + BMP_H_MINUS);
    XOC_SetLayerEnable(0, 0, newLayerID, 1); // seems layer is enabled by default
#endif // CONFIG_COMPOSITOR_XCM_V2

    // save rgb_vram_info as last step, in case something above fails.
    rgb_vram_info   = pNewLayer;
    _rgb_vram_layer_id = newLayerID;

    // erase buffer and force redraw
    rgba_buffer_clear();
    return 0;
}

/*
// TODO: Write implementationf for pure Ximr, if we ever want it.
#elif defined(CONFIG_COMPOSITOR_XIMR)
*/
#endif // CONFIG_COMPOSITOR_XCM

#endif // CONFIG_COMPOSITOR_DEDICATED_LAYER
