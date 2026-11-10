/*
 *  EOS R8 1.4.0 consts
 */

#define CARD_LED_ADDRESS            0xd22392f0   /* focus aid led on R8 */
#define LEDON                       0x24D0002
#define LEDOFF                      0x24C0003

#define BR_DCACHE_CLN_1     0xe0100058   /* first call to dcache_clean, before cstart */
#define BR_ICACHE_INV_1     0xe0100062   /* first call to icache_invalidate, before cstart */
#define BR_DCACHE_CLN_2     0xe0100090   /* second call to dcache_clean, before cstart */
#define BR_ICACHE_INV_2     0xe010009a   /* second call to icache_invalidate, before cstart */
#define BR_BR_CSTART        0xe01000b0   // indirect branch to cstart; the first branch is
                                         // absolute to original, we must patch
#define BR_BZERO32          0xe010013c   /* called from cstart */
#define BR_CREATE_ITASK     0xe010019e   /* called from cstart */

// from dryos bootloader to init_task + a little bit of overhead just in case
#define FIRMWARE_ENTRY_LEN 0x1000

#define CANON_ORIG_MMU_TABLE_ADDR 0xe0000000 // R8's copy_mmu_tables (0xe000c8e4)
                                             // memcpy()s 0x4900 bytes from 0xe0000000

#define DRYOS_SGI_HANDLERS_PTR 0x400c

/* "Malloc Information" */
#define MALLOC_STRUCT_ADDR 0xF3610
//#define MALLOC_FREE_MEMORY (MEM(MALLOC_STRUCT + 8) - MEM(MALLOC_STRUCT + 0x1C)) // "Total Size" - "Allocated Size"

/* high confidence */
#define DRYOS_ASSERT_HANDLER        0x4004   // debug_assert (0xe086adf4): handler(int *args) = {LR, msg, file, line}
#define DRYOS_ASSERT_HANDLER_3ARG   0x4000   // 3-arg assert (0xe086ae48): handler(msg, file, line)

#define CURRENT_GUI_MODE            (*(int*)0x7920)      // see SetGUIRequestMode, Compared with param 1 before write to 0x8708

/**
// new GUI MODE numering scheme!
 */
#define GUIMODE_PLAY 0x2002
#define GUIMODE_MENU 0x2003
#define GUIMODE_ML_MENU (lv ? 0xC053 : GUIMODE_MENU)
//#define GUIMODE_ML_MENU (RECORDING ? 0 : lv ? 0x68 : GUIMODE_MENU)

#define CANON_SHUTTER_RATING 200000 // Some internet sources say 200K

#define DISPLAY_IS_ON               (*(int *)0x10328)     //search for "DispOperator_PropertyMasterSetDisplayTurnOffOn (%d)"

#define GMT_FUNCTABLE               0xE0DE7E20           //from gui_main_task
#define GMT_NFUNCS                  0x8                  //size of table above

#define LVAE_STRUCT                 0x11789c
#define CONTROL_BV      (*(uint16_t*)(LVAE_STRUCT+0x28)) // via "lvae_setcontrolbv", maybe 0x40?
#define CONTROL_BV_TV   (*(uint16_t*)(LVAE_STRUCT+0x42)) // via "lvae_setcontrolaeparam"
#define CONTROL_BV_AV   (*(uint16_t*)(LVAE_STRUCT+0x44)) // via "lvae_setcontrolaeparam"
#define CONTROL_BV_ISO  (*(uint16_t*)(LVAE_STRUCT+0x46)) // via "lvae_setcontrolaeparam"
#define CONTROL_BV_ZERO (*(uint16_t*)(LVAE_STRUCT+0x48)) // via "lvae_setcontrolaccumh"
#define LVAE_DISP_GAIN  (*(uint16_t*)(LVAE_STRUCT+0x74)) // via "lvae_setdispgain"
#define LVAE_MOV_M_CTRL (*(uint8_t* )(LVAE_STRUCT+0x24)) // via "lvae_setmoviemanualcontrol"

/*
 * kitor: DIGIC 8+ has no PROP_LV_OUTPUT_TYPE (PROP_HOUTPUT_TYPE in ML source)
 * I looked around LiveViewApp and found `LvInfoToggle_Update()` which updates
 * variable to represent currently display overlays:
 *    0x0 - 1st overlays mode in LV (top+bottom)
 *    0x1 - above + sides
 *    0x2 - above + level
 *    0x3 - clean overlays
 *    0x6 - OlcApp? The screen like outside LV on DSLR
 *    0x7 - Nothing? Executes "LvInfoToggle_Update TURNOFF"
 */
#define LV_OVERLAYS_MODE MEM(0x1E9F0)

