#ifndef _adtglog2_h_
#define _adtglog2_h_

#include <stdint.h>

// Max size of CMOS command buffer.
// (I have no idea what the observed max is, will refine this with experiments)
//
// NB: some of ADTG, CMOS, CMOS16 and CMOSDMA use 16 bit commands, some 32.
// I don't know which are which.  End marker is -1 of the appropriate length.
#define MAX_CMOS_BUF 512
#define CMOS_END_MARKER 0xffffffff

void log_cmos_command_buffer(uint32_t *cmos_buf, uint32_t lr);

#endif
