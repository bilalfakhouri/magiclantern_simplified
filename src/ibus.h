/** \file
 * DIGIC X IBus helpers: translate between CPU addresses and the 64-bit
 * "IBus" addresses used by EDMAC, Ximr and other DIGIC X bus masters,
 * and check which bus master may use which DRAM channel (MemiArbiter).
 *
 * Header-only (static inline). Include after dryos.h (needs consts.h).
 * Compiles to nothing on non DIGIC X models.
 *
 * See also the DIGIC X IBus and Memory Guide.
 */
#ifndef _ibus_h_
#define _ibus_h_

#include <stdint.h>

#ifdef CONFIG_DIGIC_X

/**
 * IBus addresses
 * ==============
 *
 * DIGIC X RAM is split over three DRAM channels (R8: Ch1 1 GB, Ch2 1 GB,
 * Ch3 2 GB). Hardware (bus masters) addresses it with 64-bit IBus addresses:
 *
 *     IBus = (DRAM channel << 32) | bus address
 *
 * written "dram_ch:bus" in comments, e.g. 2:10080000. The same 32-bit bus
 * address exists once per DRAM channel, so anything handed to hardware must
 * carry the DRAM channel too.
 *
 * Fixed memory windows (MEMIF)
 * ============================
 *
 * The CPU sees RAM through 12 fixed windows of 256 MB covering CPU
 * 0x00000000..0xBFFFFFFF. Window i is one register at MEMIF_WINDOW_REGS + 4*i:
 *
 *     bits 31..28 : bus address bits 31..28 for this window
 *     bits  2..0  : DRAM channel (1..3; 0 = nothing / not RAM)
 *
 * R8 boot code (0xe000a880) programs them once:
 *
 *     CPU window          reg value    -> IBus                cache
 *     0x0xxxxxxx  (0)     0x00000001      1:0xxxxxxx           cached
 *     0x1xxxxxxx  (1)     0x10000001      1:1xxxxxxx           cached
 *     0x2xxxxxxx  (2)     0x00000002      2:0xxxxxxx           cached
 *     0x3xxxxxxx  (3)     0x10000002      2:1xxxxxxx           cached
 *     0x4..0x7            same as 0..3                         uncached copy
 *     0x8xxxxxxx  (8)     0x20000001      1:2xxxxxxx           uncached only
 *     0x9xxxxxxx  (9)     0x30000001      1:3xxxxxxx           uncached only
 *     0xAxxxxxxx  (10)    0x20000002      2:2xxxxxxx           uncached only
 *     0xBxxxxxxx  (11)    0x30000002      2:3xxxxxxx           uncached only
 *
 * Caching comes from the MMU (0x02000000..0x3FFFFFFF cached, 0x40000000..
 * 0xBEFFFFFF uncached). CACHEABLE() / UNCACHEABLE() (mem_defs.h) switch
 * between windows 0..3 and 4..7 and correctly do nothing on 0x8..0xB.
 *
 * No fixed window maps DRAM Ch3: the CPU cannot reach it, only hardware.
 * The 4 dynamic windows 12..15 (CPU 0xC2000000..0xC9FFFFFF, device memory)
 * are pointed at DRAM Ch3 only by the bootloader RAM test; at runtime they
 * map DRAM Ch1. ML leaves them alone.
 *
 * Canon's equivalents, which assert on bad input (ML's versions return 0):
 *     MemifWindow_GetIBusAddress(void *cpu)  0xe092f20c, asserts if cpu >= 0xC0000000
 *     MemifWindow_GetCpuAddress(u64 ibus)    0xe092f246, asserts if no window maps it
 *     IBus address check                     0xe0857746, "Invalid IBUS Address!!"
 *
 * Allocating memory "on IBus": every RscMgr pool is ordinary RAM on some
 * DRAM channel; use GetIBusAddressOfMemoryChunk() + ibus_to_cpu() (see
 * exmem.c, CONFIG_RSCMGR_IBUS). DRAM Ch3 is only reachable through fixed
 * areas handed out by RscMgr WorkMem (not used by ML yet).
 */