// kitor: ISO related stuff is not in LVAE struct anymore?
#define LVAE_ISO_STRUCT 0x117854
#define LVAE_ISO_MIN    (*(uint8_t* )LVAE_ISO_STRUCT + 0x0E ) // via string: ISOMin:%d

//#define LVAE_ISO_HIS    (*(uint8_t* )(LVAE_STRUCT+0xXX)) // no idea, not referenced in ./src?!
//#define LVAE_ISO_SPEED  (*(uint8_t* )(LVAE_STRUCT+0xXX))  //WRONG, not sure how to follow

/*
 * DISP_VRAM had a nasty surprise on R5 - vram pointers are over 32 bits.
 * FIO has new functions - ReadIBus and WriteIBus, and those are used in a function
 * that dumps LV. At this moment we have no idea how it works.
 */

#define YUV422_LV_BUFFER_1   0x0
#define YUV422_LV_BUFFER_2   0x0
#define YUV422_LV_BUFFER_3   0x0

//#define DISP_VRAM_STRUCT_PTR *(unsigned int *)0x83e8             // DispVram structure
//#define DV_DISP_TYPE  *((unsigned int *)(DISP_VRAM_STRUCT_PTR + 0xC))   // Display type mask
//#define DV_VRAM_LINE  *((unsigned int *)(DISP_VRAM_STRUCT_PTR + 0xA4))  // Pointer to LV buffer for HDMI output
//#define DV_VRAM_PANEL *((unsigned int *)(DISP_VRAM_STRUCT_PTR + 0xAC))  // Pointer to LV buffer for Panel output
//#define DV_VRAM_EVF   *((unsigned int *)(DISP_VRAM_STRUCT_PTR + 0xB4))  // Pointer to LV buffer for EVF output

/* Hardcoded to Panel for now. It would be easier if we can replace this with a
 * function call that would be put into function_overrides.c. Then we could just
 * define full structs there instead of playing with pointers */
#define YUV422_LV_BUFFER_DISPLAY_ADDR 0x0
#define YUV422_LV_PITCH               1024       // depends on display type

#define YUV422_HD_BUFFER_DMA_ADDR 0x0 // it expects this to be shamem_read(some_DMA_ADDR)

/**
 * DIGIC X IBus, see src/ibus.h.
 */
#define MEMIF_WINDOW_REGS         0xd2044080    // MMIO, 12 fixed memory windows (boot code 0xe000a880)
#define MEMIF_DYNWIN_REGS         0xd20440b0    // dynamic windows 12..15: target, range (8 bytes each), see ibus.h
#define MEMIARB_PID_TABLE         0x6278        // RAM, 253 bytes per DRAM channel, filled by 0xe0956fee
#define MEMIARB_ENABLED           0x620c        // RAM, MemiArbiter check (0xe09225bc) active when != 0
#define EDMAC_DX_CHANNEL_TABLE    0xe1bf6d04    // ROM, 0x14 bytes per EDMAC channel: {base, base2, slot, pid, flags}
#define EDMAC_DX_CHANNEL_COUNT    0x6f
#define XIMR_PID                  0x12          // bus master pid of Ximr (display): DRAM Ch1 / Ch2 only

/**
 * EDMAC mem2mem through Engine::MemoryToMemoryEsub1 (CONFIG_EDMAC_MEMCPY_DX, src/edmac-memcpy-dx.c).
 * Esub1 accepts rd 55..67 and wr 1..18. rd 66 and wr 6 are both in block 0xd243
 * (EDMAC ch 6..18, 66, 67), which ML powers itself (EDMAC_DX_M2M_SUBCHIPS); both are
 * free in movie and photo LiveView, support transfer modes 0..2 and may use DRAM Ch1..3.
 * Measured rd 66 -> wr 6: mode 0 ~650 MB/s, mode 1 ~1.2 GB/s, mode 2 1.75..2.06 GB/s.
 * (Was wr 14: modes 0..1 only, not allowed on DRAM Ch3; mode 2 there asserts.)
 */
#define EDMAC_DX_M2M_RD_CH        66            // pid 0x6d
#define EDMAC_DX_M2M_WR_CH        6             // free in both modes, used by no Canon path seen
#define EDMAC_DX_M2M_SUBCHIPS     { 1, 9, -1 }  // powers block 0xd243, same list as Mem1WritePath
#define EDMAC_DX_M2M_MODE         2
/* ERMGR: candidate is SHTVP_MEMTOMEMESUB1 {0x20042, 0x2004c, 0x90002} (entry 0 of the
 * SHTVP lock table 0xe1361bf8, 31 x 0x50 bytes, created by 0xe01472e8). 0x9000x look like
 * the Esub module IDs (SUB5 -> 0x90003, COPYESUB6 -> 0x90001), so { 0x90002 } alone would
 * exclude Canon's Esub1 users. Not verified which EDMAC channels 0x2004x stand for. */
