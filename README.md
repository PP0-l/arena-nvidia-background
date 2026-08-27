# NVIDIA Background for Resolume Arena

`NVIDIA Background` is a 64-bit FFGL 2 video effect for Resolume Arena. It uses
the NVIDIA Video Effects SDK 0.7.6 AI Green Screen feature to produce a
premultiplied RGBA foreground with a transparent background.

The plug-in performs its OpenGL/CUDA bridge and mask processing on the GPU. The
same plug-in DLL contains CUDA code for RTX 20, 30, 40, and 50 series GPUs. Each
computer must install the NVIDIA Video Effects runtime and models that match its
GPU architecture.

## Features

- Quality and performance inference modes
- Person-only and person-plus-chair segmentation
- Threshold, softness, feather, mix, and mask inversion controls
- Mask preview and NVIDIA temporal-state reset
- GPU edge-color cleanup
- Premultiplied alpha output for direct Arena compositing
- CUDA targets `sm_75`, `sm_86`, `sm_89`, and `sm_120`

## Requirements

- 64-bit Windows 10 or Windows 11
- Resolume Arena 7.3.1 or newer
- Supported NVIDIA RTX GPU with a current NVIDIA display driver
- NVIDIA Video Effects SDK runtime 0.7.6 and matching models
- Visual Studio 2022 with Desktop development with C++
- CMake 3.24 or newer
- CUDA Toolkit 12.8 or newer

The NVIDIA runtime and models are not included in this repository. Download the
architecture-specific Video Effects 0.7.6 redistributable from the
[NVIDIA Broadcast SDK resources page](https://www.nvidia.com/en-us/geforce/broadcasting/broadcast-sdk/resources/).

See [GPU compatibility](docs/GPU-COMPATIBILITY.md) for the runtime mapping.

## Install a release

Copy `NVIDIA Background.dll` to:

```text
%USERPROFILE%\Documents\Resolume Arena\Extra Effects
```

Restart Arena, locate `NVIDIA Background` in the Effects panel, and drag it onto
a camera or video clip. Put another clip or layer below it to verify the alpha
output.

## Clone and build

Clone the repository together with the pinned FFGL dependency:

```powershell
git clone --recurse-submodules <repository-url>
cd arena-nvidia-background
```

Configure and build with the supplied presets:

```powershell
cmake --preset windows-vs2022 -DCUDAToolkit_ROOT="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8"
cmake --build --preset windows-release
```

The release plug-in is generated at:

```text
build\Release\NVIDIA Background.dll
```

Run the registered smoke tests on a computer with a matching NVIDIA runtime:

```powershell
ctest --preset windows-release
```

If FFGL is missing after cloning, initialize it with:

```powershell
git submodule update --init --recursive
```

## Parameters

| Parameter | Description |
| --- | --- |
| Mode | Selects quality/performance and person/chair handling. |
| Threshold | Foreground/background confidence cutoff. |
| Softness | Width of the soft alpha transition. |
| Feather | GPU mask smoothing radius. |
| Mix | Blends between the source alpha and generated alpha. |
| Invert | Inverts the generated mask. |
| View Mask | Displays the processed matte. |
| Reset State | Resets NVIDIA temporal tracking after an input change. |
| Edge Cleanup | Pulls edge color from inside the person to reduce background contamination. |

For runtime and model errors, see [Troubleshooting](docs/TROUBLESHOOTING.md).

## Repository contents

- `src/` — FFGL plug-in, NVIDIA API loader, and CUDA kernels
- `tests/` — NVIDIA runtime, raw-sequence, and OpenGL smoke tests
- `third_party/ffgl/` — pinned Resolume FFGL Git submodule
- `docs/` — GPU compatibility and troubleshooting documentation

Build directories, release binaries, analysis frames, and test videos are
intentionally excluded from source control.

## Licensing and trademarks

This project is released under the [MIT License](LICENSE). Third-party
components and notices are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

NVIDIA, CUDA, and related names are trademarks of NVIDIA Corporation. Resolume
and Arena are trademarks of Resolume B.V. This independent project is not
affiliated with or endorsed by NVIDIA Corporation or Resolume B.V.
