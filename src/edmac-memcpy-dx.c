/** \file
 * EDMAC memory-to-memory copy (edmac-memcpy.h API) for DIGIC X.
 *
 * Port of kitor's edmac-d8.c (EOS R 1.8.0, raw_draft_v2 0abe352f) to the
 * EOS R8 1.4.0. Both drive Canon's Engine::MemoryToMemoryEsub1.c module,
 * which pairs one read and one write EDMAC channel.
 *
 * Built for every camera; compiles to nothing without CONFIG_EDMAC_MEMCPY_DX.
 * Everything camera-specific (EDMAC channels, transfer mode, power list,
 * channel table) comes from the platform consts.h and stubs.S.
 */
/*
 * Copyright (C) 2026 Magic Lantern Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "dryos.h"
#include "edmac.h"          /* struct edmac_info, edmac_pid(), edmac_mode_ok(); includes ibus.h */
#include "edmac-memcpy.h"

#ifdef CONFIG_EDMAC_MEMCPY_DX

#ifndef CONFIG_DIGIC_X
#error "CONFIG_EDMAC_MEMCPY_DX is for DIGIC X (IBus addresses)"
#endif
#ifndef EDMAC_DX_CHANNEL_TABLE
#error "CONFIG_EDMAC_MEMCPY_DX needs EDMAC_DX_CHANNEL_TABLE in consts.h"
#endif

/*
 * MemoryToMemoryEsub1: R (DIGIC 8) vs R8 (DIGIC X)
 * ================================================
 *
 * Same module, same call sequence, same state layout - with three DIGIC X
 * changes that matter to us.
 *
 *                         R 1.8.0 (DIGIC 8)       R8 1.4.0 (DIGIC X)
 *   module state          0x243a8                 0x45358
 *   SetupEDMAC({rd,wr})   0xe0712bac              0xe0dc5d88
 *     read channels       40..47                  55..67
 *     write channels      1..13                   1..18 (0 asserts)
 *     write connection    0x3f7f0120 + (rd-40)    table at state+0x28:
 *                                                 [rd-55][wr<6 ? 0 : 1]
 *     completion          software: both EDMAC    hardware combiner
 *                         CBRs OR bits 1|2 into   0xd2402000 + slot*4,
 *                         state+0x18, ==3 -> CBR  slot taken in Setup
 *                                                 (0xe084fa74), freed in Term
 *   RegisterCBR(cb,arg)   0xe0712ca0              0xe0dc5ec6
 *   ResetCBR()            0xe0712caa              0xe0dc5ed0
 *   Start()               0xe0712cb6              0xe0dc5edc
 *   Stop()                -  (R stop path only    0xe0dc5ef6 (new: stops rd
 *                          does ResetCBR + Term)   and wr channels)
 *   Term()                0xe0712c86              0xe0dc5ea6
 *                         ("ClearEdmacCBR")       (+ frees combiner slot)
 *   SetAddress(struct)    0xe0712d86              0xe0dc5fc0
 *                         {u32 src, u32 dst}      {u64 src, u64 dst} IBus!
 *   SetSize(struct)       0xe0712da0              0xe0dc5ff0
 *                         {src_info*, dst_info*,  same struct; R8 dropped the
 *                          mode}, asserts on       field asserts
 *                          unused fields
 *   SetParam(struct)      -                       0xe0dc6020 (new; builds
 *                                                 sizes/offsets from geometry,
 *                                                 not needed here)
 *
 *  1. Addresses are 64-bit IBus addresses (DRAM channel << 32 | bus addr).
 *     The ROM's per-channel set_address (0xe0845824) validates the channel
 *     (asserts if not 1..3), writes the region register and the low word.
 *     See src/ibus.h.
 *  2. With MemiArbiter bandwidth control on (MEM(0x620c)), set_address
 *     also asserts if the EDMAC channel's bus master is not registered on
 *     the buffer's DRAM channel. We check that ourselves first, so a bad
 *     buffer fails the copy instead of crashing the camera.
 *  3. Completion is signalled by a hardware combiner, so Term() really must
 *     run after every transfer, or Setup() leaks combiner slots.
 *
 * Canon's caller on R8 is Prima::MemoryToMemory.c (0xe0d9c224 start,
 * 0xe0d9c280 stop), reached through Prima::MemoryToMemoryPublic.c. It picks
 * channels per "path" from a table (0xe01648ec, 0xe1f2349c); path 0 is
 * rd 66 -> wr 9. ML uses rd 66 -> wr 6 (EDMAC_DX_M2M_*): wr 6 was idle in
 * movie and photo LiveView, supports transfer modes 0..2 and may use all
 * three DRAM channels (tested on hardware, 0 bad bytes over ~10 GB).
 * Note: once ML has used an EDMAC channel, its registers stay "configured",
 * so free channels can't be found by reading registers later.
 *
 * Power: before touching any EDMAC register the EDMAC block must be powered
 * (else hard lock, same as on R). Block 0xd243xxxx (EDMAC ch 6..18, 66, 67)
 * is powered with PwrMng_WakeSubChips({1, 9, -1}) - exactly what
 * Prima::Mem1WritePath does for RAW LV channel 8. R used {0, 7}: on R8 the
 * IDs moved up (0..8 modules, 9 / 10 special) and the list is terminated by
 * any value >= 11 (we use -1), not by 7.
 *
 * Locking: R8 replaced R's CreateResLockEntry(ids, n) with the ERMGR API,
 * ErscCreateLockEntry(name, ids, n). It's a pure software conflict graph:
 * two entries exclude each other iff they share an ID; the IDs are not
 * checked against hardware. To be useful we'd need the IDs Canon uses for
 * these EDMAC channels - not decodable statically, so the lock is
 * only used if EDMAC_DX_M2M_RESIDS is defined (candidate in consts.h).
 */