#ifndef MEMIF_WINDOW_REGS
#define MEMIF_WINDOW_REGS   0xd2044080      /* R8 value; other DIGIC X models not verified */
#endif
#define MEMIF_WINDOW_COUNT  12              /* 12 x 256 MB = CPU 0x00000000..0xBFFFFFFF */

#define IBUS_ADDR(dram_ch, bus) ((((uint64_t)(dram_ch)) << 32) | (uint32_t)(bus))
#define IBUS_DRAM_CH(ibus)      ((uint32_t)((ibus) >> 32))
#define IBUS_LO(ibus)           ((uint32_t)(ibus))

static inline uint32_t memif_window(uint32_t win)
{
    return *(volatile uint32_t *)(MEMIF_WINDOW_REGS + 4 * win);
}

/**
 * CPU address -> IBus address, 0 if not covered by a RAM window.
 * Cached and uncached addresses give the same result.
 */
static inline uint64_t cpu_to_ibus(const void *cpu)
{
    uint32_t addr = (uint32_t)cpu;
    uint32_t win  = addr >> 28;
    if (win >= MEMIF_WINDOW_COUNT)
        return 0;

    uint32_t reg     = memif_window(win);
    uint32_t dram_ch = reg & 7;
    if (dram_ch == 0)
        return 0;

    return IBUS_ADDR(dram_ch, (reg & 0xF0000000) | (addr & 0x0FFFFFFF));
}

/**
 * IBus address -> CPU address, 0 if no window maps it (always for DRAM Ch3).
 * Windows 0..3 are searched first, so the cached address comes back when
 * there is one; use UNCACHEABLE() / CACHEABLE() on the result as needed.
 */
static inline uint32_t ibus_to_cpu(uint64_t ibus)
{
    uint32_t dram_ch = IBUS_DRAM_CH(ibus);
    uint32_t bus     = IBUS_LO(ibus);
    if (dram_ch < 1 || dram_ch > 3)
        return 0;

    for (uint32_t win = 0; win < MEMIF_WINDOW_COUNT; win++)
    {
        uint32_t reg = memif_window(win);
        if ((reg & 7) == dram_ch && (reg & 0xF0000000) == (bus & 0xF0000000))
            return (win << 28) | (bus & 0x0FFFFFFF);
    }
    return 0;
}

/** DRAM channel of a CPU buffer: 1..3, 0 if not RAM */
static inline uint32_t cpu_dram_channel(const void *cpu)
{
    return IBUS_DRAM_CH(cpu_to_ibus(cpu));
}

/**
 * MemiArbiter (Peripheral::MemiArbiter.c): every bus master has a pid, and
 * Canon keeps a list of the pids allowed on each DRAM channel. Using a DRAM
 * channel the pid is not registered for asserts with
 * "pid(%d) is not registered in DRAM-Ch%d" (R8 0xe09225bc).
 *
 * MEMIARB_PID_TABLE (platform consts.h): 253 bytes per DRAM channel from
 * Ch1, byte[pid] == 1 if allowed; first byte 0xff = not filled yet.
 * Canon checks only while MEMIARB_ENABLED != 0. The DRAM Ch3 list changes
 * between movie and photo mode, so check right before use.
 * EDMAC pids: see edmac_pid() in edmac.h. Ximr (display) is pid 0x12.
 *
 * Returns 1 = allowed, 0 = not allowed (Canon would assert), -1 = unknown.
 */
static inline int memiarb_allowed(int pid, uint32_t dram_ch)
{
#ifdef MEMIARB_PID_TABLE
    const uint8_t *table = (const uint8_t *)MEMIARB_PID_TABLE;
    if (pid < 0 || pid > 252 || dram_ch < 1 || dram_ch > 3)
        return -1;
    if (table[0] == 0xff)
        return -1;
    return table[(dram_ch - 1) * 253 + pid] == 1;
#else
    (void)pid; (void)dram_ch;
    return -1;
#endif
}

#endif // CONFIG_DIGIC_X
#endif // _ibus_h_
