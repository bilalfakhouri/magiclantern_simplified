#ifndef _platform_lens_h
#define _platform_lens_h

/*
 * Those are quite huge, they size depend on camera.
 * So far we have data from following models:
 *             M50   SX740    R     RP    SX70   250D   850D   R5/R6   R8
 * STATIC     0x138  0x178  0x184  0x184  0x180  0x180  0x1C8  0x1C8   0x1DC
 * DYNAMIC    0x84   0x8C   0x90   0x90   0x90   0x8C   0x90   0x94    0xA8
 * DryOS ICU   P2     P3     P4     P4     P4     P5     P8     P9      P2 (release #0061)
*/

/* EOS R8 1.4.0 lens property structures.
 *
 * Verified against the R8.140 ROM (not copied from R5/R6 any more).
 * Evidence (all addresses are R8.140, ROM loaded at 0xE0000000):
 *  - InitializeLensProp (0xE02B1B4E) registers PROP_LENS_STATIC_DATA  (0x8003009A)
 *    with a 0x1DC byte buffer @ 0x11EFC4 and PROP_LENS_DYNAMIC_DATA (0x80020037)
 *    with a 0xA8 byte buffer @ 0x11F1A0. The dynamic data is also snapshotted
 *    with memcpy(..., 0xA8) in the diff printer (0xE02AF2xx..0xE02AFE78).
 *  - Static fields: evshell "readid" (0xE02AE5FC) and the full static data
 *    printer (0xE02AC8A4..0xE02AD540), which prints every field by name.
 *  - Dynamic fields: change-detect/print function (0xE02AF2xx..0xE02AFE78),
 *    cross-checked with the ShootingInfoEx:LensDynamicInfo print (0xE0591A6C),
 *    where the embedded copy starts at avef (ShootingInfoEx + 0x4C relative to
 *    that function's base register) and every field lands on the same offset.
 *
 * Differences against R5/R6 (the old file):
 *  - STATIC  is 0x1DC bytes (was 0x1C8), DYNAMIC is 0xA8 bytes (was 0x94).
 *  - Static: lens_dev_ver[3] inserted at 0x56 (everything up to lens_name moves
 *    by +3), a few new bytes in the 0xE7..0xFA block, tail fields moved by +0xA/+0xC,
 *    new decimal_focal_length_* fields. aberationControl does not exist on R8
 *    (ABERRATION_VER is a bit field inside byte 0x09 of the LENSID block).
 *  - Dynamic: jsstep, IDC are gone, the po* block starts at 0x22 (was 0x2C),
 *    FL is at 0x44 (was 0x4E) and a new u16 sits after abs_near. New fields:
 *    menuEn, truePfOn, fZmMv, decimal_focal_length, lensTemprature,
 *    limit_sw_state, fcRingFunc, LensReqAfMf and three 32-bit float values.
 *
 * Fields marked "(not printed)" have a name inherited from other models and
 * were not seen in an R8 print; their offset follows from neighbours that
 * were verified. Padding is left explicit so offsets stay greppable.
 *
 * Structs are packed (the ICU copies them byte-wise), so there is no alignment.
 */
