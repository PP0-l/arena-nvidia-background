# 故障排查

## 特效可以加载，但画面没有完成抠像

请确认 NVIDIA Video Effects 运行库和模型包与显卡代数相匹配。模型文件名末尾的
`_75`、`_86`、`_89` 或 `_120` 应与 `GPU-COMPATIBILITY.md` 中记录的 GPU
架构一致。

## 找不到 NVIDIA 运行库

默认安装位置为：

```text
C:\Program Files\NVIDIA Corporation\NVIDIA Video Effects
```

如果运行库安装在其他位置，请在启动 Arena 前将 `NV_VIDEO_EFFECTS_PATH` 设置为
对应目录。

## 笔记本电脑上 CUDA/OpenGL 初始化失败

请在 Windows“图形设置”或 NVIDIA 应用程序配置中，将 Resolume Arena 设置为使用
高性能 NVIDIA 独立显卡，而不是集成显卡。更改后重新启动 Arena。

## 遮罩仍在跟踪之前的摄像机画面

更换摄像机、视频片段或大幅跳转播放位置后，请点击 `Reset State`（重置状态）。

## CMake 找不到 FFGL

请在仓库根目录执行：

```powershell
git submodule update --init --recursive
```

## CMake 找不到 CUDA 12.8

请安装 CUDA Toolkit 12.8 或更高版本，也可以在配置时明确指定安装位置：

```powershell
cmake --preset windows-vs2022 -DCUDAToolkit_ROOT="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8"
```
