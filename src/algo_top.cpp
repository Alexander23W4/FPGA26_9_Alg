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

/*
输入图像 256x256x8bit
tdata: 8bit
tvalid: 这一帧有效
tlast: eol
tuser: sof

不管:
tkeep tdest tid tstrb tready
*/


void algo_top(hls::stream<axis8_t> &s_axis,
              hls::stream<axis8_t> &m_axis)
{
#pragma HLS INTERFACE axis        port=s_axis
#pragma HLS INTERFACE axis        port=m_axis
#pragma HLS INTERFACE ap_ctrl_none port=return


    for (;;) {
#pragma HLS PIPELINE II=1

        axis8_t in = s_axis.read();
        axis8_t out;

// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

// denonise: 
        ap8_t denoise_in_data = in.data;
        int denoise_in_valid = in.keep;

        ap8_t denoise_out;
        int denoise_out_last;
        int denoise_out_valid;
        int denoise_sof;

        denoise(denoise_in_data, denoise_in_valid, in.last, in.user, &denoise_out, &denoise_out_last, &denoise_out_valid &denoise_frame_end);


// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        out.data = denoise_out;
        out.keep = denoise_out_valid;
        out.strb = 0;
        out.last = denoise_out_last;
        out.user = denoise_sof;   // 这个user我不知道axi_stream里面定义的是什么


        m_axis.write(out);
    }
}