/******************************** ROM API ********************************/

/* Engine::MemoryToMemoryEsub1.c */
struct m2m_e1_addr { uint64_t src; uint64_t dst; };   /* IBus addresses */
struct m2m_e1_size { struct edmac_info *src; struct edmac_info *dst; uint32_t mode; };

extern void MemToMemE1_SetupEDMAC(const uint32_t channels[2]);     /* {read, write} */
extern void MemToMemE1_RegisterCBR(void (*cbr)(void *), void *arg);
extern void MemToMemE1_ResetCBR(void);
extern void MemToMemE1_SetAddress(const struct m2m_e1_addr *addr);
extern void MemToMemE1_SetSize(const struct m2m_e1_size *size);
extern void MemToMemE1_Start(void);
extern void MemToMemE1_Stop(void);
extern void MemToMemE1_Term(void);

/* Engine::PwrMng.c - list of sub-chip IDs, terminated by a value >= 11 */
extern void PwrMng_WakeSubChips(const int32_t *list);
extern void PwrMng_SuspendSubChips(const int32_t *list);

/* ERMGR (engine resource manager) */
extern void    *ErscCreateLockEntry(const char *name, const uint32_t *ids, uint32_t count);
extern uint32_t ErscLockResources(void *entry, uint32_t timeout_ms);     /* bit 0 set = error */
extern uint32_t ErscUnLockResources(void *entry);

/***************************** configuration *****************************/

#ifndef EDMAC_DX_M2M_RD_CH
#error "define EDMAC_DX_M2M_RD_CH / EDMAC_DX_M2M_WR_CH / EDMAC_DX_M2M_SUBCHIPS in consts.h"
#endif

/* transfer mode, ROM set_transfer_mode (0xe08461a2): 0..3. kitor (R): 1 = 32, 2 = 64,
 * 3 = 128 bit. Not every EDMAC channel supports every mode: the ROM checks capability
 * bits in the channel table flags (bit 10 = mode 1, bit 9 = mode 2, bit 8 = mode 3;
 * mode 0 always OK) and ASSERTS otherwise (Engine::Edmac.c line 0x7fb..0x803).
 * Measured on R8: rd 66 and wr 6 = 0..2, wr 14 = 0..1 (mode 2 on wr 14 crashed
 * the camera). m2m_mode_supported() checks this before Canon does. */
#ifndef EDMAC_DX_M2M_MODE
#define EDMAC_DX_M2M_MODE 0
#endif

/* conservative alignment of the total transfer size, per mode (bytes) */
static const uint32_t m2m_mode_unit[4] = { 2, 4, 8, 16 };

/* base timeout, plus 1 ms per 256 KB (>= 256 MB/s even at transfer mode 0) */
#define M2M_TIMEOUT_MS 200

