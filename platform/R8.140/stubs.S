/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.4.0 firmware for the EOS R8
 *
 */
/*
 * Copyright (C) 2022 Magic Lantern Team
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

#include <stub.h>

/* All functions are Thumb. */


/** Startup **/
NSTUB(MAIN_FIRMWARE_ADDR,  firmware_entry)                  // also known as 2nd stage loader, boot2 or DryOS loader
THUMB_FN(0xe0867bd0,  dcache_clean)                         // call from firmware_entry(); in assembly: loop with MCR p15 c7,c10,1; DSB
THUMB_FN(0xe0867ca4,  icache_invalidate)                    // call from firmware_entry(); in assembly: loop with MCR p15 c7,c5,1; c7,c1,6; c7,c1,0; ISB
THUMB_FN(0xe01000f0,  cstart)                               // call from firmware_entry(); calls bzero32 and create_init_task(..., init_task, ...)
THUMB_FN(0xe086aeb8,  bzero32)                              // call from cstart(); zeros out a data structure
THUMB_FN(0xe02b6958,  create_init_task)                     // call from cstart() only on core0; low-level DryOS initialization
THUMB_FN(0xe0100222,  init_task)                            // 2nd param to create_init_task(); xrefs strings like USER_MEM size checking, dmSetup, termDriverInit...

/** Startup - Digic X specials **/
THUMB_FN(0xe0005bd8,  fixup_boot_memory)                    // called from 1st stage loader, just before decision where to jump next.

/** Tasks **/
DATA_PTR(    0x1010,  current_interrupt)                    // DryosDebugMsg() prints "TASK Name:Interrupt" and "Vector:0x%l", argument to 2nd print.
DATA_PTR(    0x1028,  current_task)                         // from task_create(); pointer to the current task structure
DATA_PTR(    0x4488,  task_max)                             // entry in SysConfig struct
THUMB_FN(0xe084b08e,  msleep)                               // argument is always multiple of 10
THUMB_FN(0xe086a71e,  task_create)                          // used to start TaskMain, GuiMainTask etc

/** File I/O **/
// kitor: ordered by ROM address, as FIO block usually holds the same in-ROM order.
THUMB_FN(0xe0850d00, _FIO_OpenFile)                         // Storage::Storage.c Open
THUMB_FN(0xe0850eb8, _FIO_CreateFile)                       // Storage::Storage.c Create
THUMB_FN(0xe08511b6, _FIO_RemoveFile)                       // Storage::Storage.c Remove
THUMB_FN(0xe0851268, _FIO_ReadFile)                         // Storage::Storage.c Read
THUMB_FN(0xe0851656,  FIO_SeekSkipFile)                     // Storage::Storage.c Search
THUMB_FN(0xe085171c, _FIO_WriteFile)                        // Storage::Storage.c Write
THUMB_FN(0xe0852276,  FIO_CloseFile)                        // Storage::Storage.c Close
THUMB_FN(0xe0852528, _FIO_GetFileSize64)                    // Storage::Storage.c AcqSize
THUMB_FN(0xe085302e, _FIO_RenameFile)                       // Storage::Storage.c Rename
THUMB_FN(0xe0853506, _FIO_CreateDirectory)                  // Storage::Storage.c CreateDir
THUMB_FN(0xe08536fa,  FIO_Flush)                            // Storage::Storage.c Flush
THUMB_FN(0xe0853a24, _FIO_FindFirstEx)                      // Storage::Storage.c FirstEnt
THUMB_FN(0xe0853b36,  FIO_FindNextEx)                       // Storage::Storage.c NextEnt
THUMB_FN(0xe0853bf6,  FIO_FindClose)                        // Storage::Storage.c CloseEnt

/** Bitmap **/
//DATA_PTR(    0xF938,  bmp_vram_info)                        // first YUV VRAM buffer, used right after DISP_SetUpdateOSDVram // Bilal: No longer used?!

/** Debug messages **/
THUMB_FN(0xe08690e0,  DryosDebugMsg)                        // Easy to catch in init task: "InitializeIntercom", "CreateSequencer"

/** Eventprocs (call by name) **/
THUMB_FN(0xe08570ac, call);

/** GUI timers **/
THUMB_FN(0xe0a27e88,  SetHPTimerAfterNow)                   // System::HPTimer.c
THUMB_FN(0xe0a27f1a,  SetHPTimerNextTick)                   // System::HPTimer.c; next function; same "worker" as SetHPTimerAfterNow()
THUMB_FN(0xe084fd84,  SetTimerAfter)                        // SystemIF::Timer1m.c
THUMB_FN(0xe084fe3e,  CancelTimer)                          // CancelRefreshTimer, CancelPlayTimer

