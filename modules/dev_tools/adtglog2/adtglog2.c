/*
"ADTG" is a special Analog Devices part that we assume
is a Timing Generator (and maybe some other stuff). See:
https://magiclantern.fandom.com/wiki/ADTG
Modifying timing can do things such as changing the
sensor sample area (a longer time while reading cols
leads to more being read, etc).

"CMOS" is somehow related, a string found in ADTG contexts
in ROMs.  Possibly the naming is preserved from when the cam
sensors were CMOS.  Certainly some parts of "CMOS" in the
forum discussions of ADTG relate to modifying handling of the sensor,
for example, controlling ISO when reading the sensor.

The ADTG part has its own internal control registers.  These
are the primary things we're interested in logging.  The Digic
layer sends messages to the ADTG part to control it.  We parse
and log these.
*/

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>

#include "log.h"
#include "patch_mmu.h"
#include "hooks_thumb.h"

static unsigned int init()
{
    static uint8_t *log_buf = NULL;
    log_buf = malloc(MIN_LOG_BUF_SIZE);
    if (log_buf == NULL)
        return -1;

    if (init_log(log_buf, MIN_LOG_BUF_SIZE, "adtg.log") < 0)
        return -2;

    if (is_camera("200D", "1.0.1"))
    {
        // install hooks
        struct function_hook_patch f_patches[] = {
            {
                .patch_addr = 0xe034256e, // CMOS_write
                .orig_content = {0x2d, 0xe9, 0xf8, 0x4f, 0x04, 0x46, 0x86, 0x4d},
                .target_function_addr = (uint32_t)hook_CMOS_write_200D,
                .description = "Log ADTG CMOS writes"
            },
        };

        struct patch patches[COUNT(f_patches)] = {};
        uint8_t code_hooks[8 * COUNT(f_patches)] = {};

        for (int i = 0; i < COUNT(f_patches); i++)
        {
            if (convert_f_patch_to_patch(&f_patches[i],
                                         &patches[i],
                                         &code_hooks[8 * i]))
            {
                return 1;
            }
        }
        apply_patches(patches, COUNT(f_patches));
    }

    return 0;
}

static unsigned int deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(init)
    MODULE_DEINIT(deinit)
MODULE_INFO_END()