// #define EDMAC_DX_M2M_RESIDS    { 0x90002 }

/**
 * RAW LiveView (CONFIG_RAW_LIVEVIEW).
 *
 * RAW LV stream: Prima::Mem1WritePath.c writes the Bayer data on EDMAC ch 8
 * (block 0xd243xxxx, slot 3 - the DIGIC 8 equivalent is R ch 3 "WR_M1",
 * 0xd0420200). Setup 0xe035a75a hard-codes ch 8. Every frame, 0xe035a972
 * (via 0xe035aab4) recomputes the write address and reprograms the channel:
 *     state+0x80  u64 IBus base of the frame buffer chunk (alloc 0xe035a8ae,
 *                 chunk handle at +0x34, 64-bit chunk API)
 *     state+0x88  u64 IBus address for this frame = base, or in ring mode
 *                 (+0x44 == 1) base + stride(+0x68) * index(+0x6c)  [0xe035ac1c]
 *     set_address64(8, *(u64 *)(state + 0x88))                       [0xe0845824]
 * Measured (movie LV, 1.4.0): 2064x688 @ 14 bpp, pitch 3612 (xb 0xe1c, yb 0x2af),
 * Bayer, black 2048, buffer IBus 2:10080000 = CPU 0x30080000 (DRAM Ch2),
 * stride 0x25eb40, ring off (+0x44 = 0, +0x20 = 35 frames capacity), address
 * constant across frames.
 * So anything written to the ch 8 registers (raw_lv_redirect_edmac) only lasts
 * until Canon's next per-frame update; +0x88 is recomputed every frame too.
 * The address register holds only the low 32 bits of an IBus address;
 * `region` (0xa0) holds the DRAM channel (0x22 = DRAM Ch2).
 *
 * Enabled with call("imgc_save_raw", 1) - R8's lv_save_raw. There is no
 * lv_set_mm equivalent: Mem1WritePath only carries Bayer data (only .mmr
 * dump naming exists on R8). The tap point is imgc_set_raw_wp (SAP HEAD = 0,
 * default; refused while saving).
 *
 * Pixel packing: LSB-first 14-bit bitstream (4 px in 7 bytes), not ML's
 * struct raw_pixblock layout, see CONFIG_RAW_LE14. Pack unit for ch 8:
 * unit 0xa = 0xd24315c0 (ROM tables 0xe1bf6184: 4 unit indices per channel,
 * 0xe1bf75b0: {reg base, connect reg} per unit), control at +0x34 = 0x21,
 * written once per stream start by 0xe0846810.
 *
 * Bit depth: setup (0xe035a75a) takes the pack mode from +0x54 (12 / 14 ->
 * mode 4 / 5; others but 16 assert) and sets xb = +0x54 * +0x58 / 8. The pack
 * setter writes code << 4 | 1, code 0 / 1 / 2 for 10 / 12 / 14 bpp. Setup
 * runs again only when +0x04 is 0 - not on imgc_save_raw off / on - and on
 * a Mem1 reallocation (LV mode change, 0xe035a802, which resets +0x54 to
 * 14). Measured: writing 0x11 / 0x01 and xb gives clean 12 / 10-bit frames,
 * the same as Canon's own 12-bit setup; LSB-first like 14-bit; the packer
 * keeps the top bits (truncates: clip 15998 -> 3999 / 999).
 */
#define RAW_LV_EDMAC_CHANNEL_ADDR 0xd2430200    // channel 8, EDMAC block 0xd243xxxx, slot 3. Prima::Mem1WritePath.c
#define RAW_LV_EDMAC_CHANNEL_NUM  8
#define RAW_LV_IMGC_STATE         0x16038       // Mem1WritePath state: +0x04 ch 8 set up, +0x28 save_raw, +0x54 bpp, +0x80 u64 buffer base, +0x88 u64 current write addr (IBus)
#define RAW_LV_WIDTH              (*(int *)(RAW_LV_IMGC_STATE + 0x58))   // raw width in pixels, any bit depth (Mem1WritePath: xb = bpp * width / 8)
#define RAW_LV_PACK_CTRL          0xd24315f4    // pack register (PACK32_MODE): unit 0xa (ch 8) +0x34, code << 4 | 1, code 0 / 1 / 2 = 10 / 12 / 14 bpp

/**
 * RscMgr pools (old API), type ID = 4th argument of (Continuous)AllocateMemoryResource.
 * 0x50 (older models) and unknown IDs assert in RscMgrOldApiWrapper.c.
 * Measured on R8 1.4.0 (largest single request, one chunk each):
 *   generic : plain 0, 1, 2, 4, 8, 0xc / cont 0, 4, 6, 9, 0xa, 0xc     47 MB  1:062d6070
 *   0xb     : plain 0xb / cont 0xb                                     47 MB  1:092d6000
 *   big     : plain 0x5, 0x52, 0x56 / cont 0x7, 0x52, 0x53, 0x5a, 0x5b 170 MB  2:11a84000
 *             (blocked while RAW LV holds area 0x40)
 *   0x58    : cont 0x58                                                24 MB  2:1e708080
 *   cont 0x57 / 0x59 return nothing.
 * 0xb and big have no CPU address in RscMgr: see CONFIG_RSCMGR_IBUS (exmem.c).
 */
