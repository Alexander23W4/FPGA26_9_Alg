#include "algo_top.h"

ap8_t denoise_pixel(ap8_t pixel, ap8_t prev)
{
    ap_uint<9> sum = pixel + prev;
    return sum >> 1;
}


