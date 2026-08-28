# 第三方组件声明

## Resolume FFGL SDK

本项目使用 Resolume 维护的 FreeFrameGL SDK 分支，并将其作为固定版本的 Git 子模块
放置在 `third_party/ffgl` 下。

Copyright 2023 FreeFrame contributors。FFGL SDK 采用 BSD 三条款风格许可证，
具体内容请参阅 `third_party/ffgl/LICENSE.md`。

## GLEW

FFGL SDK 包含 GLEW 2.1.0。其版权与许可证声明请参阅
`third_party/ffgl/deps/glew-2.1.0/LICENSE.txt`。

## NVIDIA 软件

本仓库不包含 NVIDIA Video Effects SDK 运行库、TensorRT 依赖、模型或安装程序。
用户需要从 NVIDIA 单独获取这些组件，并遵守 NVIDIA 对应的软件及模型许可条款。

插件会在运行时动态加载已安装的 `NVVideoEffects.dll` 和 `NVCVImage.dll`。
