#include "algo_top.h"

int frame_idx = 0;   // 0-65535

ap8_t denoise_pixel(ap8_t pixel, int in_valid, int in_last, int* out_last, int* out_valid)
{



    // 流控:
    if (in_last) {
        frame_idx = 0;
    } else {
        frame_idx++;
    }
}