/** Memory allocation **/
THUMB_FN(0xe08681dc, _AllocateMemory)                       // NG AllocateMemory
THUMB_FN(0xe08683a2, _FreeMemory)                           // uses the same heap
THUMB_FN(0xe0868066,  GetMemoryInformation)                 // called from _AllocateMemory()
THUMB_FN(0xe086803a,  GetSizeOfMaxRegion)                   // called from _AllocateMemory()
THUMB_FN(0xe086a144, _alloc_dma_memory)                     // allocates 64 bytes more, aligns at 32 and sets the uncacheable bit
THUMB_FN(0xe086a14a, _free_dma_memory)                      // paired with the above; clears the uncacheable bit and frees the raw pointer
THUMB_FN(0xe055eb48, _malloc)                               // Just before conditional uart_printf("can't malloc")
THUMB_FN(0xe055eb80, _free)                                 // After malloc() in ROM

/** Memory info **/
THUMB_FN(0xe012317a,  res_memshow)                          // DIGIX X replacement of smemShowFix()
THUMB_FN(0xe03313f8,  memmap_info)                          // Exception vector, DRYOS system memory etc
THUMB_FN(0xe0359978,  malloc_info)                          // Malloc Information */
THUMB_FN(0xe0359a10,  sysmem_info)                          // System Memory Information */

/** Misc **/
THUMB_FN(0xe086ad34,  uart_printf)                          // in init task, prints "%s READY\n"
THUMB_FN(0xe01203d2,  vsnprintf)                            // called by LogFile_printf(..., "*Data Lost*\n", ...); the one called by DebugMsg only knows %s

THUMB_FN(0xe031db3e,  dump_to_file)

/** Startup **/
// 16d0c + 0xd80 -> is this static location, or we have to rewrite for dynamic access?
//DATA_PTR(    0x,  additional_version)                     // via GUI_GetFirmVersion(); Points at '\n' char in char* with FW version

/** Tasks **/
DATA_PTR(    0x44c4,  task_dispatch_hook)
THUMB_FN(0xe011a358,  task_trampoline)
/*
 * kitor: task_create calls internal function (e040fa1c) which set up basic
 * task structure, and part of that is call to prepare_task_trampoline (e011ecd8)
 * (it is followed by check of caller return value).
 * prepare_task_trampoline will pass task_trampoline+1 as parameter to another
 * function.
 */
THUMB_FN(0xe04faf58, _get_task_info_by_id)                  // via extask

/** Recursive locks **/
THUMB_FN(0xe086a07c,  CreateRecursiveLock)                  // SystemIF::KerMisc.c
THUMB_FN(0xe083d900,  AcquireRecursiveLock)                 // SystemIF::KerRLock.c
THUMB_FN(0xe083d9ea,  ReleaseRecursiveLock)                 // SystemIF::KerRLock.c

/** Semaphores **/
THUMB_FN(0xe086aae0,  create_named_semaphore)               // SystemIF::KerSem.c, called from init task with "PU0Wait", "PU1Wait", "CCInit"... as 1st arg
THUMB_FN(0xe086ac8e,  give_semaphore)                       // SystemIF::KerSem.c, follow xrefs to result of any of create_named_semaphore()
THUMB_FN(0xe086ac04,  take_semaphore)                       // SystemIF::KerSem.c, follow xrefs like in give_semaphore()
THUMB_FN(0xe086abcc,  take_semaphore_now)

/** GUI **/
DATA_PTR(    0x4ED8,  gui_main_struct)                      // via string "QueueClearFlg Clear", accessed by that function
DATA_PTR(   0x1897C,  gui_task_list)                        // via WINSYS_CreateController()
THUMB_FN(0xe0473526,  gui_main_task)                        // via task_create("GuiMainTask"...
THUMB_FN(0xe04734c4,  gui_enqueue_message)                  // We have two functions for this on R8? via (CannotPost id 0x%08x param:%d pparam:%), they are different from R5
THUMB_FN(0xe0473460,  putBackMessageEvent)
THUMB_FN(0xe0600c36,  gui_massive_event_loop)               // various "GUICMD_"* strings
THUMB_FN(0xe082706a,  SetGUIRequestMode)                    // by debug message. New style (RP+)
// gui_init_end and GUI_Control DNE. Implemented via function_overrides.

