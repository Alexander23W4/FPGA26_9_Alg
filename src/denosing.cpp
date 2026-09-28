#include "algo_top.h"

// 256x256 图像
#define IMG_W 256
#define IMG_H 256

#define CORNER_WEIGHT 1
#define NEIGHBOR_WEIGHT 2
#define SELF_WEIGHT 4

// module denose(
//     input clk, rst,
//     input in_data, in_valid, in_last
//     output in_ready,

//     output out_data, out_valid, out_last,
//     input out_ready

// );

// endmodule

/*
只有中间254x254的像素进行3x3的kernel处理, 周围一圈的像素不处理

存下来之前的515个有效像素, 输出延迟为257(读到第i个输出, 输出第i-257个像素的处理值)
存储介质用队列
typedef struct{
    int pixels[515];
    int last;
    int head;
}denoise_queue;

i = 257开始, 开始输出:

如果 i = 257-513, 直接输出 i - 257

只有 i > 511(不是前两行) && i % 256 > 1 (不是前两列) 时, 进行Gauss kernel去噪处理 
当 513 < i <= 65535 且 不属于上一行的情况时, 直接输出i-257

当 收到 in_last之后, 从65279开始, 每一拍输出一帧(从队列里面取)

i-514  i-513  i-512
i-258  i-257  i-256
i-2    i-1    i

3x3 Gaussian kernel
     1 2 1
     2 4 2
     1 2 1
*/

/*
全程用 idx 和 op_idx 进行流控, 流缓存对象是q
*/
int idx = 0;
int op_idx = 0;
int first_frame = 1;

// ★ 写法优化(算法逻辑完全不变): 存的是 8bit 像素, 用 ap_uint<8> 而不是 int。
//   存储缩到 1/4, 下标加法也从 32 位降到 8 位。
ap_uint<8> q[515] = { 0 };
ap_uint<8> rear_frame[257] = { 0 }; // 65279-65535

// ★ 写法优化(算法逻辑完全不变): 新增写指针 wr。
//   它始终等于 idx % 515, 但用"回绕计数"代替取模 —— 这样 q[...] 里那 10 处
//   常量取模 %515 就全部不需要了 (515 不是 2 的幂, 每处都会被综合成一个除法阵列)。
int wr = 0;
ap_uint<8> pending_data = 0;
int pending_last = 0;
int pending_valid = 0;

void denoise(
    ap8_t in_data,
    int in_valid,
    int in_last,
    int* in_ready,
    int out_ready,

    ap8_t* out_data,
    int* out_last,
    int* out_valid
)
{

    // Present the registered output until the downstream side accepts it.
    *out_data = pending_data;
    *out_last = pending_last;
    *out_valid = pending_valid;
    *in_ready = !pending_valid || out_ready;

    if(pending_valid && out_ready){
        pending_valid = 0;
        pending_last = 0;
    }

    int has_output = 0;
    int output_idx = op_idx;
    ap_uint<8> next_data = 0;
    int next_last = 0;
    int r514 = wr - 514; if(r514 < 0) r514 += 515;
    int r513 = wr - 513; if(r513 < 0) r513 += 515;
    int r512 = wr - 512; if(r512 < 0) r512 += 515;
    int r258 = wr - 258; if(r258 < 0) r258 += 515;
    int r257 = wr - 257; if(r257 < 0) r257 += 515;
    int r256 = wr - 256; if(r256 < 0) r256 += 515;
    int r2   = wr -   2; if(r2   < 0) r2   += 515;
    int r1   = wr -   1; if(r1   < 0) r1   += 515;

    if(idx >= 257 && idx <= 513){
        has_output = 1;
        if(idx == 257){
            output_idx = 0;
        }
    }
    else if(idx > 513 && idx <= 65535){
        if((idx & 255) > 1 || op_idx == idx - 257){
            has_output = 1;
        }
    }
    else if(!first_frame && op_idx <= 65535){
        has_output = 1;
    }

    if(in_valid && *in_ready){
        if(has_output){
            if(idx >= 257 && idx <= 513){
                next_data = q[output_idx];
            }
            else if(idx > 513 && idx <= 65535){
                if((idx & 255) > 1){
                    ap_uint<16> sum =
                        q[r514] * CORNER_WEIGHT
                        + q[r513] * NEIGHBOR_WEIGHT
                        + q[r512] * CORNER_WEIGHT
                        + q[r258] * NEIGHBOR_WEIGHT
                        + q[r257] * SELF_WEIGHT
                        + q[r256] * NEIGHBOR_WEIGHT
                        + q[r2]   * CORNER_WEIGHT
                        + q[r1]   * NEIGHBOR_WEIGHT
                        + in_data * CORNER_WEIGHT;

                    next_data = (ap8_t)(sum >> 4);
                }
                else{
                    next_data = q[r257];
                }
            }
            else{
                next_data = rear_frame[output_idx - 65279];
                if(output_idx == 65535){
                    next_last = 1;
                }
            }

            pending_data = next_data;
            pending_last = next_last;
            pending_valid = 1;
        }

        q[wr] = in_data;
        if(idx >= 65279){
            rear_frame[idx - 65279] = in_data;
        }

        if(has_output){
            if(idx == 257){
                op_idx = 1;
            }
            else{
                op_idx++;
            }
        }

        if(in_last){
            idx = 0;
            wr = 0;
            first_frame = 0;
        }
        else{
            idx++;
            wr++;
            if(wr == 515){
                wr = 0;
            }
        }
    }

}

