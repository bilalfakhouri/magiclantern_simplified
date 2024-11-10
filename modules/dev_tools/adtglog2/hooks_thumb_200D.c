#include "hooks_thumb.h"
#include "dryos.h"
#include "log.h"

// ADTG logging

// Max size of CMOS command buffer.
// (I have no idea what the observed max is, will refine this with experiments)
// Old code suggests DryOS fills this with 16-bit command words,
// and uses 0xffff as an end marker.
#define MAX_CMOS_BUF 512
#define CMOS_END_MARKER 0xffff

void __attribute__((noreturn,noinline,naked,aligned(4)))hook_CMOS_write_200D(void)
{
    asm(
        "push { r0-r11, lr }\n"
    );

    uint16_t *cmos_buf;
    uint32_t lr;
    asm __volatile__ (
        "mov %0, r0\n"
        "mov %1, lr\n" : "=&r"(cmos_buf), "=&r"(lr)
    );

    // make space for snprintf and log_buf
    uint32_t log_buf_size = 80;
    uint32_t stack_space = 0xc + log_buf_size;
    asm(
        "sub    sp, %0\n" : : "r"(stack_space)
    );

    char log_buf[log_buf_size];
    snprintf(log_buf, log_buf_size, "CMOS_write, time: %d,  LR: %x\n\tdata: ",
             get_ms_clock(), lr);
    send_log_data_str(log_buf);

    size_t cmos_i = 0;
    size_t log_buf_i = 0;
    while (cmos_i < MAX_CMOS_BUF)
    {
        // TODO add nzri decoding when appropriate
        int err;

        if (log_buf_i > log_buf_size - 28)
        { // when space in stack buffer gets low,
          // flush and reset
            send_log_data_str(log_buf);
            log_buf_i = 0;
        }
        err = snprintf(log_buf + log_buf_i,
                       log_buf_size - log_buf_i,
                       "0x%04x ", cmos_buf[cmos_i]);
        if (err < 0)
            break;
        log_buf_i += err;

        if ((cmos_i % 8) == 7)
        {
            err = snprintf(log_buf + log_buf_i,
                           log_buf_size - log_buf_i,
                           "\n\t      ");
            if (err < 0)
                break;
            log_buf_i += err;
        }

        if (cmos_buf[cmos_i] == CMOS_END_MARKER)
            break;

        cmos_i++;
    }
    snprintf(log_buf + log_buf_i, log_buf_size - log_buf_i, "\n");
    send_log_data_str(log_buf);

    if (cmos_i >= MAX_CMOS_BUF)
    {
        snprintf(log_buf, log_buf_size, "WARNING, end of CMOS command buf not found\n");
        send_log_data_str(log_buf);
    }

    asm(
        "add    sp, %0\n" : : "r"(stack_space)
    );

    asm(
        "pop { r0-r11, lr }\n"

        // do overwritten instructions
        //"ldr   r2, =0xa5c30\n"
        "push  {r3, r4, r5, r6, r7, r8, r9, r10, r11, lr}\n"
        "mov   r4, r0\n"
        "ldr   r5, =0x973e8\n"
        

        // jump back
        "ldr pc, =0xe0342577\n"
    );
}

