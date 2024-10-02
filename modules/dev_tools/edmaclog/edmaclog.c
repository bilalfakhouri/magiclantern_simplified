// EDMAC logging, next gen.
//
// The old logger doesn't log things we want,
// doesn't account for changes with D678X,
// hard-codes addresses which have changed, etc.
// And it's not documented well.
// Just do it all again.

#include "module.h"
#include "dryos.h"
#include "compiler.h"
#include "menu.h"
#include "console.h"
#include "bmp.h"
#include "log.h"
#include "patch_mmu.h"

#include "hooks_thumb.h"

static uint32_t is_log_enabled = 0;
static void log_control_task()
{
    if (is_log_enabled)
    {
        is_log_enabled = 0;
        send_log_data_str("Disabling log.\n");
        disable_logging();
    }
    else
    {
        is_log_enabled = 1;
        enable_logging();
        send_log_data_str("Enabling log.\n");
    }
}
 
static struct menu_entry edmac_menu[] =
{
    {
        .name   = "Toggle EDMAC logging",
        .select = run_in_separate_task,
        .priv   = log_control_task,
        .icon_type = IT_ACTION,
    }
};

static unsigned int edmac_init()
{
    static uint8_t *log_buf = NULL;
    if (log_buf == NULL)
    {
        log_buf = malloc(MIN_LOG_BUF_SIZE);
        if (log_buf == NULL)
            return -1;
    }
    init_log(log_buf, MIN_LOG_BUF_SIZE, "edmac.log");
    extern int uart_printf(const char *fmt, ...);
    uart_printf("pre patch\n");

    if (is_camera("200D", "1.0.1"))
    {
        // install 200D hooks
        struct function_hook_patch f_patches[] = {
            {
                .patch_addr = 0xe04b3738, // CreateResLockEntry
                .orig_content = {0x2d, 0xe9, 0xf0, 0x47, 0x80, 0x46, 0xd8, 0x4d},
                .target_function_addr = (uint32_t)hook_CreateResLockEntry_200D,
                .description = "Log CreateResLockEntry"
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
                return -2;
            }
        }
        apply_patches(patches, COUNT(f_patches));
    }
    else
    {
        bmp_printf(FONT_MED, 20, 20, "No hooks defined for this cam, cannot start logging");
        return CBR_RET_ERROR;
    }
    uart_printf("post patch\n");
    
    menu_add("Debug", edmac_menu, COUNT(edmac_menu));
    return 0;
}

static unsigned int edmac_deinit()
{
    menu_remove("Debug", edmac_menu, COUNT(edmac_menu));
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(edmac_init)
    MODULE_DEINIT(edmac_deinit)
MODULE_INFO_END()