#define RSCMGR_TYPE_ID            0             // generic pool: shoot_malloc / fio_malloc
#define RSCMGR_POOLS              { { 0x52, 0 }, { 0x0b, 0 }, { 0x58, 1 } }   // shoot_malloc_suite(0), largest first

/*
 * RscMgr WorkMem areas on DRAM Ch3 for exmem_dram3_alloc() (CONFIG_DRAM3_WORKMEM).
 * Measured on R8 1.4.0, movie mode, RAW LV on: all 13 free,
 * 1549 MB in 19 ranges (largest 4 x 220 MB).
 */
#define DRAM3_WORKMEM_AREAS       { 0x4128, 0x4129, 0x412a, 0x412b, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, \
                                    0x37, 0x3a, 0x38, 0x1c }

/* WRONG! */
#define HALFSHUTTER_PRESSED         0

#define NUM_PICSTYLES 10 // guess, but seems to be always 9 for old cams, 10 for new

#define AUDIO_MONITORING_HEADPHONES_CONNECTED 0
#define INFO_BTN_NAME               "INFO"
#define Q_BTN_NAME                  "Q/SET"

#define MIN_MSLEEP 11
#define PLAY_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_GUI_MODE == GUIMODE_PLAY)
#define MENU_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_GUI_MODE == GUIMODE_MENU)

/* WRONG: copied straight from 200d/50d */
// Definitely wrong / hacks / no testing at all:
#define IMGPLAY_ZOOM_LEVEL_ADDR (0x2CBC) //wrong

extern int winsys_bmp_dirty_bit_neg;
#define WINSYS_BMP_DIRTY_BIT_NEG MEM(&winsys_bmp_dirty_bit_neg) // faked via function_overrides.c
#define FOCUS_CONFIRMATION 0 // wrong, but safe, no focus lock

#define LV_BOTTOM_BAR_DISPLAYED 0x0 // wrong, fake bool
// this block all copied from 50D, and probably wrong, though likely safe
#define FASTEST_SHUTTER_SPEED_RAW 160
#define MAX_AE_EV 2
#define FLASH_MAX_EV 3
#define FLASH_MIN_EV -10
#define COLOR_FG_NONLV 80
#define AF_BTN_HALFSHUTTER 0
#define AF_BTN_STAR 2
// another block copied from 50D
#define GUIMODE_WB 5
#define GUIMODE_FOCUS_MODE 9
#define GUIMODE_DRIVE_MODE 8
#define GUIMODE_PICTURE_STYLE 4
#define GUIMODE_Q_UNAVI 0x18
#define GUIMODE_FLASH_AE 0x22
#define GUIMODE_PICQ 6

// all these MVR ones are junk, don't try and record video and they probably don't get used?
#define MVR_190_STRUCT (*(void**)0x1ed8) // look in MVR_Initialize for AllocateMemory call;
                                         // decompile it and see where ret_AllocateMemory is stored.
#define div_maybe(a,b) ((a)/(b))
// see mvrGetBufferUsage, which is not really safe to call => err70
// macros copied from arm-console
#define MVR_BUFFER_USAGE 70 /* obviously wrong, don't try and record video
       // div_maybe(-100*MEM(236 + MVR_190_STRUCT) - \
       // 100*MEM(244 + MVR_190_STRUCT) - 100*MEM(384 + MVR_190_STRUCT) - \
       // 100*MEM(392 + MVR_190_STRUCT) + 100*MEM(240 + MVR_190_STRUCT) + \
       // 100*MEM(248 + MVR_190_STRUCT), \
       // - MEM(236 + MVR_190_STRUCT) - MEM(244 + MVR_190_STRUCT) + \
       // MEM(240 + MVR_190_STRUCT) +  MEM(248 + MVR_190_STRUCT)) */
#define MVR_FRAME_NUMBER (*(int*)(220 + MVR_190_STRUCT))
//#define MVR_LAST_FRAME_SIZE (*(int*)(512 + MVR_752_STRUCT))
#define MVR_BYTES_WRITTEN MEM((212 + MVR_190_STRUCT))

// address of XimrContext structure to redraw in FEATURE_VRAM_RGBA
// XCM + 0x10 is first XC
#define XCM_PTR *(unsigned int *)0x1455D0
#define XIMR_CONTEXT ((unsigned int *)(XCM_PTR + 0x10))
