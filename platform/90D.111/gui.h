#ifndef _cameraspecific_gui_h_
#define _cameraspecific_gui_h_

// button codes as received by gui_main_task
#define BGMT_PRESS_UP 0x40
#define BGMT_UNPRESS_UP 0x41
#define BGMT_PRESS_DOWN 0x42
#define BGMT_UNPRESS_DOWN 0x43
#define BGMT_PRESS_LEFT 0x3e
#define BGMT_UNPRESS_LEFT 0x3f
#define BGMT_PRESS_RIGHT 0x3c
#define BGMT_UNPRESS_RIGHT 0x3d

#define BGMT_PRESS_SET 0xa
#define BGMT_UNPRESS_SET 0xb

#define BGMT_PLAY 0x12
// #define BGMT_UNPRESS_PLAY 0x13
// These are the real Trash codes, but we don't use them.  Instead
// We fake a Trash button press when Q is long pressed, see menu.c
//#define BGMT_TRASH 0xb3 // or 0xb4 // seems to alternate depending on "lock" status, could be a problem
#define BGMT_TRASH 0x70 // fake, so we can simulate it
#define BGMT_MENU 0xc
//#define BGMT_UNPRESS_MENU 0xd
#define BGMT_INFO 0xe
//#define BGMT_UNPRESS_INFO 0xf

#define BGMT_PRESS_DISP 0x10
#define BGMT_UNPRESS_DISP 0x11

#define BGMT_Q 0x2b
#define BGMT_UNPRESS_Q 0x2c

// don't seem to exist:
//#define BGMT_PRESS_UP_RIGHT 0x17
//#define BGMT_PRESS_UP_LEFT 0x18
//#define BGMT_PRESS_DOWN_RIGHT 0x1B
//#define BGMT_PRESS_DOWN_LEFT 0x1C
//#define BGMT_UNPRESS_UDLR 0x15

//~ #define BGMT_Q 0x1E //Compatibility
//~ #define BGMT_Q_ALT 0x15 //Probably don't use 15 is generated by others
#define BGMT_PRESS_HALFSHUTTER 0xa3 // not sure
//#define BGMT_UNPRESS_HALFSHUTTER 0xa4 // FIXME just this one button is defined in another file...
#define BGMT_PRESS_ZOOM_IN 0x4a
#define BGMT_UNPRESS_ZOOM_IN 0x4b
#define BGMT_PRESS_ZOOM_OUT 0x48 // when in Menu or Play, 0x9f on quick access menu
#define BGMT_UNPRESS_ZOOM_OUT 0x49 // When in Menu or Play, 0xa0 on quick access menu
//#define BGMT_PICSTYLE 0x13
//#define BGMT_FUNC 0x12
//#define BGMT_JOY_CENTER 0x0 // wrong, avoiding conflicting definition

#define BGMT_LV 0x2d
//#define BGMT_UNPRESS_LV 0x2e

// wrong - avoiding conflicting definitions
#define BGMT_WHEEL_LEFT 0x8
#define BGMT_WHEEL_RIGHT 0x9
#define BGMT_WHEEL_UP 0x0 // I bet this causes problems in our code with comparisons...
#define BGMT_WHEEL_DOWN 0x1

#define BGMT_FLASH_MOVIE 0x77
#define BGMT_PRESS_FLASH_MOVIE 0x77
#define BGMT_UNPRESS_FLASH_MOVIE 0x77

#define BGMT_ISO_MOVIE 0x77
#define BGMT_PRESS_ISO_MOVIE 0x77
#define BGMT_UNPRESS_ISO_MOVIE 0x77

// search for "copyOlcDataToStorage uiCommand(%d)", condition is event ID
#define GMT_OLC_INFO_CHANGED 0x62

// needed for correct shutdown from powersave modes
// kitor: gui_massive_event_loop decomp looks insane on 200D, had to work those backwards.
// 200D.101 at e02d1f1a does relative jump based on event ID as index in table of offsets at e02d1f1e
// Offsets point to jump instructions that start from e02d2824 to e02d2880
// Some of those jumps point to another table of jumps...
// Decomp everything, you will get xrefs in relevant code.
#define GMT_GUICMD_START_AS_CHECK 0x5A
#define GMT_GUICMD_OPEN_SLOT_COVER 0x56
#define GMT_GUICMD_LOCK_OFF 0x54

#define BTN_ZEBRAS_FOR_PLAYBACK BGMT_FUNC // what button to use for zebras in Play mode
#define BTN_ZEBRAS_FOR_PLAYBACK_NAME "FUNC"

//void GUI_SetLvMode(int);
//void GUI_SetMovieSize_a(int);
//void GUI_SetMovieSize_b(int);

#endif
