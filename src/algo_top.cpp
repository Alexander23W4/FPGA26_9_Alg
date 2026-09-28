#include "algo_top.h"

/*
相当于通路上只有两个缓存:
一个是ddr缓存, 分为buffer帧 + axi-stream读取帧
一个是hdmi缓存, 分为buffer帧 + hdmi读取帧

axis8_t

├── data   数据
├── keep   字节有效标志
├── strb    字节类型标志
├── user   用户自定义信息
├── last   数据流最后一个标志
├── id     数据流 ID
└── dest   目标地址/目标端口
*/

ap_uint<16> input_frame_idx = 0;

void algo_top(hls::stream<axis8_t> &s_axis,
              hls::stream<axis8_t> &m_axis)
{
#pragma HLS INTERFACE axis        port=s_axis
#pragma HLS INTERFACE axis        port=m_axis
#pragma HLS INTERFACE ap_ctrl_none port=return

    ap8_t prev = 0;

    for (;;) {
#pragma HLS PIPELINE II=1

        axis8_t in = s_axis.read();
        axis8_t out;

        ap8_t pixel = in.data;
        int denoise_in_valid = in.keep && in.strb;

        ap8_t denoise_out;
        int denoise_out_last;
        int denoise_out_valid;
        denoise_out = denoise_pixel(pixel, denoise_in_valid, in.last, &denoise_out_last, &denoise_out_valid);



        out.keep = ;
        out.strb = in.strb;
        out.last = in.last;
        out.user = in.user;
        out.id   = in.id;
        out.dest = in.dest;


        m_axis.write(out);
    }
}

