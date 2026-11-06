#ifndef _edmac_c
#define _edmac_c

#include "module.h"
#include "compiler.h" // for SIZE_CHECK_STRUCT

#define EDMAC_WRITE_0  0
#define EDMAC_WRITE_1  1
#define EDMAC_WRITE_2  2
#define EDMAC_WRITE_3  3
#define EDMAC_WRITE_4  4
#define EDMAC_WRITE_5  5
#define EDMAC_WRITE_6  6
#define EDMAC_READ_0   8
#define EDMAC_READ_1   9
#define EDMAC_READ_2   10
#define EDMAC_READ_3   11
#define EDMAC_READ_4   12
#define EDMAC_READ_5   13
#define EDMAC_WRITE_7  16
#define EDMAC_WRITE_8  17
#define EDMAC_WRITE_9  18
#define EDMAC_WRITE_10 19
#define EDMAC_WRITE_11 20
#define EDMAC_WRITE_12 21
#define EDMAC_WRITE_13 22
#define EDMAC_READ_6   24
#define EDMAC_READ_7   25
#define EDMAC_READ_8   26
#define EDMAC_READ_9   27
#define EDMAC_READ_10  28
#define EDMAC_READ_11  29
#define EDMAC_WRITE_14 33
#define EDMAC_WRITE_15 34
#define EDMAC_READ_12  40
#define EDMAC_READ_13  41
#define EDMAC_READ_14  42
#define EDMAC_READ_15  43

#define EDMAC_DIR_READ    0
#define EDMAC_DIR_WRITE   1
#define EDMAC_DIR_UNUSED  2

#define EDMAC_BYTES_PER_TRANSFER_MASK_D4    0x60000000  /* digic 4: up to 4 bytes per transfer */
#define EDMAC_BYTES_PER_TRANSFER_MASK_D5    0x60001000  /* digic 5: up to 16 bytes per transfer */
#define EDMAC_16_BYTES_PER_TRANSFER         0x40001000
#define EDMAC_8_BYTES_PER_TRANSFER          0x20001000
#define EDMAC_4_BYTES_PER_TRANSFER          0x40000000
#define EDMAC_2_BYTES_PER_TRANSFER          0x20000000

#if defined(CONFIG_DIGIC_8X)
struct edmac_info
{
    // Extra "S" fields, probably 3rd dimension?
    unsigned int off1s; //D8+
    unsigned int off1a;
    unsigned int off1b;
    unsigned int off2s; //D8+
    unsigned int off2a;
    unsigned int off2b;
    unsigned int off3;
    unsigned int xs; //D8+
    unsigned int xa;
    unsigned int xb;
    unsigned int ys; //D8+
    unsigned int ya;
    unsigned int yb;
    unsigned int xn;
    unsigned int yn;
};
#else
struct edmac_info
{
    unsigned int off1a;
    unsigned int off1b;
    unsigned int off2a;
    unsigned int off2b;
    unsigned int off3;
    unsigned int xa;
    unsigned int xb;
    unsigned int ya;
    unsigned int yb;
    unsigned int xn;
    unsigned int yn;
};
#endif


// This represents an EDmac "channel", also called a "port"
// from Digic 8 onwards.  These are very likely not really 0x100
// in size, probably the DMA controller aligns to this value
// for hardware reasons.  I sized it that way to allow easy
// pointer increment, etc.
#if defined(CONFIG_DIGIC_X)
/**
 * DIGIC X EDMAC channel registers (R8 1.4.0, Engine::Edmac.c / ELD/Edmac/Edmac.c).
 *
 * Mapped from the ROM's own register writes, compared with R 1.8.0 (DIGIC 8):
 *
 *   function          R (D8)                  R8 (DX)
 *   set_size          0x48 ys_xs ...          0x38 xs, 0x3c ys, 0x40 xa, 0x44 ya,
 *                     0x54 yn_xn (packed)     0x48 xb, 0x4c yb, 0x50 xn, 0x54 yn
 *                                             (x and y in separate registers)
 *                     0x58..0x70 offsets      0x58..0x70 offsets (unchanged)
 *   set_address       0xa0 = 32-bit CPU addr  0xa4 = IBus addr low 32 bits
 *                                             0xa0 = region (DRAM ch | ch<<4)
 *                                             0xa8 = region (plain ch), on
 *                                                    channels with flag 0x80/0x40
 *   set_transfer_mode 0xc0 (mask 0x70)        0xc4 (bits 1..0: 0..3)
 *   start             0xb4 = 1                0xb8 = 3 (normal channels),
 *                                             0xb4 = 1 / 0xc0 = 1 (flag 0x80 / 0x40)
 *
 * The "flag" is the byte at +0x10 of the ROM EDMAC channel table
 * (R8 0xe1bf6d04, 0x14 bytes per channel: base, base2, slot, pid, flags).
 * The EDMAC channels ML uses (8, 6, 66) have neither 0x80 nor 0x40 set, so they use
 * `region` at 0xa0 and `start` at 0xb8.
 *
 * ram_addr alone is meaningless on DIGIC X: the same 32-bit bus address exists
 * once per DRAM channel. Use edmac_mmio_get_ibus() / edmac_mmio_get_cpu().
 */
