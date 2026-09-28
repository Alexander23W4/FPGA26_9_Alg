// =============================================================================
//  algo_top.h —— 接口定义
//
//  接口标准 (统一约定):
//
//      输入图像 256x256x8bit
//      tdata  : 8bit            一拍一个像素
//      tvalid : 这一拍数据有效    (AXI-Stream 必备握手)
//      tlast  : EOL             行尾, 每行最后一个像素
//      tuser  : SOF             帧首, 每帧第一个像素
//
//      不接出: tkeep / tstrb / tid / tdest
//
//  ★ 关于 tready:
//      AXI-Stream 的 TVALID/TREADY 是握手信号, 只要用 hls::stream + axis 接口,
//      综合工具就会生成, 【无法从接口里去掉】(流式内核必须有握手, 否则无法表达
//      "何时可以读下一拍")。所以这里保留 tready, PL 侧把它拉高即可 —— 效果上
//      就是"不管"。
//      而 tkeep / tstrb / tid / tdest 可以在载荷类型里彻底关掉, 一个端口都不出。
// =============================================================================
#ifndef ALGO_TOP_H
#define ALGO_TOP_H

#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <ap_int.h>

#include "denosing.h"
#include "contour_ext.h"

#define ALGO_IMG_W      256     // 一行 256 个像素
#define ALGO_IMG_H      256     // 一帧 256 行
#define ALGO_PIXEL_W    8       // 8bit 灰度

#define IMG_IDX_MAX     (ALGO_IMG_W * ALGO_IMG_H - 1)

// -----------------------------------------------------------------------------
//  AXI4-Stream 载荷: 只保留 DATA / LAST / USER
//    KEEP / STRB / ID / DEST 全部关掉 => 综合出来不会有这些端口
// -----------------------------------------------------------------------------
#define ALGO_AXIS_W      8
#define ALGO_AXIS_SIGS   (AXIS_ENABLE_LAST | AXIS_ENABLE_USER)

typedef hls::axis<ap_uint<ALGO_AXIS_W>, 1, 0, 0, ALGO_AXIS_SIGS> axis_t;

// 顶层: 输入像素流 -> 处理 -> 输出像素流
void algo_top(hls::stream<axis_t> &s_axis,
              hls::stream<axis_t> &m_axis);

typedef ap_uint<8> ap8_t;

// -----------------------------------------------------------------------------
//  C 仿真专用: algo_top 里是 for(;;) 死循环, csim 下不会返回, testbench 跑不下去。
//  所以非综合时把它变成有界循环 (拍数 = 下面这个)。__SYNTHESIS__ 只在综合时定义,
//  综合出来的 RTL 和原来完全一致。
// -----------------------------------------------------------------------------
#ifndef __SYNTHESIS__
#define ALGO_TB_FRAMES 4
#define ALGO_TB_PIXELS (ALGO_TB_FRAMES * ALGO_IMG_W * ALGO_IMG_H)
#endif

#endif // ALGO_TOP_H
