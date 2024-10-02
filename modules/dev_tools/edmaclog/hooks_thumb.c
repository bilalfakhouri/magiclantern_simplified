#include "hooks_thumb.h"
#include "dryos.h"
#include "log.h"

// EDMAC logging, around device IDs.  Trying to work out
// how / if these correlate with channels, and what different
// device IDs might mean.
void __attribute__((noreturn,noinline,naked,aligned(4)))hook_connect_read(void)
{
    asm(
        "push { r0-r11, lr }\n"
    );

    uint32_t channel, device_ID, lr;
    asm __volatile__ (
        "mov %0, r0\n"
        "mov %1, r1\n"
        "mov %2, lr\n" : "=&r"(channel), "=&r"(device_ID), "=&r"(lr)
    );

    // make space for snprintf
    asm(
        "sub    sp, #0xc\n"
    );

    char *log_buf = malloc(60);
    snprintf(log_buf, 60, "%d 0x%x ConR chan, dev: 0x%x, 0x%x\n",
             get_ms_clock(), lr, channel, device_ID);
    uint32_t log_len = strlen(log_buf);
    send_log_data((uint8_t *)log_buf, log_len);
    free(log_buf);

    asm(
        "add    sp, #0xc\n"
    );

    asm(
        "pop { r0-r11, lr }\n"

        // do overwritten instructions
        "push {r4,r5,r6,lr}\n"
        "cmp  r0, #0x35\n"
        "mov  r5, r1\n"
        "mov  r4, r0\n"

        // jump back
        "ldr pc, =0x364f9\n"
    );
}

void __attribute__((noreturn,noinline,naked,aligned(4)))hook_connect_write(void)
{
    asm(
        "push { r0-r11, lr }\n"
    );

    uint32_t channel, device_ID, lr;
    asm __volatile__ (
        "mov %0, r0\n"
        "mov %1, r1\n"
        "mov %2, lr\n" : "=&r"(channel), "=&r"(device_ID), "=&r"(lr)
    );

    // make space for snprintf
    asm(
        "sub    sp, #0xc\n"
    );

    char *log_buf = malloc(60);
    snprintf(log_buf, 60, "%d  0x%x ConW chan, dev: 0x%x, 0x%x\n",
             get_ms_clock(), lr, channel, device_ID);
    uint32_t log_len = strlen(log_buf);
    send_log_data((uint8_t *)log_buf, log_len);
    free(log_buf);

    asm(
        "add    sp, #0xc\n"
    );

    asm(
        "pop { r0-r11, lr }\n"

        // do overwritten instructions
        "push {r4,r5,r6,lr}\n"
        "cmp  r0, #0x35\n"
        "mov  r5, r1\n"
        "mov  r4, r0\n"

        // jump back
        "ldr pc, =0x364a5\n"
    );
}

void __attribute__((noreturn,noinline,naked,aligned(4)))hook_set_addr(void)
{
    asm(
        "push { r0-r11, lr }\n"
    );

    uint32_t channel, addr, lr;
    asm __volatile__ (
        "mov %0, r0\n"
        "mov %1, r1\n"
        "mov %2, lr\n" : "=&r"(channel), "=&r"(addr), "=&r"(lr)
    );

    // make space for snprintf
    asm(
        "sub    sp, #0xc\n"
    );

    char *log_buf = malloc(60);
    snprintf(log_buf, 60, "%d   0x%x SetA chan, addr: 0x%x, 0x%x\n",
             get_ms_clock(), lr, channel, addr);
    uint32_t log_len = strlen(log_buf);
    send_log_data((uint8_t *)log_buf, log_len);
    free(log_buf);

    asm(
        "add    sp, #0xc\n"
    );

    asm(
        "pop { r0-r11, lr }\n"

        // do overwritten instructions
        "ldr   r2, =0x66264\n"
        "bic   r1, r1, #0xc0000000\n"
        "ldr.w r3, [r2, r0, lsl #0x3]\n"

        // jump back
        "ldr pc, =0x35491\n"
    );
}

void __attribute__((noreturn,noinline,naked,aligned(4)))hook_CreateResLockEntry_200D(void)
{
    asm(
        "push { r0-r11, lr }\n"
    );

    uint32_t pResources, res_count, lr;
    asm __volatile__ (
        "mov %0, r0\n"
        "mov %1, r1\n"
        "mov %2, lr\n" : "=&r"(pResources), "=&r"(res_count), "=&r"(lr)
    );

    // make space for snprintf and log_buf
    uint32_t log_buf_size = 80;
    uint32_t stack_space = 0xc + log_buf_size;
    asm(
        "sub    sp, %0\n" : : "r"(stack_space)
    );

    char log_buf[log_buf_size];
    snprintf(log_buf, log_buf_size, "CreateResLockEntry, LR: %x, time: %d\n",
             lr, get_ms_clock());
    uint32_t log_len = strlen(log_buf);
    send_log_data((uint8_t *)log_buf, log_len);
    for (uint32_t i = 0; i < res_count; i++)
    {
        snprintf(log_buf, log_buf_size, "    0x%x\n", *((uint32_t *)pResources + i));
        log_len = strlen(log_buf);
        send_log_data((uint8_t *)log_buf, log_len);
    }

    asm(
        "add    sp, %0\n" : : "r"(stack_space)
    );

    asm(
        "pop { r0-r11, lr }\n"

        // do overwritten instructions
        "push {r4,r5,r6,r7,r8,r9,r10,lr}\n"
        "mov  r8, r0\n"
        "ldr  r5, =0x10990\n"

        // jump back
        "ldr pc, =0xe04b3741\n"
    );
}