struct edmac_mmio
{
    uint32_t unk_00;            // 0x00
    uint32_t unk_04;            // 0x04
    uint32_t unk_08[2];         // 0x08, 0x0c
    uint32_t unk_10;            // 0x10
    uint32_t unk_14;            // 0x14
    uint32_t reset_a;           // 0x18  reset pulse: 0x80000000 then 0 (also on pack units, 0xe0845628)
    uint32_t reset_b;           // 0x1c  second reset variant
    uint32_t ctrl_20;           // 0x20
    uint32_t ctrl_24;           // 0x24
    uint32_t unk_28;            // 0x28
    uint32_t status;            // 0x2c  read in the IRQ path
    uint32_t state_ack;         // 0x30
    uint32_t irq_enable;        // 0x34

    /* size: one register per field; ROM set_size 0xe084587c masks them as noted.
     * Channels with flag 0x80/0x40 use a different set_size (0xe08459b0). */
    uint32_t xs;                // 0x38  & 0x3fffe
    uint32_t ys;                // 0x3c  15 bits
    uint32_t xa;                // 0x40  & 0x3fffe
    uint32_t ya;                // 0x44  15 bits
    uint32_t xb;                // 0x48  & 0x3fffe, bytes per line
    uint32_t yb;                // 0x4c  15 bits, lines - 1
    uint32_t xn;                // 0x50  13 bits (non-zero xn/yn needs channel flag bit 11)
    uint32_t yn;                // 0x54  16 bits

    /* offsets: same order as DIGIC 8; ROM clears bit 0 (even byte offsets) */
    uint32_t off1s;             // 0x58
    uint32_t off2s;             // 0x5c
    uint32_t off1a;             // 0x60
    uint32_t off2a;             // 0x64
    uint32_t off1b;             // 0x68
    uint32_t off2b;             // 0x6c
    uint32_t off3;              // 0x70

    uint32_t unk_74[0x0b];      // 0x74..0x9c  used by the flag 0x80/0x40 set_size only

    uint32_t region;            // 0xa0  DRAM channel, written as ch | ch << 4 (e.g. 0x22)
                                //       by 0xe084440a (MemiArbiter check + write), called
                                //       from set_address 0xe0845824
    uint32_t ram_addr;          // 0xa4  IBus address, low 32 bits
    uint32_t region_alt;        // 0xa8  DRAM channel (plain), flag 0x80/0x40 channels only
    uint32_t unk_ac[2];         // 0xac..0xb0
    uint32_t start_alt;         // 0xb4  write 1 = start, channels with flag 0x80
    uint32_t start;             // 0xb8  write 3 = start, normal channels (0xe08443be, 0xe0844202)
    uint32_t stop;              // 0xbc  written by the stop / abort paths
    uint32_t start_alt2;        // 0xc0  write 1 = start, channels with flag 0x40
    uint32_t transfer_mode;     // 0xc4  bits 1..0 = mode 0..3; modes 1..3 only on channels with
                                //       flag bit 10 / 9 / 8, ROM 0xe08461a2 asserts otherwise
    uint32_t unk_c8[0x0e];      // 0xc8..0xfc
};
#elif defined(CONFIG_DIGIC_8X)
// Digic 8 changes the struct significantly
struct edmac_mmio
{
    uint32_t unk_01[0xb];
    uint32_t ModeInfo;
    uint32_t unk_02[0x3];
    uint32_t cbr_registered;
    uint32_t unk03[0x2];
    uint32_t ys_xs;
    uint32_t ya_xa;
    uint32_t yb_xb;
    uint32_t yn_xn;
    uint32_t off1s;
    uint32_t off2s;
    uint32_t off1a;
    uint32_t off2a;
    uint32_t off1b;
    uint32_t off2b;
    uint32_t off3;
    uint32_t unk_04[0xb];
    uint32_t ram_addr;
    uint32_t unk_05[0x7];
    uint32_t transfer_mode;
    uint32_t unk_06[0x3];
    uint32_t PackUnpackInfo;
    uint32_t unk_07[0xb]; // some of this is padding, but I don't know how much
};
#else
// For some more, not terribly clear info on pre-DIGIC8, see:
// https://magiclantern.fandom.com/wiki/Register_Map#EDMAC
// near "SDRAM destination offset"
struct edmac_mmio
{
    uint32_t dma_state;
    uint32_t dma_flags;
    uint32_t ram_addr;
    uint32_t yn_xn;
    uint32_t yb_xb;
    uint32_t ya_xa;
    uint32_t off1b;
    uint32_t off2b;
    uint32_t off1a;
    uint32_t off2a;
    uint32_t off3;
    uint32_t unk_01;
    uint32_t irq_reason;
    uint32_t irq_state_related;
    uint32_t unk_02;
    uint32_t unk_03;
    uint32_t fencing_related_maybe;
    uint32_t unk_04[0x2f]; // some of this is padding, but I don't know how much
};
#endif
SIZE_CHECK_STRUCT(edmac_mmio, 0x100);

