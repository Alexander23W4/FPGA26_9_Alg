#include "algo_top.h"

int frame_idx = 0;

// 256x256 图像
#define IMG_W 256
#define IMG_H 256

// 3x3 Gaussian kernel
//      1 2 1
//      2 4 2
//      1 2 1
//
// 最后除以 16

ap8_t denoise(
    ap8_t pixel,
    int in_valid,
    int in_last,
    int* out_last,
    int* out_valid
)
{
// 先定义 流控 和 状态量/寄存器 默认值  和  output默认值

    // 当前行的列位置
    static int col = 0;
    // 当前行号
    static int row = 0;


    // 行缓冲：
    // linebuf[0][x] = 上一行
    // linebuf[1][x] = 上两行
    static ap8_t linebuf0[IMG_W];
    static ap8_t linebuf1[IMG_W];

    // 3x3 窗口
    static ap8_t p00, p01, p02;
    static ap8_t p10, p11, p12;
    static ap8_t p20, p21, p22;

    ap8_t result = 0;

    *out_valid = 0;
    *out_last  = 0;

    if (!in_valid) {
        return 0;
    }

    /*
     * 保存当前输入像素
     *
     * linebuf0[col]：上一行
     * linebuf1[col]：上两行
     */
    ap8_t old1 = linebuf0[col];
    ap8_t old2 = linebuf1[col];

    linebuf1[col] = old1;
    linebuf0[col] = pixel;

    /*
     * 窗口左移
     */

    p00 = p01;
    p01 = p02;
    p02 = old2;

    p10 = p11;
    p11 = p12;
    p12 = old1;

    p20 = p21;
    p21 = p22;
    p22 = pixel;

    /*
     * 当窗口真正形成以后再计算
     *
     * 当前像素位于：
     *
     * p22
     *
     * 所以至少需要：
     * row >= 2
     * col >= 2
     */
    if (row >= 2 && col >= 2) {

        int sum =
              p00
            + 2 * p01
            + p02
            + 2 * p10
            + 4 * p11
            + 2 * p12
            + p20
            + 2 * p21
            + p22;

        result = (ap8_t)(sum >> 4);

        *out_valid = 1;
    }

    /*
     * 当前像素是否为这一帧最后一个像素
     */
    if (in_last) {
        *out_last = 1;
    }

    /*
     * 更新坐标
     */
    if (col == IMG_W - 1) {
        col = 0;

        if (row == IMG_H - 1) {
            row = 0;

            // 新的一帧
            // 清空窗口
            p00 = p01 = p02 = 0;
            p10 = p11 = p12 = 0;
            p20 = p21 = p22 = 0;
        } else {
            row++;
        }
    } else {
        col++;
    }

    return result;
}