/** Dialog API **/
THUMB_FN(0xe082c864,  dialog_redraw)                        // via xrefs to "pDialog->pSignature == m_pcSignature"
THUMB_FN(0xe081f8e0,  dialog_set_property_str)              // call with "RF-TEST" as 3rd arg

/** GUI timers **/
THUMB_FN(0xe0b34e02,  StartDateTimer)                       // by debug message
THUMB_FN(0xe0b34e92,  CancelDateTimer)                      // by debug message

/** Queue **/
THUMB_FN(0xe083e48c,  msg_queue_create)                     // SystemIF::KerQueue.c; used in UiOpe_Initialize() (function that creates gui_main_task)
THUMB_FN(0xe083e56c,  msg_queue_receive)                    // SystemIF::KerQueue.c; used in gui_main_task()
THUMB_FN(0xe083e6fa,  msg_queue_post)                       // SystemIF::KerQueue.c; used in gui_enqueue_message()
THUMB_FN(0xe083e738,  msg_queue_count)                      // via string "warning! QueueLength"

/** Memory **/
THUMB_FN(0xe0a3c888,  AllocateMemoryResource)               // missing debug string on R5, matched via xrefs to R
THUMB_FN(0xe0a3c8ba,  AllocateContinuousMemoryResource)     // missing debug string on R5, matched via xrefs to R
THUMB_FN(0xe0a3c8ec, FreeMemoryResource)                    // missing debug string on R5, matched via xrefs to R
//THUMB_FN(0xe0849cbc,  ConcatMemorySuite)                  // Via debug msg with fn name (after call). Not used by ML, but utilizes functions below:
THUMB_FN(0xe0849300,  GetFirstChunkFromSuite)               // In ConcatMemorySuite(), First function called.
THUMB_FN(0xe0849be6,  GetNextMemoryChunk)                   // In ConcatMemorySuite(), function called in first loop with param2 passed as 1st arg
THUMB_FN(0xe0849076,  GetMemoryAddressOfMemoryChunk)        // In ConcatMemorySuite(), uses param2 from GetNextMemoryChunk() as own param1
THUMB_FN(0xe0848cd0,  CreateMemoryChunk)                    // In ConcatMemorySuite(), just after calls to GetMemoryAddressOfMemoryChunk(); also called from CreateMemorySuite()
THUMB_FN(0xe0849270,  AddMemoryChunk)                       // In ConcatMemorySuite(), just after calls to CreateMemoryChunk()
THUMB_FN(0xe08497a8,  CreateMemorySuite)                    // One of just a few xrefs of CreateMemoryChunk(); String "PackMem.c", calls malloc(0x18)
THUMB_FN(0xe08494cc,  DeleteMemorySuite)                    // DebugMsg "DeleteMemorySuite 0x%x"
// kitor: disabled using CONFIG_MEMORY_SRM_NOT_WORKING
// DIGIC X does not use RscMgr anymore, those are dummy functions that just assert!
//THUMB_FN(0xe081baa0,  SRM_AllocateMemoryResourceFor1stJob) // R5
//THUMB_FN(0xe081baaa,  SRM_FreeMemoryResourceFor1stJob) // R5

/** Uncategorized **/
// From function that references "SD_DeviceCreate:"; looks like now it is a part of dynamic struct
DATA_PTR(       0x0,  sd_device)                            // should not matter unless "keep after format" feature is enabled
THUMB_FN(0xe0bb6b10,  _LoadCalendarFromRTC)                 // Used in SetStartMovieRecDate();
THUMB_FN(0xe03f4ee4,  ErrForCamera_handler)                 // via debug msg: "DlgErrForCamera.c TERMINATE_WINSYS"

/** Properties **/
THUMB_FN(0xe0859d08, _prop_cleanup)                         // missing debug string on R5, matched via xrefs to R
THUMB_FN(0xe0859710,  prop_register_slave)                  // missing debug string on R5, matched via xrefs to R
THUMB_FN(0xe08597e6, _prop_request_change)                  // missing debug string on R5, matched via xrefs to R


/** General FEATURE_VRAM_RGBA stubs **/
//THUMB_FN(0xe0289f1c, VMIX_TransferRectangleToVram)        // via debug message; not needed - left for references below
//THUMB_FN(0xe029cdea, RENDERER_InitializeScreen)           // via debug message; not needed - left for references below
DATA_PTR(   0x18ae8, winsys_sem);                           // Used in VMIX_TransferRectangleToVram() as first arg of take_semaphore()
DATA_PTR(   0x18ab0, _rgb_vram_info);                       // in RENDERER_InitializeScreen(), written just after "JediWrapperClearSurface vram:%d" message
THUMB_FN(0xe01ac174, XimrExe);                              // In VMIX_TransferRectangleToVram(), just before "XimrExe" debug message

