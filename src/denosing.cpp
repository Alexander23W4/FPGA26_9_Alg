#include "algo_top.h"

// 256x256 图像
#define IMG_W 256
#define IMG_H 256

#define CORNER_WEIGHT 1
#define NEIGHBOR_WEIGHT 2
#define SELF_WEIGHT 4



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

void denoise(
    ap8_t in_data,
    int in_valid,
    int in_last,

    ap8_t* out_data,
    int* out_last,
    int* out_valid
)
{

// 先 状态量/寄存器 默认值  和  output默认值
    *out_last = 0;
    *out_valid = 0;
    *out_data = 0;


// 有效output 实现逻辑
    if(in_valid){
        q[wr] = in_data;                    // 原来是 q[idx % 515] = in_data;  (wr 恒等于 idx%515)

        // ★ 写法优化(算法逻辑完全不变): 9 个抽头在 q 里的下标 = (wr - K) 回绕,
        //   和原来的 (idx + 515 - K) % 515 完全等价, 但只是一次比较 + 一次加法。
        //   (idx 恒 >= 0 且 K <= 514, 所以最多补一次 515 就够)
        int r514 = wr - 514; if(r514 < 0) r514 += 515;
        int r513 = wr - 513; if(r513 < 0) r513 += 515;
        int r512 = wr - 512; if(r512 < 0) r512 += 515;
        int r258 = wr - 258; if(r258 < 0) r258 += 515;
        int r257 = wr - 257; if(r257 < 0) r257 += 515;
        int r256 = wr - 256; if(r256 < 0) r256 += 515;
        int r2   = wr -   2; if(r2   < 0) r2   += 515;
        int r1   = wr -   1; if(r1   < 0) r1   += 515;

        if(idx >= 65279){
            rear_frame[idx - 65279] = in_data;  // 尾帧单独存储, 避免被覆盖
        }

        if(idx >= 257 && idx <= 513){
            if(idx == 257){
                op_idx = 0; // 在这里同步一下, 开始新的一帧的输出
            }

            *out_valid = 1;
            *out_data = q[op_idx++];   
        }
        else if(idx > 513 && idx <= 65535){
            // ★ 写法优化: idx 恒 >= 0, 所以 idx % 256 等价于 idx & 255。
            //   (原来 idx 是 int, 综合出来的是【有符号】取模 srem, 是最贵的一种)
            if((idx & 255) > 1){
                    *out_valid = 1;
                    ap_uint<16> sum =      // ★ 写法优化: 9 项最大 16*255=4080, 16 位足够
                        q[r514] * CORNER_WEIGHT
                        + q[r513] * NEIGHBOR_WEIGHT
                        + q[r512] * CORNER_WEIGHT
                        + q[r258] * NEIGHBOR_WEIGHT
                        + q[r257] * SELF_WEIGHT
                        + q[r256] * NEIGHBOR_WEIGHT
                        + q[r2]   * CORNER_WEIGHT
                        + q[r1]   * NEIGHBOR_WEIGHT
                        + q[wr]   * CORNER_WEIGHT;

                    *out_data = (ap8_t)(sum >> 4);

                    op_idx++;
            }
            else{
                if(op_idx == idx - 257){
                    *out_valid = 1;
                    // ★ 写法优化: 此处必然 op_idx == idx-257,
                    //   所以 op_idx % 515 == (idx-257) % 515 == r257, 不需要取模。
                    *out_data = q[r257];
                    op_idx++;
                }
            }
        }
        else {
            if(!first_frame){
                if(op_idx <= 65535){   // op_idx 最终停到 65535
                    *out_valid = 1;
                    if(op_idx == 65535){
                        *out_last = 1;
                    }
                    *out_data = rear_frame[op_idx - 65279];
                    op_idx++;
                }
            }
        }
    }




// 状态更新


    if(in_last) {
        idx = 0;
        wr = 0;                 // ★ 新增: 写指针与 idx 同步复位 (保持 wr == idx%515)
        first_frame = 0;
    }
    if(!in_last && in_valid) {
        idx++;
        wr++;                   // ★ 新增: 写指针回绕推进 (替代 idx % 515)
        if(wr == 515){
            wr = 0;
        }
    }

}