uint32_t edmac_read_chan  = EDMAC_DX_M2M_RD_CH;   /* used by mlv_lite edmac spy, needs edmac_get_base() */
uint32_t edmac_write_chan = EDMAC_DX_M2M_WR_CH;   /* -> stays disabled on DIGIC X (base = -1) */

static const int32_t  m2m_subchips[] = EDMAC_DX_M2M_SUBCHIPS;
static const uint32_t m2m_channels[2] = { EDMAC_DX_M2M_RD_CH, EDMAC_DX_M2M_WR_CH };

#ifdef EDMAC_DX_M2M_RESIDS
static const uint32_t m2m_resids[] = EDMAC_DX_M2M_RESIDS;
static void *m2m_lock_entry = NULL;
#endif

/******************************** state ********************************/

static struct semaphore *m2m_mutex = NULL;      /* one copy at a time */
static struct semaphore *m2m_done_sem = NULL;   /* given by completion CBR */
static volatile uint32_t m2m_done = 0;
static int m2m_power_refs = 0;                  /* guarded by m2m_mutex */

/* reason of the last failed copy, for the debug messages */
static const char *m2m_last_error = "none";

/* transfer mode, see EDMAC_DX_M2M_MODE; checked against the channel
 * capabilities once at init (falls back to 0) */
static uint32_t m2m_mode = EDMAC_DX_M2M_MODE & 3;

static int m2m_mode_supported(uint32_t mode);

static void edmac_dx_init()
{
    m2m_mutex    = create_named_semaphore("edmac_dx_mutex", SEM_CREATE_UNLOCKED);
    m2m_done_sem = create_named_semaphore("edmac_dx_done", SEM_CREATE_LOCKED);

    if (!m2m_mode_supported(m2m_mode))
    {
        DryosDebugMsg(0, 15, "[EDMAC_DX] mode %d not supported by rd %d / wr %d, using 0",
                      m2m_mode, EDMAC_DX_M2M_RD_CH, EDMAC_DX_M2M_WR_CH);
        m2m_mode = 0;
    }
}
INIT_FUNC("edmac_dx", edmac_dx_init);

/* Esub1 calls this once BOTH channels completed (hardware combiner). Interrupt context. */
static void m2m_complete_cbr(void *arg)
{
    m2m_done = 1;
    give_semaphore(m2m_done_sem);
}

/* requires m2m_mutex */
static void m2m_power_get(void)
{
    if (m2m_power_refs++ == 0)
        PwrMng_WakeSubChips(m2m_subchips);
}

/* requires m2m_mutex */
static void m2m_power_put(void)
{
    if (--m2m_power_refs == 0)
        PwrMng_SuspendSubChips(m2m_subchips);
}

static int m2m_mode_supported(uint32_t mode)
{
    return edmac_mode_ok(EDMAC_DX_M2M_RD_CH, mode) &&
           edmac_mode_ok(EDMAC_DX_M2M_WR_CH, mode);
}

/*
 * Will the ROM accept this buffer for this EDMAC channel?
 * ROM set_address asserts on: DRAM channel not 1..3, or (arbiter enabled)
 * bus master not registered on that DRAM channel. Returns IBus addr or 0.
 */
static uint64_t m2m_check_ibus(uint64_t ibus, uint32_t edmac_ch)
{
    uint32_t dram_ch = IBUS_DRAM_CH(ibus);
    if (dram_ch < 1 || dram_ch > 3)
    {
        m2m_last_error = "buffer not on a DRAM channel";
        return 0;
    }

    int pid = edmac_pid(edmac_ch);
    if (pid >= 0 && memiarb_allowed(pid, dram_ch) == 0)
    {
        m2m_last_error = "EDMAC master not registered on buffer's DRAM channel";
        return 0;
    }
    return ibus;
}

/*
 * One complete mem2mem transfer, same sequence as Canon's
 * Prima::MemoryToMemory.c start (0xe0d9c224) + stop (0xe0d9c280).
 * Blocking, task context only. Returns 0 on success.
 */