/**
 * Things needed for CONFIG_COMPOSITOR_XCM.
 *
 * R5 uses two layers (GUI, focus overlays). WINSYS code swaps pointer in
 * WINSYS structure (one we know as _rgb_vram_info) to select which layer to
 * draw.
 *
 * With FEATURE_COMPOSITOR_XCM we ask XCM via XCM_GetSourceSurface() to give us
 * pointer to layer 0, which is more reliable.
 */
THUMB_FN(0xe0187670, XCM_GetSourceSurface)                  // by debug message
//THUMB_FN(0xe01871ec, XCM_MakeContext)                     // by debug message. Not used in code, left for reference below.
DATA_PTR(  0x1455d0, _pXCM);                                // param1 to XCM_MakeContext() when called from VMIX_TransferRectangleToVram()

/** Wrong on purpose **/
DATA_PTR(       0x0,  LCD_Palette)

/** App handlers **/
/**
 * Those can usually be found by looking at function address passed to
 * WINSYS_CreateDialogBox(), usually after a debug message.
 */
THUMB_FN(0xe0807e36,  PlayMain_handler)                     // via debug string "PlayMain_StartPlayMain"
THUMB_FN(0xe09d7d0e,  ShootOlcApp_handler)                  // via debug string "StartShootOlcApp PushPalette(%d)"
THUMB_FN(0xe064a8b6,  LiveViewApp_handler)                  // via debug string "StartLiveViewApp"
//THUMB_FN(0xe0aefa4c,  LiveViewShutterApp_handler)         // via debug string "StartLiveViewShutterApp"
THUMB_FN(0xe9999999,  PlayMovieGuideApp_handler)            // PlayMovieGuideApp DNE on R5, used for comparison so it is safe-ish.

/**
 * Code that creates / stops given task from above, also saves dialog pointer
 * somewhere. This seems to be a better way to detect if given app is running.
 *
 * Pointers to result of WINSYS_CreateDialogBox() call in given function.
 */
//DATA_PTR( 0x1719c,  PlayMain_dialog)                      // via debug string "PlayMain_StartPlayMain" // R5
//DATA_PTR( 0x22770,  ShootOlcApp_dialog)                   // via debug string "StartShootOlcApp PushPalette(%d)" // R5

/* LiveViewApp_dialog is not a dialog on R8:

redraw() just queues MLEV_REDRAW. _redraw_do() then runs on this DIGIC 6/7/8/X path:
if (LiveViewApp_dialog) dialog_redraw(LiveViewApp_dialog);

-That path is taken by the NotifyBox timeout and by closing the menu.
-The R8 stub is 0x153B4. StartLiveViewApp (0xE064AB42 onwards) stores its result there from 0xE0AECBDA, which mallocs a 32-byte node and links it into gui_task_list. So that value is a struct gui_task *, whose priv is 0.
-On R5 the same slot holds a real dialog_create() result.
-R8’s slot also uses a different convention: 0xE0647414 treats bit 0 set as “not active” instead of NULL.
-R8’s dialog_redraw (0xE082C864) checks *(u32*)dialog == MEM(0x18A04+4). A gui_task fails that check and trips the assert at WindowSystem::Dialog.c line 0x29B. An odd sentinel value would fault outright. */

DATA_PTR(   0x153b4,  LiveViewApp_dialog)                   // via debug string "StartLiveViewApp(%#x)" // This is not real LiveViewApp_dialog

//DATA_PTR( 0x21c60,  LiveViewShutterApp_dialog)            // via debug string "StartLiveViewShutterApp" // R5
//DATA_PTR(      0x,  PlayMovieGuideApp_dialog)             // PlayMovieGuideApp DNE on R5 // R5

/**
 * UNUSED but referenced elsewhere
 **/
//THUMB_FN(0xe07d6458, WINSYS_CreateDialogBox)              // Equiv. of `CreateDialogBox()`. Forces layer ID to 0 (GUI) // R5
//THUMB_FN(0xe07d6204, WINSYS_CreateDialogBoxEx)            // Actual logic. Takes one more argument than CreateDialogBox() (layer ID) // R5
