# GPU compatibility

The plug-in DLL contains CUDA machine code for four RTX generations. The
NVIDIA Video Effects SDK 0.7.6 runtime and model package installed on the target
computer must match the GPU architecture.

| GPU generation | CUDA target | Required NVIDIA package |
| --- | --- | --- |
| GeForce RTX 20 / Quadro RTX | `sm_75` | Video Effects 0.7.6 for RTX 20 / Turing |
| GeForce RTX 30 / RTX Ampere | `sm_86` | Video Effects 0.7.6 for RTX 30 / Ampere |
| GeForce RTX 40 / RTX Ada | `sm_89` | Video Effects 0.7.6 for RTX 40 / Ada |
| GeForce RTX 50 / Blackwell | `sm_120` | Video Effects 0.7.6 for RTX 50 / Blackwell |

The runtime normally installs to:

```text
C:\Program Files\NVIDIA Corporation\NVIDIA Video Effects
```

Set `NV_VIDEO_EFFECTS_PATH` before starting Arena to override the runtime
directory.

GTX, AMD, Intel, and NVIDIA GPUs without the required Tensor Core support are
not supported.