struct prop_lens_static_data
{
        uint8_t                 attached;                      // 0x000 (static[0] is tested as 0/1 in LensApi_SendPropAUMIdentify)
        uint8_t                 type;                          // 0x001 90 EF, 91 RF (LENSID1 in readid)
        uint8_t                 LENSID[38];                    // 0x002 LENSID
        uint16_t                lens_id;                       // 0x028
        uint16_t                lens_id_ext;                   // 0x02A lens_id_ex
        uint16_t                fl_wide;                       // 0x02C
        uint16_t                fl_tele;                       // 0x02E
        uint8_t                 lens_serial[5];                // 0x030
        uint8_t                _unk_02[24];                    // 0x035 not printed
        uint8_t                 extender_id[6];                // 0x04D
        uint8_t                 lens_firm_ver[3];              // 0x053
        uint8_t                 lens_dev_ver[3];               // 0x056 new vs R5
        uint8_t                 field_vision;                  // 0x059
        uint8_t                 lens_type;                     // 0x05A
        uint8_t                 extenderMountInfo;             // 0x05B
        uint8_t                 lens_name_len;                 // 0x05C
        char                    lens_name[73];                 // 0x05D
        uint8_t                _unk_03[65];                    // 0x0A6 not printed
        uint8_t                 mount_size;                    // 0x0E7
        uint8_t                 lens_switch_exists;            // 0x0E8
        uint8_t                 lens_af_func_exists;           // 0x0E9
        uint8_t                 lens_mf_funct_exists;          // 0x0EA
        uint8_t                 lens_is_switch_exists;         // 0x0EB
        uint8_t                 lens_is_funct_exists;          // 0x0EC
        uint8_t                 af_speed_setting_available;    // 0x0ED
        uint8_t                 lens_focus_control_ring_sw;    // 0x0EE new
        uint8_t                 dafLimitFno;                   // 0x0EF
        uint8_t                 lens_is_fc_share_ring;         // 0x0F0 new
        uint8_t                 lens_fc_sw_type;               // 0x0F1 new
        uint8_t                 distortionCorrectionInfo;      // 0x0F2 ("distortionCorrectInfo")
        uint8_t                 focusBreathingCorrect;         // 0x0F3 new
        uint8_t                 bcfInfo;                       // 0x0F4
        uint8_t                 lens_id_1292;                  // 0x0F5 ("lens_id_l292")
        uint8_t                 emd_hot_limit;                 // 0x0F6
        uint8_t                 setDistortion;                 // 0x0F7 new
        uint8_t                 dispDepth;                     // 0x0F8 new
        uint8_t                 vrlens_info;                   // 0x0F9 new
        uint16_t                zoom_pos_size;                 // 0x0FA
        uint16_t                focus_pos_size;                // 0x0FC
        uint16_t                fine_focus_size;               // 0x0FE
        uint16_t                av_stop_div;                   // 0x100
        uint8_t                 av_slow_enable;                // 0x102
        uint8_t                 av_dlp_lens;                   // 0x103 ("av_d1p_lens")
        uint16_t                av_dlp_max_spd;                // 0x104
        uint16_t                av_dlp_silent_spd;             // 0x106
        uint16_t                av_dlp_min_spd;                // 0x108
        uint8_t                 av_depth_sim_enable;           // 0x10A new
        uint8_t                _unk_04a;                       // 0x10B not printed
        uint8_t                 dispConfig;                    // 0x10C b4:sink b5:focusNGMacro b6:switchMacro b7:limitSW
        uint8_t                _unk_04b[149];                  // 0x10D not printed
        uint8_t                 extendMagnificationVal;        // 0x1A2
        uint8_t                _unk_05;                        // 0x1A3
        uint16_t                ois_shift_max;                 // 0x1A4
        uint8_t                 colorBalance;                  // 0x1A6
        uint8_t                 pza_exists;                    // 0x1A7
        uint8_t                 pza_id[5];                     // 0x1A8
        uint8_t                 pza_firm_ver[3];               // 0x1AD
        uint8_t                 pza_firmup;                    // 0x1B0
        uint8_t                 dlAdp_count;                   // 0x1B1
        uint8_t                _pad_dl[2];                     // 0x1B2 not printed
        uint32_t                dlAdp1_id;                     // 0x1B4
        uint8_t                 dlAdp1_func1;                  // 0x1B8
        uint8_t                 dlAdp1_firm_ver[3];            // 0x1B9
        uint32_t                dlAdp2_id;                     // 0x1BC
        uint8_t                 dlAdp2_func1;                  // 0x1C0
        uint8_t                 dlAdp2_firm_ver[3];            // 0x1C1
        uint8_t                _unk_06;                        // 0x1C4
        uint8_t                 demandWarnDispFromLens;        // 0x1C5
        uint8_t                 demandWarnDispFromAdp;         // 0x1C6
        uint8_t                _unk_07[3];                     // 0x1C7
        uint16_t                decimal_focal_length_tele;     // 0x1CA new
        uint16_t                decimal_focal_length_wide;     // 0x1CC new
        uint8_t                _unk_08[14];                    // 0x1CE
};

SIZE_CHECK_STRUCT( prop_lens_static_data, 0x1DC );

/*
 * PROP_LENS_DYNAMIC_DATA structure (R8.140, 0xA8 bytes).
 *
 * Names follow the debug prints of the ICU (see the header comment above).
 * st1..st5 are printed together as "st1:0x%x st2:... st5:0x%x" (0x5F..0x63);
 * the AF/MF (st2 & 0x80) and IS (st3 & 0x0F) meanings are inherited from
 * other models and were not re-verified on R8 ("AfMf" print uses byte 0x60 >> 7).
 */
