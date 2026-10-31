#include "dryos.h"
#include "fps-engio_per_cam.h"

int get_fps_register_a(void)
{
    return *(uintptr_t*)(FPS_REGISTER_A);
}
 
int get_fps_register_a_default(void)
{
    return *(uintptr_t*)(FPS_REGISTER_A + 4);     // 0xD240819C
}
 
int get_fps_register_b(void)
{
    return *(uintptr_t*)(FPS_REGISTER_B);
}
