# 用于 Resolume Arena 的 NVIDIA Background

`NVIDIA Background` 是一款用于 Resolume Arena 的 64 位 FFGL 2 视频特效插件。
它调用 NVIDIA Video Effects SDK 0.7.6 的 AI Green Screen（AI 绿幕）功能，
生成带透明背景的预乘 RGBA 前景画面。

插件在 GPU 上完成 OpenGL/CUDA 数据桥接和遮罩处理。同一个插件 DLL 内含适用于
RTX 20、30、40 和 50 系列显卡的 CUDA 代码。每台电脑都必须安装与其 GPU 架构
相匹配的 NVIDIA Video Effects 运行库和模型文件。

## 主要功能

- 质量优先和性能优先两种推理模式
- 仅人物、人物加椅子两种分割范围
- 阈值、柔化、羽化、混合和遮罩反转控制
- 遮罩预览和 NVIDIA 时序状态重置
- GPU 边缘颜色清理
- 输出预乘 Alpha，可直接在 Arena 中合成
- CUDA 目标架构：`sm_75`、`sm_86`、`sm_89` 和 `sm_120`

## 运行与编译要求

- 64 位 Windows 10 或 Windows 11
- Resolume Arena 7.3.1 或更高版本
- NVIDIA RTX 系列显卡及较新的 NVIDIA 显卡驱动
- NVIDIA Video Effects SDK 0.7.6 运行库和与显卡匹配的模型
- Visual Studio 2022，并安装“使用 C++ 的桌面开发”组件
- CMake 3.24 或更高版本
- CUDA Toolkit 12.8 或更高版本

本仓库不包含 NVIDIA 运行库和模型。请从
[NVIDIA Broadcast SDK 资源页面](https://www.nvidia.com/en-us/geforce/broadcasting/broadcast-sdk/resources/)
下载与显卡架构对应的 Video Effects 0.7.6 可再发行组件。

运行库与显卡的对应关系请参阅[显卡兼容性说明](docs/GPU-COMPATIBILITY.md)。

## 安装已编译插件

将 `NVIDIA Background.dll` 复制到：

```text
%USERPROFILE%\Documents\Resolume Arena\Extra Effects
```

重新启动 Arena，在“效果”面板中找到 `NVIDIA Background`，然后将它拖到摄像机或
视频片段上。可在下方放置另一个片段或图层，以确认插件是否正确输出透明通道。

## 克隆和编译

克隆仓库时同时拉取已经固定版本的 FFGL 依赖：

```powershell
git clone --recurse-submodules <repository-url>
cd arena-nvidia-background
```

使用仓库提供的预设进行配置和编译：

```powershell
cmake --preset windows-vs2022 -DCUDAToolkit_ROOT="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8"
cmake --build --preset windows-release
```

编译完成的插件位于：

```text
build\Release\NVIDIA Background.dll
```

在已经安装匹配 NVIDIA 运行库的电脑上执行冒烟测试：

```powershell
ctest --preset windows-release
```

如果克隆后缺少 FFGL，请执行：

```powershell
git submodule update --init --recursive
```

## 参数说明

| 参数 | 说明 |
| --- | --- |
| 模式（Mode） | 选择质量优先或性能优先，并设置是否识别椅子。 |
| 阈值（Threshold） | 设置前景与背景的置信度分界值。 |
| 柔化（Softness） | 设置 Alpha 透明度软过渡区域的宽度。 |
| 羽化（Feather） | 设置 GPU 遮罩的平滑半径。 |
| 混合（Mix） | 在原始画面 Alpha 与生成的 Alpha 之间进行混合。 |
| 反转（Invert） | 反转生成的遮罩。 |
| 查看遮罩（View Mask） | 显示经过处理的黑白遮罩。 |
| 重置状态（Reset State） | 更换输入源后重置 NVIDIA 的时序跟踪状态。 |
| 边缘清理（Edge Cleanup） | 从人物内部提取颜色修复边缘，减少背景颜色污染。 |

如果遇到运行库或模型错误，请参阅[故障排查](docs/TROUBLESHOOTING.md)。

## 仓库内容

- `src/` — FFGL 插件、NVIDIA API 动态加载器和 CUDA 内核源码
- `tests/` — NVIDIA 运行库、原始序列和 OpenGL 冒烟测试
- `third_party/ffgl/` — 固定版本的 Resolume FFGL Git 子模块
- `docs/` — 显卡兼容性与故障排查文档

编译目录、发布版二进制文件、分析帧和测试视频不会纳入源码版本控制。

## 许可证与商标

本项目采用 [MIT 许可证](LICENSE)发布。第三方组件及其声明请参阅
[第三方声明](THIRD_PARTY_NOTICES.md)。

NVIDIA、CUDA 及相关名称是 NVIDIA Corporation 的商标。Resolume 和 Arena 是
Resolume B.V. 的商标。本项目为独立项目，与 NVIDIA Corporation 或 Resolume B.V.
不存在从属、合作或官方认可关系。
