#include "algo_top.h"

// 256x256 图像
#define IMG_W 256
#define IMG_H 256

#define CORNER_WEIGHT 1
#define NEIGHBOR_WEIGHT 2
#define SELF_WEIGHT 4

// 3x3 Gaussian kernel
//      1 2 1
//      2 4 2
//      1 2 1
//
// 最后除以 16

/*
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
*/

/*
全程用 idx 和 op_idx 进行流控, 流缓存对象是q
*/
int idx = 0;
int op_idx = 0;
int first_frame = 1;

int q[515] = { 0 };

void denoise(
    ap8_t in_data,
    int in_valid,
    int in_last,

    ap8_t out_data,
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
        q[idx % 515] = in_data;
    }


    if(idx >= 257 && idx <= 513){
        if(idx == 257){
            op_idx = 0; // 在这里同步一下, 开始新的一帧的输出
        }
        if(op_idx == idx - 257){
            *out_valid = 1;
            *out_data = q[op_idx++];   
        }
    }
    else if(idx > 513 && idx <= 65535){
        if(idx % 256 > 1){
            if(op_idx == idx - 257){
                *out_valid = 1;
                *out_data = q[(idx + 515 - 514) % 515] * CORNER_WEIGHT
                        + q[(idx + 515 - 513) % 515] * NEIGHBOR_WEIGHT
                        + ...

                op_idx++;
            }
        }
        else{
            if(op_idx == idx - 257){
                *out_valid = 1;
                *out_data = q[op_idx++ % 515];
            }
        }
    }
    else {
        if(!first_frame){
            if(op_idx <= 65535){   // op_idx 最终停到 65535
                *out_valid = 1
                *out_data = q[op_idx++ % 515];
            }
            if(op_idx == 65535){
                *out_last = 1;
            }
        }
    }


// 状态更新


    if(in_last) {
        idx = 0;
        first_frame = 0;
    }
    if(!in_last && in_valid) {
        idx++;
    }

}

