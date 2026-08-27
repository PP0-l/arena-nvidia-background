#include "NvidiaVfxApi.h"

#include <cuda_runtime_api.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
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

int main(int argc, char** argv)
{
    if (argc != 5 && argc != 6)
    {
        std::cerr << "Usage: NvidiaVfxRawSequenceTest input.bgr width height output.gray [mode]\n";
        return 1;
    }

    const unsigned int width = static_cast<unsigned int>(std::stoul(argv[2]));
    const unsigned int height = static_cast<unsigned int>(std::stoul(argv[3]));
    const unsigned int mode = argc == 6 ? static_cast<unsigned int>(std::stoul(argv[5])) : 0u;
    const size_t inputFrameBytes = static_cast<size_t>(width) * height * 3u;
    const size_t maskFrameBytes = static_cast<size_t>(width) * height;

    std::ifstream inputFile(argv[1], std::ios::binary);
    std::ofstream outputFile(argv[4], std::ios::binary | std::ios::trunc);
    if (!inputFile || !outputFile)
    {
        std::cerr << "Unable to open input or output file.\n";
        return 2;
    }

    inputFile.seekg(0, std::ios::end);
    const auto inputBytes = static_cast<size_t>(inputFile.tellg());
    inputFile.seekg(0, std::ios::beg);
    const size_t frameCount = inputBytes / inputFrameBytes;
    if (frameCount == 0 || inputBytes % inputFrameBytes != 0)
    {
        std::cerr << "Input size is not an exact number of BGR frames.\n";
        return 3;
    }

    NvidiaVfxApi api;
    if (!api.Load())
    {
        std::cerr << api.LastError() << '\n';
        return 4;
    }

    cudaStream_t stream = nullptr;
    NvVFX_Handle effect = nullptr;
    NvVFX_StateObjectHandle state = nullptr;
    NvVFX_StateObjectHandle states[1]{};
    NvCVImage inputImage{};
    NvCVImage maskImage{};
    std::vector<unsigned char> hostInput(inputFrameBytes);
    std::vector<unsigned char> hostMask(maskFrameBytes);
    int result = 5;

    if (!CheckCuda("cudaSetDevice", cudaSetDevice(0)) ||
        !CheckCuda("cudaStreamCreate", cudaStreamCreate(&stream)) ||
        !CheckNv(api, "NvVFX_CreateEffect", api.CreateEffect("GreenScreen", &effect)))
        goto cleanup;

    {
        const std::string modelDirectory = api.ModelDirectory().u8string();
        if (!CheckNv(api, "ModelDir", api.SetString(effect, "ModelDir", modelDirectory.c_str())) ||
            !CheckNv(api, "CudaStream", api.SetCudaStream(effect, "CudaStream", stream)) ||
            !CheckNv(api, "Mode", api.SetU32(effect, "Mode", mode)) ||
            !CheckNv(api, "CudaGraph", api.SetU32(effect, "CudaGraph", 1)) ||
            !CheckNv(api, "MaxInputWidth", api.SetU32(effect, "MaxInputWidth", width)) ||
            !CheckNv(api, "MaxInputHeight", api.SetU32(effect, "MaxInputHeight", height)) ||
            !CheckNv(api, "MaxNumberStreams", api.SetU32(effect, "MaxNumberStreams", 1)) ||
            !CheckNv(api, "NvVFX_Load", api.LoadEffect(effect)) ||
            !CheckNv(api, "NvVFX_AllocateState", api.AllocateState(effect, &state)) ||
            !CheckNv(api, "input allocation", api.ImageAlloc(&inputImage, width, height, NVCV_BGR,
                                                                 NVCV_U8, NVCV_CHUNKY, NVCV_GPU, 1)) ||
            !CheckNv(api, "mask allocation", api.ImageAlloc(&maskImage, width, height, NVCV_A,
                                                                NVCV_U8, NVCV_CHUNKY, NVCV_GPU, 1)) ||
            !CheckNv(api, "input image", api.SetImage(effect, "SrcImage0", &inputImage)) ||
            !CheckNv(api, "output image", api.SetImage(effect, "DstImage0", &maskImage)))
            goto cleanup;

        states[0] = state;
        if (!CheckNv(api, "state array", api.SetStateArray(effect, "State", states)))
            goto cleanup;
    }

    {
        const auto start = std::chrono::steady_clock::now();
        double coverageSum = 0.0;
        for (size_t frame = 0; frame < frameCount; ++frame)
        {
            inputFile.read(reinterpret_cast<char*>(hostInput.data()),
                           static_cast<std::streamsize>(hostInput.size()));
            if (!inputFile)
            {
                std::cerr << "Failed while reading frame " << frame << ".\n";
                goto cleanup;
            }

            if (!CheckCuda("upload input", cudaMemcpy2DAsync(
                    inputImage.pixels, inputImage.pitch, hostInput.data(), width * 3u,
                    width * 3u, height, cudaMemcpyHostToDevice, stream)) ||
                !CheckNv(api, "NvVFX_Run", api.RunEffect(effect, 0)) ||
                !CheckCuda("download mask", cudaMemcpy2DAsync(
                    hostMask.data(), width, maskImage.pixels, maskImage.pitch,
                    width, height, cudaMemcpyDeviceToHost, stream)) ||
                !CheckCuda("synchronize", cudaStreamSynchronize(stream)))
                goto cleanup;

            outputFile.write(reinterpret_cast<const char*>(hostMask.data()),
                             static_cast<std::streamsize>(hostMask.size()));
            if (!outputFile)
            {
                std::cerr << "Failed while writing frame " << frame << ".\n";
                goto cleanup;
            }

            double foreground = 0.0;
            for (unsigned char value : hostMask)
                foreground += static_cast<double>(value) / 255.0;
            coverageSum += foreground / static_cast<double>(hostMask.size());
        }

        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << "Processed " << frameCount << " frames in " << seconds
                  << " seconds (" << frameCount / seconds << " fps). Average foreground: "
                  << 100.0 * coverageSum / static_cast<double>(frameCount) << "%\n";
    }

    result = 0;

cleanup:
    if (stream != nullptr)
        cudaStreamSynchronize(stream);
    if (inputImage.pixels != nullptr)
        api.ImageDealloc(&inputImage);
    if (maskImage.pixels != nullptr)
        api.ImageDealloc(&maskImage);
    if (state != nullptr && effect != nullptr)
        api.DeallocateState(effect, state);
    if (effect != nullptr)
        api.DestroyEffect(effect);
    if (stream != nullptr)
        cudaStreamDestroy(stream);
    return result;
}
