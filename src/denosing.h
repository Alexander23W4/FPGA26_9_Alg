#ifndef DENOSING_H
#define DENOSING_H
typedef ap_uint<8> ap8_t;

ap_uint<8> denoise_pixel(ap_uint<8> pixel, ap_uint<8> prev);

typedef struct{
    int pixels[515];
    int last;
    int head;
}denoise_queue;

#endif 

