// =============================================================================
//  algo_top.h —— 接口约定
//
//  图像: 256 x 256, 8bit 灰度, 一拍一个像素。
//
//  AXI4-Stream 约定:
//      TDATA : 8bit 灰度 (0..255)
//      TUSER : ==1 表示本帧第一个像素 (SOF)
//      TLAST : ==1 表示【本行】最后一个像素 (EOL)
//      TVALID/TREADY : 标准握手
//
//  TLAST 用"行尾"而不是"帧尾": 3x3 窗口/行缓存需要知道行边界;
//  而且这正好和 Xilinx VDMA / AXI4-Stream Video 的约定一致。
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

// AXI4-Stream 载荷: 8bit 数据 + 1bit user
typedef ap_axiu<ALGO_PIXEL_W, 1, 0, 0> axis8_t;

// 顶层: 输入像素流 -> 处理 -> 输出像素流
void algo_top(hls::stream<axis8_t> &s_axis,
              hls::stream<axis8_t> &m_axis);

typedef ap_uint<8> ap8_t;

#endif // ALGO_TOP_H
