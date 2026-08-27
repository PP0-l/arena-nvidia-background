# Troubleshooting

## The effect loads but the image is not keyed

Verify that the NVIDIA Video Effects runtime and model package matches the GPU
generation. A model filename ending in `_75`, `_86`, `_89`, or `_120` should
match the GPU architecture documented in `GPU-COMPATIBILITY.md`.

## The NVIDIA runtime cannot be found

The default location is:

```text
C:\Program Files\NVIDIA Corporation\NVIDIA Video Effects
```

If the runtime is installed elsewhere, set `NV_VIDEO_EFFECTS_PATH` to that
directory before starting Arena.

## CUDA/OpenGL initialization fails on a laptop

Configure Windows Graphics settings or the NVIDIA application profile so that
Resolume Arena uses the high-performance NVIDIA GPU rather than the integrated
GPU. Restart Arena after changing the selection.

## The mask tracks an old camera feed

Press `Reset State` after changing cameras, clips, or discontinuously seeking
the input.

## CMake cannot find FFGL

Run the following from the repository root:

```powershell
git submodule update --init --recursive
```

## CMake cannot find CUDA 12.8

Install CUDA Toolkit 12.8 or newer, or pass its location explicitly:

```powershell
cmake --preset windows-vs2022 -DCUDAToolkit_ROOT="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8"
```
