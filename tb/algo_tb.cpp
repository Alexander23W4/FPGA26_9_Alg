// =============================================================================
//  algo_tb.cpp —— 只验证 denosing.cpp (功能 + 反压握手时序)
//
//  只编译 denosing.cpp + 本 TB, 不牵扯 algo_top.cpp / denose_top.cpp。
//
//  握手约定 (你新版的端口):
//      in_valid + *in_ready   => 该拍输入被接收
//      out_valid + out_ready  => 该拍输出被下游取走
//
//  验证内容:
//    (1) 功能: 被下游真正取走的输出序列, 应逐拍等于"优化前原算法"的参考模型
//              的有效输出序列 (data / last / valid 三项)
//    (2) 握手时序:
//        · 输入不丢: 被接收的输入像素数 == 喂进去的总数
//        · 反压保持: out_valid=1 且 out_ready=0 时, 下一拍输出必须原样保持
//        · in_ready: 输出没被取走时不得再接收新输入 (不允许覆盖)
//    (3) 帧标记: out_last 出现的位置和参考模型一致
//
//  输入 in_valid 和下游 out_ready 都按"周期性停顿"的方式驱动,
//  以覆盖两侧都有停顿的情况。
// =============================================================================

#include <cstdio>
#include <cstdlib>
#include <ap_int.h>
#include "../src/denosing.h"

#define IMG_W 256
#define IMG_H 256
#define IMG_N (IMG_W * IMG_H)
#define FRAMES 4
#define NPIX (FRAMES * IMG_N)

#define CORNER_WEIGHT   1
#define NEIGHBOR_WEIGHT 2
#define SELF_WEIGHT     4

// ---------------- 参考模型: 优化前的原写法 (515 队列 + 取模) ----------------
static int ref_idx = 0, ref_op_idx = 0, ref_first_frame = 1;
static int ref_q[515] = {0}, ref_rear_frame[257] = {0};

static void ref_denoise(ap8_t in_data, int in_valid, int in_last,
                        ap8_t* out_data, int* out_last, int* out_valid)
{
    *out_last = 0; *out_valid = 0; *out_data = 0;
    if(in_valid){
        ref_q[ref_idx % 515] = in_data;
        if(ref_idx >= 65279) ref_rear_frame[ref_idx - 65279] = in_data;
        if(ref_idx >= 257 && ref_idx <= 513){
            if(ref_idx == 257) ref_op_idx = 0;
            *out_valid = 1; *out_data = ref_q[ref_op_idx++];
        } else if(ref_idx > 513 && ref_idx <= 65535){
            if(ref_idx % 256 > 1){
                *out_valid = 1;
                int sum =
                    ref_q[(ref_idx + 515 - 514) % 515] * CORNER_WEIGHT
                    + ref_q[(ref_idx + 515 - 513) % 515] * NEIGHBOR_WEIGHT
                    + ref_q[(ref_idx + 515 - 512) % 515] * CORNER_WEIGHT
                    + ref_q[(ref_idx + 515 - 258) % 515] * NEIGHBOR_WEIGHT
                    + ref_q[(ref_idx + 515 - 257) % 515] * SELF_WEIGHT
                    + ref_q[(ref_idx + 515 - 256) % 515] * NEIGHBOR_WEIGHT
                    + ref_q[(ref_idx + 515 - 2) % 515] * CORNER_WEIGHT
                    + ref_q[(ref_idx + 515 - 1) % 515] * NEIGHBOR_WEIGHT
                    + ref_q[ref_idx % 515] * CORNER_WEIGHT;
                *out_data = (ap8_t)(sum >> 4); ref_op_idx++;
            } else {
                if(ref_op_idx == ref_idx - 257){ *out_valid = 1; *out_data = ref_q[ref_op_idx++ % 515]; }
            }
        } else {
            if(!ref_first_frame && ref_op_idx <= 65535){
                *out_valid = 1;
                if(ref_op_idx == 65535) *out_last = 1;
                *out_data = ref_rear_frame[ref_op_idx - 65279]; ref_op_idx++;
            }
        }
    }
    if(in_last) { ref_idx = 0; ref_first_frame = 0; }
    if(!in_last && in_valid) ref_idx++;
}

// ---------------- 参考模型的"有效输出"序列 ----------------
static ap_uint<8> ref_list_d[NPIX];
static ap_uint<1> ref_list_l[NPIX];
static int        ref_list_n = 0;

static ap_uint<8> img_in[NPIX];

