#include "NvidiaVfxApi.h"

#include <cuda_runtime_api.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace
{
std::string PathToUtf8(const std::filesystem::path& path)
{
    const std::wstring text = path.wstring();
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}

bool CheckNv(NvidiaVfxApi& api, const char* stage, NvCV_Status status)
{
    if (status == NVCV_SUCCESS)
        return true;
    std::cerr << stage << ": " << api.StatusString(status) << '\n';
    return false;
}

bool CheckCuda(const char* stage, cudaError_t status)
{
    if (status == cudaSuccess)
        return true;
    std::cerr << stage << ": " << cudaGetErrorString(status) << '\n';
    return false;
}
}

int main()
{
    constexpr unsigned int width = 512;
    constexpr unsigned int height = 288;

    NvidiaVfxApi api;
    if (!api.Load())
    {
        std::cerr << api.LastError() << '\n';
        return 1;
    }

    int device = 0;
    if (!CheckCuda("cudaSetDevice", cudaSetDevice(device)))
        return 2;

    cudaStream_t stream = nullptr;
    NvVFX_Handle effect = nullptr;
    NvVFX_StateObjectHandle state = nullptr;
    NvCVImage input{};
    NvCVImage mask{};
    int result = 3;

    if (!CheckCuda("cudaStreamCreate", cudaStreamCreate(&stream)))
        goto cleanup;
    if (!CheckNv(api, "NvVFX_CreateEffect", api.CreateEffect("GreenScreen", &effect)))
        goto cleanup;

    {
        const std::string modelDirectory = PathToUtf8(api.ModelDirectory());
        if (!CheckNv(api, "ModelDir", api.SetString(effect, "ModelDir", modelDirectory.c_str())) ||
            !CheckNv(api, "CudaStream", api.SetCudaStream(effect, "CudaStream", stream)) ||
            !CheckNv(api, "Mode", api.SetU32(effect, "Mode", 1)) ||
            !CheckNv(api, "CudaGraph", api.SetU32(effect, "CudaGraph", 1)) ||
            !CheckNv(api, "MaxInputWidth", api.SetU32(effect, "MaxInputWidth", width)) ||
            !CheckNv(api, "MaxInputHeight", api.SetU32(effect, "MaxInputHeight", height)) ||
            !CheckNv(api, "MaxNumberStreams", api.SetU32(effect, "MaxNumberStreams", 1)) ||
            !CheckNv(api, "NvVFX_Load", api.LoadEffect(effect)) ||
            !CheckNv(api, "NvVFX_AllocateState", api.AllocateState(effect, &state)) ||
            !CheckNv(api, "input allocation", api.ImageAlloc(&input, width, height, NVCV_BGR,
                                                               NVCV_U8, NVCV_CHUNKY, NVCV_GPU, 1)) ||
            !CheckNv(api, "mask allocation", api.ImageAlloc(&mask, width, height, NVCV_A,
                                                              NVCV_U8, NVCV_CHUNKY, NVCV_GPU, 1)) ||
            !CheckNv(api, "input image", api.SetImage(effect, "SrcImage0", &input)) ||
            !CheckNv(api, "output image", api.SetImage(effect, "DstImage0", &mask)))
            goto cleanup;
    }

    if (!CheckCuda("clear input", cudaMemset2DAsync(input.pixels, input.pitch, 0,
                                                     width * 3u, height, stream)))
        goto cleanup;

    {
        NvVFX_StateObjectHandle states[] = {state};
        if (!CheckNv(api, "state array", api.SetStateArray(effect, "State", states)) ||
            !CheckNv(api, "NvVFX_Run", api.RunEffect(effect, 0)))
            goto cleanup;
    }

    {
        std::vector<unsigned char> hostMask(static_cast<size_t>(width) * height);
        if (!CheckCuda("copy mask", cudaMemcpy2D(hostMask.data(), width, mask.pixels,
                                                 mask.pitch, width, height,
                                                 cudaMemcpyDeviceToHost)))
            goto cleanup;
        const double average = std::accumulate(hostMask.begin(), hostMask.end(), 0.0) /
                               static_cast<double>(hostMask.size());
        std::cout << "NVIDIA Video Effects 0.7.6 smoke test passed. Average mask: "
                  << average << '\n';
    }

    result = 0;

cleanup:
    if (stream != nullptr)
        cudaStreamSynchronize(stream);
    if (input.pixels != nullptr)
        api.ImageDealloc(&input);
    if (mask.pixels != nullptr)
        api.ImageDealloc(&mask);
    if (state != nullptr && effect != nullptr)
        api.DeallocateState(effect, state);
    if (effect != nullptr)
        api.DestroyEffect(effect);
    if (stream != nullptr)
        cudaStreamDestroy(stream);
    return result;
}

