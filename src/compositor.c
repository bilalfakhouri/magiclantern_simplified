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
#include "exmem.h"

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

#ifdef CONFIG_DIGIC_X
/* layer memory, kept for the whole session */
static struct ibus_buf layer_buf;
#endif

struct MARV *_compositor_create_layer(uint32_t bmp_w, uint32_t bmp_h)
{
    // buffer for MARV structure
    struct MARV* pNewLayer = malloc(sizeof(struct MARV));
    if(pNewLayer == NULL) return NULL;

#ifdef CONFIG_DIGIC_X
    // Width (= pitch) rounded up to 32 px, as Canon's CreateMARV does.
    // Buffer aligned to 0x100 and uncacheable, on a DRAM channel Ximr may
    // read (exmem checks it against MemiArbiter and gives the IBus address).
    uint32_t pitch = (bmp_w + 31) & ~31;
    if (exmem_ibus_alloc(&layer_buf, pitch * bmp_h * 4, 0x100, XIMR_PID, IBUS_BUF_LONG_LIVED))
    {
        free(pNewLayer); // cleanup
        return NULL;
    }
    uint8_t* pBitmapData = layer_buf.cpu;
#else
    uint32_t pitch = bmp_w;

    // buffer for layer data, +0x100 for alignment
    uint8_t* pBitmapData = malloc((BMP_VRAM_SIZE * 4) + 0x100);
    if(pBitmapData == NULL) {
        free(pNewLayer); // cleanup
        return NULL;
    }
    
    // Align to 0x100 and use uncacheable region.
    // This is expected by some Zico functions.
    pBitmapData = UNCACHEABLE((uint8_t*)((((uintptr_t)pBitmapData + 0x100) >> 8) << 8));
#endif

    // fill MARV data
    pNewLayer->signature    = 0x5652414D;  // MARV
    pNewLayer->bitmap_data  = pBitmapData;
    pNewLayer->opacity_data = 0x0;
    pNewLayer->flags        = XIMR_FLAGS_LAYER_RGBA; // see compositor.h
#ifdef CONFIG_DIGIC_X
    pNewLayer->unused_0c    = 0;
    pNewLayer->ibus_addr    = layer_buf.ibus;
    pNewLayer->req_width    = bmp_w;
    pNewLayer->unused_2c    = 0;
#endif
    pNewLayer->width        = pitch;
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

#ifdef CONFIG_DIGIC_X
/*
 * DIGIC X (verified on R8) uses the same XCM_V2 API as DIGIC 8. Differences:
 *  - MARV has the 64-bit IBus address of the layer data (see vram.h).
 *  - Ximr (bus master XIMR_PID) may only read some DRAM channels (R8: Ch1
 *    and Ch2, not Ch3) and asserts on others; exmem_ibus_alloc() checks it.
 *  - The display is described in bmp.h (DISPLAY_W, ...); our layer holds
 *    just the display and bmp.c scales ML's screen into it.
 */
#if !defined(CONFIG_COMPOSITOR_XCM_V2) || !defined(CONFIG_RSCMGR_IBUS) || !defined(XIMR_PID) || !defined(DISPLAY_W)
#error "DIGIC X dedicated layer needs CONFIG_COMPOSITOR_XCM_V2, CONFIG_RSCMGR_IBUS, XIMR_PID and DISPLAY_W"
#endif
#endif

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
extern uint32_t XCM_SetLayerOffset(void * pXCM, uint layer, uint16_t dstX, uint16_t dstY);
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

    // Find first available, non used layer slot
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

#ifdef DISPLAY_W
    // our layer holds just the display; bmp.c scales ML's screen into it
    uint32_t bmp_w = DISPLAY_W;
    uint32_t bmp_h = DISPLAY_H;
#else
    // compute our layer size
    uint32_t bmp_w = BMP_W_PLUS - BMP_W_MINUS;
    uint32_t bmp_h = BMP_H_PLUS - BMP_H_MINUS;
#endif

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
#ifdef DISPLAY_W
    // whole layer, at the display's top left (offset is in display pixels);
    // a free slot may still have an old area / offset
    XCM_SetSourceArea(_pXCM, newLayerID, 0, 0, DISPLAY_W, DISPLAY_H);
    XCM_SetLayerOffset(_pXCM, newLayerID, 0, 0);
#else
    XCM_SetSourceArea(_pXCM, newLayerID, -BMP_W_MINUS, -BMP_H_MINUS, BMP_W_PLUS + BMP_W_MINUS, BMP_H_PLUS + BMP_H_MINUS);
#endif
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