int main()
{
    // ---- 造输入 ----
    srand(20260928);
    for (int i = 0; i < NPIX; i++) {
        int f = i / IMG_N, p = i % IMG_N, r = p / IMG_W, c = p % IMG_W;
        int v = 60 + ((r + c + f * 7) % 40);
        if (r >= 96 && r < 160 && c >= 96 && c < 160) v = 200;
        v += ((rand() % 21) - 10) + ((rand() % 21) - 10) + ((rand() % 21) - 10);
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        img_in[i] = (ap_uint<8>)v;
    }

    // ---- 逐拍仿真 ----
    int  in_fed = 0, in_taken = 0, out_taken = 0;
    int  bad_data = 0, bad_last = 0, first_bad = -1;
    int  bad_hold = 0, bad_ovr = 0;      // 反压保持 / 覆盖检查
    int  prev_out_valid = 0; ap_uint<8> prev_out_d = 0; ap_uint<1> prev_out_l = 0;
    int  cyc = 0;

    while (out_taken < ref_list_n || in_fed < NPIX) {
        // 输入: 周期性停顿 (每 5 拍停 1 拍)
        int drive_valid = ((cyc % 5) != 4) ? 1 : 0;
        if (in_fed >= NPIX) drive_valid = 0;
        // 下游: 周期性反压 (每 7 拍停 1 拍)
        int out_ready = ((cyc % 7) != 6) ? 1 : 0;

        ap8_t in_d = 0; int in_l = 0;
        if (drive_valid) { in_d = img_in[in_fed]; in_l = ((in_fed % IMG_N) == (IMG_N - 1)) ? 1 : 0; }

        int in_ready = 0, out_valid = 0, out_last = 0; ap8_t out_d = 0;
        denoise(in_d, drive_valid, in_l, &in_ready, out_ready, &out_d, &out_last, &out_valid);

        // 反压保持检查: 上一拍给了有效输出但下游没收, 这一拍的输出必须原样
        if (prev_out_valid == 1) {
            if (!(out_valid == 1 && out_d == prev_out_d && out_last == prev_out_l)) bad_hold++;
            if (in_ready == 1 && drive_valid == 1) bad_ovr++;   // 不许在输出未取走时收新输入
        }

        // 输入握手 => 像素被接收, 同时推进参考模型
        if (drive_valid && in_ready) {
            ap8_t rd; int rl, rv;
            ref_denoise(in_d, 1, in_l, &rd, &rl, &rv);
            if (rv) { ref_list_d[ref_list_n] = rd; ref_list_l[ref_list_n] = (ap_uint<1>)rl; ref_list_n++; }
            in_fed++; in_taken++;
        }

        // 输出握手 => 输出被取走, 与参考序列比对
        if (out_valid && out_ready) {
            if (out_taken < ref_list_n) {
                if ((int)out_d != (int)ref_list_d[out_taken]) { if (first_bad < 0) first_bad = out_taken; bad_data++; }
                if ((int)out_last != (int)ref_list_l[out_taken]) bad_last++;
            } else bad_data++;
            out_taken++;
        }

        prev_out_valid = (out_valid && !out_ready) ? 1 : 0;
        prev_out_d = out_d; prev_out_l = (ap_uint<1>)out_last;
        cyc++;
        if (cyc > 40 * NPIX) { printf("[tb] 仿真超时, 提前退出\n"); break; }
    }

    printf("\n============ denosing.cpp 反压版 仿真 ============\n");
    printf("拍数(周期)      : %d\n", cyc);
    printf("喂入像素        : %d  (共 %d)\n", in_taken, NPIX);
    printf("取走输出        : %d  (参考有效输出 %d)\n", out_taken, ref_list_n);
    printf("(1) data  差异  : %d\n", bad_data);
    if (first_bad >= 0) printf("    首个差异在第 %d 个输出\n", first_bad);
    printf("    last  差异  : %d\n", bad_last);
    printf("(2) 反压保持错  : %d   (输出未被取走时没有原样保持)\n", bad_hold);
    printf("    反压期收输入: %d   (输出未取走却收了新输入 => 会丢数据)\n", bad_ovr);
    printf("(3) 输出数一致  : %s\n", (out_taken == ref_list_n) ? "YES" : "NO");
    printf("RESULT: %s\n",
           ((bad_data|bad_last|bad_hold|bad_ovr)==0 && out_taken==ref_list_n && in_taken==NPIX) ? "PASS" : "FAIL");
    printf("==================================================\n\n");
    return 0;
}
