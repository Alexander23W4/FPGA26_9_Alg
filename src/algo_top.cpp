#include "algo_top.h"

/*
axis8_t

├── data   数据
├── keep   字节有效标志
├── strb    字节类型标志
├── user   用户自定义信息
├── last   数据流最后一个标志
├── id     数据流 ID
└── dest   目标地址/目标端口
*/

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

        ap8_t denoise_out;
        denoise_out = denoise_pixel(pixel, prev);

        out.data = contour_ext(denoise_out);

        // 保留 AXI-Stream 的 sideband
        out.keep = in.keep;
        out.strb = in.strb;
        out.last = in.last;
        out.user = in.user;
        out.id   = in.id;
        out.dest = in.dest;

        prev = pixel;

        m_axis.write(out);
    }
}

