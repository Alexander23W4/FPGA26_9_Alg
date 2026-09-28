#ifndef DENOSING_H
#define DENOSING_H
typedef ap_uint<8> ap8_t;

ap_uint<8> denoise_pixel(ap_uint<8> pixel, ap_uint<8> prev);

// ★ 声明已按 denosing.cpp 里的新签名(带反压握手)对齐。
//   注意: 这里只是把声明改成和实现一致, 没有改任何算法/变量。
//   握手约定:
//     in_valid  + *in_ready  => 该拍输入被接收
//     out_valid +  out_ready => 该拍输出被下游取走
//   输出未被取走时 *in_ready=0, 状态不推进, 输出保持。
void denoise(
    ap8_t in_data,
    int in_valid,
    int in_last,
    int* in_ready,
    int out_ready,
    ap8_t* out_data,
    int* out_last,
    int* out_valid
);

typedef struct{
    int pixels[515];
    int last;
    int head;
}denoise_queue;

#endif 

