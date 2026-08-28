# 显卡兼容性

插件 DLL 内含适用于四代 RTX 显卡的 CUDA 机器代码。目标电脑上安装的 NVIDIA
Video Effects SDK 0.7.6 运行库和模型包必须与 GPU 架构相匹配。

| GPU 系列 | CUDA 目标架构 | 所需 NVIDIA 组件 |
| --- | --- | --- |
| GeForce RTX 20 / Quadro RTX | `sm_75` | 适用于 RTX 20 / Turing 的 Video Effects 0.7.6 |
| GeForce RTX 30 / RTX Ampere | `sm_86` | 适用于 RTX 30 / Ampere 的 Video Effects 0.7.6 |
| GeForce RTX 40 / RTX Ada | `sm_89` | 适用于 RTX 40 / Ada 的 Video Effects 0.7.6 |
| GeForce RTX 50 / Blackwell | `sm_120` | 适用于 RTX 50 / Blackwell 的 Video Effects 0.7.6 |

运行库通常安装在：

```text
C:\Program Files\NVIDIA Corporation\NVIDIA Video Effects
```

如需指定其他运行库目录，请在启动 Arena 前设置 `NV_VIDEO_EFFECTS_PATH` 环境变量。

本插件不支持 GTX、AMD、Intel，以及不具备所需 Tensor Core 能力的 NVIDIA 显卡。