struct prop_lens_dynamic_data {
        uint16_t                AVEF;             // 0x00 ShootingInfoEx: avef
        uint16_t                AVO;              // 0x02 ShootingInfoEx: avo
        uint16_t                AVMAX;            // 0x04 ShootingInfoEx: avmax
        uint16_t                AVD;              // 0x06
        uint16_t                NowAvRF;          // 0x08
        uint16_t                NowAvEF;          // 0x0A
        uint8_t                _pad_01[22];       // 0x0C not printed
        uint16_t                po;               // 0x22
        uint16_t                po05;             // 0x24
        uint16_t                po10;             // 0x26
        uint16_t                po00;             // 0x28
        uint16_t                po15;             // 0x2A
        uint16_t                Npo;              // 0x2C
        uint16_t                po0;              // 0x2E
        uint16_t                po25;             // 0x30
        uint16_t                po50;             // 0x32
        uint16_t                po75;             // 0x34
        uint16_t                po100;            // 0x36
        uint16_t                po125;            // 0x38
        uint16_t                po150;            // 0x3A
        uint16_t                po175;            // 0x3C
        uint16_t                po200;            // 0x3E
        uint8_t                 v0;               // 0x40 vignetting
        uint8_t                 v1;               // 0x41
        uint8_t                 v2;               // 0x42
        uint8_t                 v3;               // 0x43
        uint16_t                FL;               // 0x44 "[FL]%d", "FL:0x%x"
        uint16_t                focal_len_image;  // 0x46 ShootingInfoEx: focal_length_image
        uint16_t                focus_far;        // 0x48 ShootingInfoEx: abs_inf  "[AbsInf(cm)]"
        uint16_t                focus_near;       // 0x4A ShootingInfoEx: abs_near "[AbsNear(cm)]"
        uint16_t                focus_near_2;     // 0x4C printed as "(%d)" after AbsNear, meaning unknown
        uint16_t                zoomPos;          // 0x4E ShootingInfoEx: zoom_pos
        uint16_t                focusPos;         // 0x50
        uint16_t                fineFocusPos;     // 0x52 ShootingInfoEx: fine_focus_pos
        uint16_t                HighResoZoomPos;  // 0x54 ShootingInfoEx: high_res_zoom_pos
        uint16_t                HighResoFocusPos; // 0x56 ShootingInfoEx: high_res_focus_pos
        uint8_t                _pad_02[6];        // 0x58 not printed
        int8_t                  abstat;           // 0x5E "[abstat]%d"
        uint8_t                 st1;              // 0x5F
        uint8_t                 st2;              // 0x60 & 0x80 -> MF ("AfMf" print)
        uint8_t                 st3;              // 0x61 & 0x0F ~ PROP_LV_LENS_STABILIZE (not re-verified)
        uint8_t                 st4;              // 0x62
        uint8_t                 st5;              // 0x63
        uint8_t                _pad_03;           // 0x64
        uint8_t                 menuEn;           // 0x65 "focusmode_menu" / "menuEn"
        uint8_t                _pad_04[3];        // 0x66
        uint8_t                 truePfOn;         // 0x69
        uint8_t                 FcsSt1;           // 0x6A FocusStep?
        uint8_t                 FcsSt2;           // 0x6B
        uint8_t                 FcsSt3;           // 0x6C
        uint8_t                 FcsSt4;           // 0x6D
        uint8_t                 ZmSt1;            // 0x6E ZoomStep?
        uint8_t                 ZmSt2;            // 0x6F
        uint8_t                 ZmSt3;            // 0x70
        uint8_t                 ZmSt4;            // 0x71
        uint8_t                 fZmMv;            // 0x72 "zmPos:0x%x FL:0x%x fZmMv:0x%x"
        uint8_t                _pad_05[5];        // 0x73
        int16_t                 ts_shift;         // 0x78 ShootingInfoEx: ts_shift
        int8_t                  ts_tilt;          // 0x7A ShootingInfoEx: ts_tilt
        int8_t                  ts_all_revo;      // 0x7B ShootingInfoEx: ts_all_revo
        int8_t                  ts_ts_revo;       // 0x7C ShootingInfoEx: ts_ts_revo
        uint8_t                _pad_06[5];        // 0x7D
        uint16_t                decimal_focal_length; // 0x82 [0.1mm]
        int8_t                  lensTemprature;   // 0x84
        uint8_t                 limit_sw_state;   // 0x85 ShootingInfoEx: limit_sw_state
        uint8_t                 fcRingFunc;       // 0x86 "AfMf:%d fcRingFunc:%d menuEn:%d LensReqAfMf:%d"
        uint8_t                 LensReqAfMf;      // 0x87
        uint8_t                _pad_07[4];        // 0x88
        uint32_t                LENSEr;           // 0x8C 32-bit load in the print (was u8 on R5/R6)
        uint8_t                _pad_08[8];        // 0x90
        uint32_t                fRealFL;          // 0x98 IEEE float bits
        uint32_t                fFrontOptPos;     // 0x9C float
        uint32_t                fHighResoDisCinema; // 0xA0 float
        uint8_t                _pad_09[4];        // 0xA4
};

SIZE_CHECK_STRUCT( prop_lens_dynamic_data, 0xA8 );

#endif
