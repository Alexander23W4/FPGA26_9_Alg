// =============================================================================
//  denose_top.cpp —— 把 denoise() 包成你指定端口的顶层, 用于生成 denose.v
//
//  目标 RTL (按 denosing.cpp 顶部注释里的端口表):
//      module denose(
//          input  clk, rst,
//          input  in_data, in_valid, in_last,
//          output in_ready,
//
//          output out_data, out_valid, out_last,
//          input  out_ready
//      );
//
//  为什么要这层包装:
//      denoise() 自己的入参是 int in_valid/in_last、int* in_ready、
//      ap8_t* out_data 等, 直接把它当顶层会推断出 32 位端口和指针式接口,
//      拿不到你要的 1bit 端口。
//
//  这层做了什么:
//      · 端口类型用 ap_uint<8> / ap_uint<1>  => 综合出来就是窄端口
//      · ap_ctrl_none => 不生成 ap_start/ap_done, 模块自由运行
//      · 函数体里【没有循环】, 每拍执行一次
//        => 一拍处理一个像素的握手 (状态寄存器每拍更新)
//
//  ★ denosing.cpp 的算法/变量一个字没动; denosing.h 只是声明与实现对齐。
//
//  综合时:
//      set_top denose
//      add_files denosing.cpp
//      add_files denose_top.cpp
//      => 生成的 RTL 在 <solution>/syn/verilog/
// =============================================================================

#include <ap_int.h>
#include "denosing.h"          // 提供 ap8_t 和 denoise() 声明

void denose(
    ap_uint<8>  in_data,
    ap_uint<1>  in_valid,
    ap_uint<1>  in_last,
    ap_uint<1>  out_ready,

    ap_uint<8>& out_data,
    ap_uint<1>& out_valid,
    ap_uint<1>& out_last,
    ap_uint<1>& in_ready
)
{
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INTERFACE ap_none port=in_data
#pragma HLS INTERFACE ap_none port=in_valid
#pragma HLS INTERFACE ap_none port=in_last
#pragma HLS INTERFACE ap_none port=out_ready
#pragma HLS INTERFACE ap_none port=out_data
#pragma HLS INTERFACE ap_none port=out_valid
#pragma HLS INTERFACE ap_none port=out_last
#pragma HLS INTERFACE ap_none port=in_ready

    ap8_t out_d;
    int   out_l, out_v;
    int   in_r;

    denoise((ap8_t)in_data, (int)in_valid, (int)in_last,
            &in_r, (int)out_ready,
            &out_d, &out_l, &out_v);

    out_data  = (ap_uint<8>)out_d;
    out_valid = (ap_uint<1>)out_v;
    out_last  = (ap_uint<1>)out_l;
    in_ready  = (ap_uint<1>)in_r;
}