#if defined(CONFIG_DIGIC_X)
#include "ibus.h"

/**
 * DIGIC X EDMAC channel table (ROM, platform consts.h EDMAC_DX_CHANNEL_TABLE):
 * 0x14 bytes per EDMAC channel {base, base2, slot, pid, flags}.
 *
 * flags: bit 1 = read channel; bit 10 / 9 / 8 = transfer mode 1 / 2 / 3
 * supported (mode 0 always; the ROM's set_transfer_mode 0xe08461a2 asserts
 * on an unsupported mode); bit 11 = non-zero xn/yn allowed; 0x80 / 0x40 =
 * the other register layout (region_alt, start_alt / start_alt2).
 */
#ifdef EDMAC_DX_CHANNEL_TABLE
#ifndef EDMAC_DX_CHANNEL_COUNT
#define EDMAC_DX_CHANNEL_COUNT 0x6f
#endif

static inline uint32_t edmac_table_word(uint32_t edmac_ch, uint32_t offset)
{
    if (edmac_ch >= EDMAC_DX_CHANNEL_COUNT)
        return 0;
    return *(volatile uint32_t *)(EDMAC_DX_CHANNEL_TABLE + edmac_ch * 0x14 + offset);
}

/** MMIO base of an EDMAC channel, 0 if unknown */
static inline uint32_t edmac_dx_base(uint32_t edmac_ch)  { return edmac_table_word(edmac_ch, 0x00); }

/** bus master pid of an EDMAC channel (for memiarb_allowed()), -1 if unknown */
static inline int edmac_pid(uint32_t edmac_ch)
{
    return edmac_ch < EDMAC_DX_CHANNEL_COUNT ? (int)edmac_table_word(edmac_ch, 0x0c) : -1;
}

static inline uint32_t edmac_flags(uint32_t edmac_ch)    { return edmac_table_word(edmac_ch, 0x10); }
static inline int edmac_is_read(uint32_t edmac_ch)       { return (edmac_flags(edmac_ch) >> 1) & 1; }

/** 1 if the EDMAC channel supports transfer mode 0..3 (else the ROM asserts) */
static inline int edmac_mode_ok(uint32_t edmac_ch, uint32_t mode)
{
    if (mode == 0) return 1;
    if (mode > 3)  return 0;
    return (edmac_flags(edmac_ch) >> (11 - mode)) & 1;
}
#endif // EDMAC_DX_CHANNEL_TABLE

/**
 * DRAM channel a DIGIC X EDMAC channel is set up for (0 = not set up).
 * `region` holds the DRAM channel in both nibbles; accept only consistent values.
 */
static inline uint32_t edmac_mmio_get_dram_channel(volatile struct edmac_mmio *mmio)
{
    uint32_t region  = mmio->region & 0xff;
    uint32_t dram_ch = region & 0xf;
    if (dram_ch < 1 || dram_ch > 3 || region != (dram_ch | (dram_ch << 4)))
        return 0;
    return dram_ch;
}

/** IBus address an EDMAC channel is set up for, 0 if invalid */
static inline uint64_t edmac_mmio_get_ibus(volatile struct edmac_mmio *mmio)
{
    uint32_t dram_ch = edmac_mmio_get_dram_channel(mmio);
    uint32_t bus     = mmio->ram_addr;
    if (!dram_ch || !bus)
        return 0;
    return IBUS_ADDR(dram_ch, bus);
}

/** CPU address (cached) of an EDMAC channel's buffer, 0 if unmapped (e.g. DRAM Ch3) */
static inline uint32_t edmac_mmio_get_cpu(volatile struct edmac_mmio *mmio)
{
    return ibus_to_cpu(edmac_mmio_get_ibus(mmio));
}

