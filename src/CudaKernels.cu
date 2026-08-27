#include "CudaKernels.h"

#include <cuda_runtime.h>

namespace
{
__device__ unsigned char ToByte(float value)
{
    value = fminf(fmaxf(value, 0.0f), 1.0f);
    return static_cast<unsigned char>(value * 255.0f + 0.5f);
}

__global__ void RgbaTextureToBgrKernel(cudaTextureObject_t rgbaTexture,
                                      unsigned char* bgrPixels,
                                      int bgrPitch,
                                      unsigned int width,
                                      unsigned int height,
                                      bool flipVertically)
{
    const unsigned int x = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height)
        return;

    const unsigned int sourceY = flipVertically ? (height - 1u - y) : y;
    const float4 rgba = tex2D<float4>(rgbaTexture,
                                     static_cast<float>(x) + 0.5f,
                                     static_cast<float>(sourceY) + 0.5f);
    unsigned char* pixel = bgrPixels + static_cast<size_t>(y) * bgrPitch + x * 3u;
    pixel[0] = ToByte(rgba.z);
    pixel[1] = ToByte(rgba.y);
    pixel[2] = ToByte(rgba.x);
}

__global__ void MaskToTextureKernel(const unsigned char* maskPixels,
                                    int maskPitch,
                                    cudaSurfaceObject_t maskSurface,
                                    unsigned int width,
                                    unsigned int height,
                                    bool flipVertically)
{
    const unsigned int x = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height)
        return;

    const unsigned char value = maskPixels[static_cast<size_t>(y) * maskPitch + x];
    const unsigned int destinationY = flipVertically ? (height - 1u - y) : y;
    surf2Dwrite(value, maskSurface, x * sizeof(unsigned char), destinationY);
}
}

cudaError_t LaunchRgbaTextureToBgr(cudaArray_t rgbaArray,
                                   unsigned char* bgrPixels,
                                   int bgrPitch,
                                   unsigned int width,
                                   unsigned int height,
                                   bool flipVertically,
                                   cudaStream_t stream)
{
    cudaResourceDesc resource{};
    resource.resType = cudaResourceTypeArray;
    resource.res.array.array = rgbaArray;

    cudaTextureDesc texture{};
    texture.addressMode[0] = cudaAddressModeClamp;
    texture.addressMode[1] = cudaAddressModeClamp;
    texture.filterMode = cudaFilterModePoint;
    texture.readMode = cudaReadModeNormalizedFloat;
    texture.normalizedCoords = 0;

    cudaTextureObject_t object = 0;
    cudaError_t error = cudaCreateTextureObject(&object, &resource, &texture, nullptr);
    if (error != cudaSuccess)
        return error;

    const dim3 block(16, 16);
    const dim3 grid((width + block.x - 1) / block.x,
                    (height + block.y - 1) / block.y);
    RgbaTextureToBgrKernel<<<grid, block, 0, stream>>>(
        object, bgrPixels, bgrPitch, width, height, flipVertically);
    error = cudaGetLastError();
    const cudaError_t destroyError = cudaDestroyTextureObject(object);
    return error != cudaSuccess ? error : destroyError;
}

cudaError_t LaunchMaskToTexture(const unsigned char* maskPixels,
                                int maskPitch,
                                cudaArray_t maskArray,
                                unsigned int width,
                                unsigned int height,
                                bool flipVertically,
                                cudaStream_t stream)
{
    cudaResourceDesc resource{};
    resource.resType = cudaResourceTypeArray;
    resource.res.array.array = maskArray;

    cudaSurfaceObject_t object = 0;
    cudaError_t error = cudaCreateSurfaceObject(&object, &resource);
    if (error != cudaSuccess)
        return error;

    const dim3 block(16, 16);
    const dim3 grid((width + block.x - 1) / block.x,
                    (height + block.y - 1) / block.y);
    MaskToTextureKernel<<<grid, block, 0, stream>>>(
        maskPixels, maskPitch, object, width, height, flipVertically);
    error = cudaGetLastError();
    const cudaError_t destroyError = cudaDestroySurfaceObject(object);
    return error != cudaSuccess ? error : destroyError;
}