static int m2m_transfer(uint64_t src, uint64_t dst,
                        struct edmac_info *src_info, struct edmac_info *dst_info,
                        uint32_t bytes)
{
    uint64_t src_ibus = m2m_check_ibus(src, EDMAC_DX_M2M_RD_CH);
    uint64_t dst_ibus = m2m_check_ibus(dst, EDMAC_DX_M2M_WR_CH);
    if (!src_ibus || !dst_ibus)
    {
        DryosDebugMsg(0, 15, "[EDMAC_DX] reject %d:%08x -> %d:%08x: %s",
                      IBUS_DRAM_CH(src), IBUS_LO(src), IBUS_DRAM_CH(dst), IBUS_LO(dst), m2m_last_error);
        return 1;
    }

    take_semaphore(m2m_mutex, 0);

    if (!m2m_mode_supported(m2m_mode))
    {
        /* never let the ROM see it: it would assert */
        m2m_last_error = "transfer mode not supported by rd/wr channel";
        give_semaphore(m2m_mutex);
        return 1;
    }

    m2m_power_get();

    /* m2m_done_sem is always empty here: every give is matched by a take
     * (see the timeout path below). No timed "drain" loop - a 1 ms
     * take_semaphore() costs at least one scheduler tick per copy. */
    m2m_done = 0;

    struct m2m_e1_addr addr = { .src = src_ibus, .dst = dst_ibus };
    struct m2m_e1_size size = { .src = src_info, .dst = dst_info, .mode = m2m_mode };

    MemToMemE1_SetupEDMAC(m2m_channels);
    MemToMemE1_RegisterCBR(m2m_complete_cbr, NULL);
    MemToMemE1_SetAddress(&addr);
    MemToMemE1_SetSize(&size);
    MemToMemE1_Start();

    int timeout = take_semaphore(m2m_done_sem, M2M_TIMEOUT_MS + (bytes >> 18));

    MemToMemE1_Stop();
    MemToMemE1_ResetCBR();
    MemToMemE1_Term();

    /* completion raced with the timeout: the CBR already gave (or is about
     * to give) the semaphore - consume it so the next copy starts clean */
    if (timeout && m2m_done)
        take_semaphore(m2m_done_sem, 10);

    m2m_power_put();
    int ok = m2m_done;
    give_semaphore(m2m_mutex);

    if (timeout || !ok)
    {
        m2m_last_error = "timeout";
        DryosDebugMsg(0, 15, "[EDMAC_DX] timeout %d:%08x -> %d:%08x",
                      IBUS_DRAM_CH(src), IBUS_LO(src), IBUS_DRAM_CH(dst), IBUS_LO(dst));
        return 1;
    }
    return 0;
}

/****************************** public API ******************************/

void edmac_memcpy_res_lock()
{
    take_semaphore(m2m_mutex, 0);
    m2m_power_get();
    give_semaphore(m2m_mutex);

#ifdef EDMAC_DX_M2M_RESIDS
    if (!m2m_lock_entry)
        m2m_lock_entry = ErscCreateLockEntry("ML_EdmacDx", m2m_resids, COUNT(m2m_resids));
    if (!m2m_lock_entry || (ErscLockResources(m2m_lock_entry, 1000) & 1))
    {
        m2m_last_error = "ERMGR lock failed";
        NotifyBox(2000, "EDMAC ResLock fail");
    }
#endif
}

void edmac_memcpy_res_unlock()
{
#ifdef EDMAC_DX_M2M_RESIDS
    if (m2m_lock_entry)
        ErscUnLockResources(m2m_lock_entry);
#endif

    take_semaphore(m2m_mutex, 0);
    m2m_power_put();
    give_semaphore(m2m_mutex);
}

/*
 * Copy a w x h byte rectangle. All sizes in bytes.
 * Synchronous: the copy is complete when this returns, and cbr_r / cbr_w are
 * called from here (task context), so callers written for the asynchronous
 * DIGIC 4/5 version (mlv_lite) work unchanged.
 * Returns dst, or NULL on error (callbacks are not called then).
 */