/**
 * Point a (running) DIGIC X EDMAC channel at a CPU buffer.
 * Same register order as the ROM's set_address (R8 0xe0845824): region, then address.
 * Only for EDMAC channels using `region` at 0xa0 (no 0x80/0x40 flag), see above.
 * No MemiArbiter check here: the caller must know the channel may use that DRAM channel.
 * Returns 0 on success, -1 if the buffer is not on a DRAM channel.
 */
static inline int edmac_mmio_set_cpu(volatile struct edmac_mmio *mmio, const void *cpu)
{
    uint64_t ibus    = cpu_to_ibus(cpu);
    uint32_t dram_ch = IBUS_DRAM_CH(ibus);
    if (!dram_ch)
        return -1;
    mmio->region   = dram_ch | (dram_ch << 4);
    mmio->ram_addr = IBUS_LO(ibus);
    return 0;
}
#endif // CONFIG_DIGIC_X

void EDMAC_Register_Complete_CBR(unsigned int channel, void (*cbr)(), unsigned int ctx);
void SetEDmac(unsigned int channel, void *address, struct edmac_info *ptr, int flags);
void StartEDmac(unsigned int channel, int flags);
void AbortEDmac(unsigned int channel);
void ConnectWriteEDmac(unsigned int channel, unsigned int where);
void ConnectReadEDmac(unsigned int channel, unsigned int where);
unsigned int GetEdmacAddress(unsigned int channel);

uint32_t edmac_channel_to_index(uint32_t channel);
uint32_t edmac_index_to_channel(uint32_t index, uint32_t direction);

uint32_t edmac_get_flags(uint32_t channel);
uint32_t edmac_get_state(uint32_t channel);     /* 0=idle, 1=running (from hardware) */
uint32_t edmac_get_base(uint32_t channel);      /* base register */
uint32_t edmac_get_channel(uint32_t reg);       /* channel from register */
uint32_t edmac_get_address(uint32_t channel);   /* start address */
uint32_t edmac_get_pointer(uint32_t channel);   /* current address (from hardware) */
uint32_t edmac_get_length(uint32_t channel);    /* yb,xb (hi,lo) */
uint32_t edmac_get_connection(uint32_t channel, uint32_t direction);
uint32_t edmac_get_dir(uint32_t channel);

/* off1/off2 are signed on some odd number of bits;
 * use this to extend the sign bit to int32 */
int edmac_fix_off1(int32_t off);
int edmac_fix_off2(int32_t off);

struct edmac_info edmac_get_info(uint32_t channel);
uint32_t edmac_get_total_size(struct edmac_info * info, int include_offsets);

uint32_t edmac_bytes_per_transfer(uint32_t flags);

/* provided by edmac.mo */
#if defined(MODULE)
char * edmac_format_size(struct edmac_info * info);
#else
static char * (*edmac_format_size)(struct edmac_info * info) = MODULE_FUNCTION(edmac_format_size);
#endif

// EDMAC mem -> mem copy related, D7.  D7 uses a global for locking
// the (I assume) mem2mem subsystem.
// D8X do it a different way: a function that creates a ResLock, locks it
// with LockEngineResources, then returns it.
// Maybe we could wrap the D7 way and pretend it's the same as D8, to unify
// the interface?
void create_mem_to_mem_lock_and_pwrmng_globals(uint32_t src_channel, uint32_t dst_channel,
                                               void *resIDs, uint32_t resID_count);
void lock_and_wake_mem_to_mem(void);
void unlock_and_sleep_mem_to_mem(void);
void cleanup_mem_to_mem(void);
void delete_mem_to_mem_lock(void);
void set_mem_to_mem_cbr(void *cbr, void *param); // when both dst and src channels are complete, this cbr fires
void mem_to_mem_setup_copy(struct edmac_info *src, struct edmac_info *dst);
void mem_to_mem_set_src_dst_and_start(void *src, void *dst);
void set_default_mem_to_mem_cbr(void);

struct LockEntry *CreateResLockEntry(uint32_t *resIds, uint32_t resIdCount);
unsigned int LockEngineResources(struct LockEntry *lockEntry);
unsigned int UnLockEngineResources(struct LockEntry *lockEntry);

void RegisterEDmacCompleteCBR(int channel, void (*cbr)(void*), void* cbr_ctx);
void RegisterEDmacAbortCBR(int channel, void (*cbr)(void*), void* cbr_ctx);
void RegisterEDmacPopCBR(int channel, void (*cbr)(void*), void* cbr_ctx);

void UnregisterEDmacCompleteCBR(int channel);
void UnregisterEDmacAbortCBR(int channel);
void UnregisterEDmacPopCBR(int channel);

#endif
