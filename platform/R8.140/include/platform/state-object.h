#ifndef __platform_state_object_h
#define __platform_state_object_h

/*
 * R8 (DIGIC X Prima) has no EVF state object: "EvfState" and "EvfDevState" are not
 * in the ROM, and the evfSetParamInterrupt / evfReadOutDoneInterrupt actions of
 * older models are gone. The old EVF_STATE (0xa600, copied from R5) does not
 * point to a state object on R8. LiveView sensor timing is handled by
 * Prima::SensorDriverState instead.
 *
 * SensorDriverState (28 inputs, 54 states, matrix 0xe137d1d0) is stored in the
 * global at 0x15ff0. On every sensor frame, in all LiveView modes we measured,
 * it goes through:
 *
 *     (state 7, input 8) -> (5, 9) -> (5, 13) -> (5, 14) -> (5, 10) -> (6, 11)
 *
 *   (5, 9)  srdvLvCvobVD (named in the ROM): LV vertical drive.
 *   (5, 10) arrives a fixed time after (5, 9): 13.9 ms at 24/25/30p, 12.2 ms at
 *           50/60p, 3.1 ms at 120p, 2.6 ms at 180p, i.e. the sensor readout
 *           time. Taken as "LV readout done" (evfReadOutDoneInterrupt on older
 *           models). Inferred from the timing, the ROM does not name it.
 *
 * Frame period measured for all of these: 41708 us (23.976p), 40000 (25p),
 * 33366 (30p), 20000 (50p), 16683 (60p), 8341 (120p), 5553 (180p).
 *
 * Do not use the display side objects (DELIVER, POSTDEV) for sync: DELIVER only
 * fires every 2nd frame at 24/25p and every 1.25 frames at 50p, POSTDEV takes a
 * different path 1 frame in 5 at 60p.
 *
 * The object itself is allocated at boot and moves between runs: always
 * dereference the global, never cache the pointer.
 */
#define SENSOR_DRIVER_STATE (*(struct state_object **)0x15ff0)

/* EVF_STATE is intentionally not defined on R8 (see above). All remaining users
 * are guarded by #ifdef EVF_STATE and are irrelevant on R8:
 *   propvalues.c      inside #ifndef LV_OVERLAYS_MODE, which R8 defines
 *   edmac-memcpy.c    needs CONFIG_EDMAC_RAW_SLURP, not defined on R8
 *   raw.c             optional BLACK_LEVEL hint only */

#endif // __platform_state_object_h
