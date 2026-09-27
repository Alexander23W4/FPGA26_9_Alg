# A_algor — 256×256×8bit 流式图像处理流水线（Vitis HLS）

这是一个**代码框架**，用 Vitis HLS 的 GUI 打开后自己往里写算法。

```
src/algo_top.h     接口约定（图像参数、AXI-Stream 类型）
src/algo_top.cpp   顶层骨架：接口 + 握手 + 循环 + 空的算法槽
tb/algo_tb.cpp     测试台骨架（空的，自己填）
```

## 接口约定

| 信号 | 含义 |
|---|---|
| `TDATA` | 8bit 灰度，`0..255` |
| `TUSER` | `1` = 本帧第一个像素（SOF） |
| `TLAST` | `1` = **本行**最后一个像素（EOL） |
| `TVALID/TREADY` | 标准握手，一拍一个像素 |

- 尺寸：**256×256**
- `TLAST` 用"行尾"而不是"帧尾"：3×3 窗口/行缓存需要知道行边界，而且这和 Xilinx VDMA / AXI4-Stream Video 的约定一致。

## 目标流水线

```
AXI-Stream(8bit)
    ↓
Line Buffer
    ↓
3×3 Sliding Window
    ↓
Gaussian 去噪
    ↓
... 剩余算法 ...
    ↓
AXI-Stream(8bit)
```

## 在 GUI 里建工程

Vitis HLS → **Create New Project**，然后：

| 项 | 值 |
|---|---|
| Project name / location | `A_algor_prj`（自己定，放在本目录下即可） |
| Design files | `src/algo_top.cpp`（头文件 `src/algo_top.h` 同目录即可） |
| Top function | `algo_top` |
| Test bench files | `tb/algo_tb.cpp`（可选） |
| Part | `xc7z015clg485-2` |
| Solution name | `solution1` |
| Flow target | `Vivado` |
| Clock period | `20` ns（= 50 MHz，和 PL 的 `FCLK_CLK0` 一致） |

然后 **C Simulation** / **C Synthesis** 直接点即可，报告在 GUI 里看。

## 两个已经踩过的坑（先知道能省时间）

1. **C 仿真的死循环**：流式内核里是 `for(;;)`，直接 csim 会卡住。标准做法是加
   `#ifdef __SYNTHESIS__` 分支，综合走死循环、csim 走有界循环（见 `tb/algo_tb.cpp` 里的说明）。
2. **3×3 卷积的输出位置天生晚一行一列**（要等下一行才能算），用**独立的输出位置计数器**标注比用延迟链干净；边界用**边缘复制**填窗口，这样首帧第一个像素就是对的。