void* edmac_copy_rectangle_cbr_start(void *dst, void *src,
                                     int src_width, int src_x, int src_y,
                                     int dst_width, int dst_x, int dst_y,
                                     int w, int h,
                                     void (*cbr_r)(void *), void (*cbr_w)(void *), void *cbr_ctx)
{
    if (!src || !dst || w <= 0 || h <= 0)
        return NULL;

    uint32_t unit = m2m_mode_unit[m2m_mode];
    if ((w * h) % unit)
    {
        m2m_last_error = "size not multiple of transfer unit";
        DryosDebugMsg(0, 15, "[EDMAC_DX] invalid size %d x %d (mod %d)", w, h, unit);
        return NULL;
    }

    /* the DMA reads RAM directly: flush dirty source lines first */
    if (src == CACHEABLE(src))
        sync_caches();

    /*  honour the source / destination offsets
     * (mlv_lite uses src_x / src_y to skip the optical black borders) */
    const uint8_t *src_adjusted  = (const uint8_t *)src + src_x + src_y * src_width;
    uint8_t       *dst_adjusted  = (uint8_t *)dst + dst_x + dst_y * dst_width;

    /* xb = bytes per line, yb = lines - 1, off1b = bytes skipped after each line */
    struct edmac_info src_info = { .xb = w, .yb = h - 1, .off1b = src_width - w };
    struct edmac_info dst_info = { .xb = w, .yb = h - 1, .off1b = dst_width - w };

    if (m2m_transfer(cpu_to_ibus(src_adjusted), cpu_to_ibus(dst_adjusted), &src_info, &dst_info, w * h))
        return NULL;

    if (cbr_r) cbr_r(cbr_ctx);
    if (cbr_w) cbr_w(cbr_ctx);
    return dst;
}

/**
 * Same rectangle copy on IBus addresses (any DRAM channel, e.g. DRAM Ch3
 * memory from exmem_dram3_alloc). Widths and offsets in bytes.
 * Blocking. Returns 0 on success.
 */
int edmac_copy_rectangle_ibus(uint64_t dst, uint64_t src,
                              int src_width, int src_x, int src_y,
                              int dst_width, int dst_x, int dst_y,
                              int w, int h)
{
    if (!src || !dst || w <= 0 || h <= 0 || (w * h) % m2m_mode_unit[m2m_mode])
    {
        m2m_last_error = "invalid size";
        return 1;
    }

    struct edmac_info src_info = { .xb = w, .yb = h - 1, .off1b = src_width - w };
    struct edmac_info dst_info = { .xb = w, .yb = h - 1, .off1b = dst_width - w };
    return m2m_transfer(src + src_x + src_y * src_width,
                        dst + dst_x + dst_y * dst_width,
                        &src_info, &dst_info, w * h);
}

/* nothing to clean up: m2m_transfer() leaves the module idle */
void edmac_copy_rectangle_adv_cleanup()
{
}

void* edmac_copy_rectangle_adv(void *dst, void *src,
                               int src_width, int src_x, int src_y,
                               int dst_width, int dst_x, int dst_y,
                               int w, int h)
{
    return edmac_copy_rectangle_cbr_start(dst, src, src_width, src_x, src_y,
                                          dst_width, dst_x, dst_y, w, h, NULL, NULL, NULL);
}

void* edmac_copy_rectangle(void *dst, void *src, int src_width, int x, int y, int w, int h)
{
    return edmac_copy_rectangle_adv(dst, src, src_width, x, y, w, 0, 0, w, h);
}

/* largest line width <= 32K that splits length into whole lines, 0 if none */
static uint32_t m2m_line_width(uint32_t length)
{
    uint32_t unit = m2m_mode_unit[m2m_mode];
    if (length % unit)
        return 0;

    for (uint32_t wd = MIN(length, 0x8000) & ~(unit - 1); wd >= unit; wd -= unit)
    {
        if (length % wd == 0)
            return wd;
    }
    return 0;
}

/* plain copy; falls back to memcpy for sizes EDMAC can't do */
void* edmac_memcpy(void *dst, void *src, size_t length)
{
    uint32_t width = m2m_line_width(length);
    if (!width || !edmac_copy_rectangle_adv(dst, src, width, 0, 0, width, 0, 0, width, length / width))
        return memcpy(dst, src, length);
    return dst;
}

/* plain copy on IBus addresses; no CPU fallback. Returns 0 on success. */
int edmac_copy_ibus(uint64_t dst, uint64_t src, uint32_t length)
{
    uint32_t width = m2m_line_width(length);
    if (!width)
    {
        m2m_last_error = "size not multiple of transfer unit";
        return 1;
    }
    return edmac_copy_rectangle_ibus(dst, src, width, 0, 0, width, 0, 0, width, length / width);
}

#endif // CONFIG_EDMAC_MEMCPY_DX
