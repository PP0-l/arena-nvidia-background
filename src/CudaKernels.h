#pragma once

#include <cuda_runtime_api.h>

cudaError_t LaunchRgbaTextureToBgr(cudaArray_t rgbaArray,
                                   unsigned char* bgrPixels,
                                   int bgrPitch,
                                   unsigned int width,
                                   unsigned int height,
                                   bool flipVertically,
                                   cudaStream_t stream);

cudaError_t LaunchMaskToTexture(const unsigned char* maskPixels,
                                int maskPitch,
                                cudaArray_t maskArray,
                                unsigned int width,
                                unsigned int height,
                                bool flipVertically,
                                cudaStream_t stream);

