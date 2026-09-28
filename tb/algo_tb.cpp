// =============================================================================
//  algo_tb.cpp —— C 仿真验证 (8bit, 一拍一个像素)
//
//  接口标准 (见 algo_top.cpp 顶部):
//      tdata 8bit / tvalid / tlast = EOL(行尾) / tuser = SOF(帧首)
//
//  验证内容:
//    (1) 逐拍数据: 输出的每一拍和【优化前原算法】的参考模型逐拍比对
//                  (data / last / user 三项都必须完全一致)
//    (2) tuser = SOF: 每帧只出现一次, 在该帧第 1 个有效输出像素那一拍
//    (3) tlast = EOL: 每 256 个输出像素一次(从帧首算起);
//                     帧尾那拍本来就是行尾, 所以那里也是 1
//    (4) 输入侧按标准构造: 每行最后一个像素 tlast=1, 每帧第一个像素 tuser=1
//
//  说明: 这里调用 algo_top() 本身, 所以接口和数据一起验证。
//        (algo_top 里是 for(;;); C 仿真时由 __SYNTHESIS__ 分支变成有界循环,
//         拍数 = algo_top.h 里的 ALGO_TB_PIXELS。)
// =============================================================================

#include <cstdio>
#include <cstdlib>
#include "../src/algo_top.h"

#define IMG_W  ALGO_IMG_W
#define IMG_H  ALGO_IMG_H
#define IMG_N  (IMG_W * IMG_H)
#define PIXELS ALGO_TB_PIXELS

#define CORNER_WEIGHT   1
#define NEIGHBOR_WEIGHT 2
#define SELF_WEIGHT     4

// =============================================================================
//  参考模型: 优化前的原写法 (515 队列 + 常量取模 %515)
//  变量加 ref_ 前缀, 避免和 DUT 里的同名全局量冲突。
// =============================================================================
static int ref_idx = 0;
static int ref_op_idx = 0;
static int ref_first_frame = 1;
static int ref_q[515] = { 0 };
static int ref_rear_frame[257] = { 0 };

static void ref_denoise(
    ap8_t in_data,
    int in_valid,
    int in_last,
    ap8_t* out_data,
    int* out_last,
    int* out_valid
)
{
    *out_last = 0;
    *out_valid = 0;
    *out_data = 0;

    if(in_valid){
        ref_q[ref_idx % 515] = in_data;
        if(ref_idx >= 65279){
            ref_rear_frame[ref_idx - 65279] = in_data;
        }

        if(ref_idx >= 257 && ref_idx <= 513){
            if(ref_idx == 257){
                ref_op_idx = 0;
            }
            *out_valid = 1;
            *out_data = ref_q[ref_op_idx++];
        }
        else if(ref_idx > 513 && ref_idx <= 65535){
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

                *out_data = (ap8_t)(sum >> 4);
                ref_op_idx++;
            }
            else{
                if(ref_op_idx == ref_idx - 257){
                    *out_valid = 1;
                    *out_data = ref_q[ref_op_idx++ % 515];
                }
            }
        }
        else {
            if(!ref_first_frame){
                if(ref_op_idx <= 65535){
                    *out_valid = 1;
                    if(ref_op_idx == 65535){
                        *out_last = 1;
                    }
                    *out_data = ref_rear_frame[ref_op_idx - 65279];
                    ref_op_idx++;
                }
            }
        }
    }

    if(in_last) {
        ref_idx = 0;
        ref_first_frame = 0;
    }
    if(!in_last && in_valid) {
        ref_idx++;
    }
}

// =============================================================================
static ap_uint<8> img_in[PIXELS];

int main()
{
    // ---------------- 1) 造输入: 每帧不同渐变 + 亮块 + 加性高斯噪声 ----------------
    srand(20260928);
    for (int i = 0; i < PIXELS; i++) {
        int f = i / IMG_N;
        int p = i % IMG_N;
        int r = p / IMG_W;
        int c = p % IMG_W;
        int v = 60 + ((r + c + f * 7) % 40);
        if (r >= 96 && r < 160 && c >= 96 && c < 160) v = 200;
        int n = ((rand() % 21) - 10) + ((rand() % 21) - 10) + ((rand() % 21) - 10);
        v += n;
        if (v < 0)   v = 0;
        if (v > 255) v = 255;
        img_in[i] = (ap_uint<8>)v;
    }

    // ---------------- 2) 按接口标准喂进去 ----------------
    hls::stream<axis_t> s_axis("s_axis");
    hls::stream<axis_t> m_axis("m_axis");

    for (int i = 0; i < PIXELS; i++) {
        axis_t x;
        x.data = img_in[i];
        x.last = ((i % IMG_W) == (IMG_W - 1)) ? 1 : 0;   // EOL: 每行最后一个像素
        x.user = ((i % IMG_N) == 0) ? 1 : 0;             // SOF: 每帧第一个像素
        s_axis.write(x);
    }

    // ---------------- 3) 跑 DUT ----------------
    algo_top(s_axis, m_axis);

    // ---------------- 4) 收输出, 逐拍和参考模型比对 ----------------
    int bad_data = 0, bad_last = 0, bad_user = 0;
    int first_bad = -1;
    int tuser_cnt = 0, tlast_cnt = 0;
    int m_in_frame = 0;              // 帧内有效输出像素序号 (0-based)

    // denoise() 的前 257 拍没有输出(启动阶段), 包装层不发这些拍
    //   => 输出拍数 = PIXELS - 257, 且第 j 拍对应输入第 (j + 257) 拍。
    const int SKIP   = 257;
    const int NB_OUT = PIXELS - SKIP;

    for (int j = 0; j < NB_OUT; j++) {
        axis_t x = m_axis.read();
        int i = j + SKIP;                    // 对应的输入像素序号

        ap8_t rd; int rl, rv;
        ref_denoise(img_in[i], 1, ((i % IMG_N) == (IMG_N - 1)) ? 1 : 0,
                    &rd, &rl, &rv);

        // (1) 数据逐拍比对
        if ((int)x.data != (int)rd) {
            if (first_bad < 0) first_bad = i;
            bad_data++;
        }

        // (2)(3) 期望的 TUSER / TLAST (按输出像素在帧内的位置)
        int exp_user = 0, exp_last = 0;
        if (rv) {
            if (m_in_frame == 0)                    exp_user = 1;   // SOF
            if ((m_in_frame % IMG_W) == (IMG_W - 1)) exp_last = 1;   // EOL
            m_in_frame = (m_in_frame + 1) % IMG_N;
        }
        if ((int)x.user != exp_user) bad_user++;
        if ((int)x.last != exp_last) bad_last++;
        if (x.user) tuser_cnt++;
        if (x.last) tlast_cnt++;
    }

    // ---------------- 5) 结果 ----------------
    printf("\n=============== algo_top 接口 + 算法 C 仿真 ===============\n");
    printf("拍数            : %d\n", PIXELS);
    printf("(1) data  差异  : %d\n", bad_data);
    if (first_bad >= 0) printf("    首个差异在第 %d 拍\n", first_bad);
    printf("(2) tuser 差异  : %d     (SOF, 期望每帧 1 次)\n", bad_user);
    printf("(3) tlast 差异  : %d     (EOL, 期望每行 1 次)\n", bad_last);
    printf("    tuser 出现次数: %d\n", tuser_cnt);
    printf("    tlast 出现次数: %d\n", tlast_cnt);
    printf("RESULT: %s\n",
           ((bad_data | bad_user | bad_last) == 0) ? "PASS" : "FAIL");
    printf("==========================================================\n\n");

    return 0;
}
