#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#if !defined(_WIN32)
#error NVIDIA Video Effects 0.7.6 loader is implemented for Windows only.
#endif

#include <windows.h>

enum NvCV_Status : int
{
    NVCV_SUCCESS = 0,
    NVCV_ERR_GENERAL = -1,
    NVCV_ERR_UNIMPLEMENTED = -2,
    NVCV_ERR_MEMORY = -3,
    NVCV_ERR_EFFECT = -4,
    NVCV_ERR_SELECTOR = -5,
    NVCV_ERR_BUFFER = -6,
    NVCV_ERR_PARAMETER = -7,
    NVCV_ERR_MISMATCH = -8,
    NVCV_ERR_PIXELFORMAT = -9,
    NVCV_ERR_MODEL = -10,
    NVCV_ERR_LIBRARY = -11,
    NVCV_ERR_INITIALIZATION = -12,
    NVCV_ERR_FILE = -13,
    NVCV_ERR_FEATURENOTFOUND = -14
};

enum NvCVImage_PixelFormat : int
{
    NVCV_FORMAT_UNKNOWN = 0,
    NVCV_Y = 1,
    NVCV_A = 2,
    NVCV_YA = 3,
    NVCV_RGB = 4,
    NVCV_BGR = 5,
    NVCV_RGBA = 6,
    NVCV_BGRA = 7
};

enum NvCVImage_ComponentType : int
{
    NVCV_TYPE_UNKNOWN = 0,
    NVCV_U8 = 1,
    NVCV_U16 = 2,
    NVCV_S16 = 3,
    NVCV_F16 = 4,
    NVCV_U32 = 5,
    NVCV_S32 = 6,
    NVCV_F32 = 7,
    NVCV_U64 = 8,
    NVCV_S64 = 9,
    NVCV_F64 = 10
};

constexpr unsigned NVCV_CHUNKY = 0;
constexpr unsigned NVCV_CPU = 0;
constexpr unsigned NVCV_GPU = 1;
constexpr unsigned NVCV_CPU_PINNED = 2;

struct NvCVImage
{
    unsigned int width;
    unsigned int height;
    signed int pitch;
    NvCVImage_PixelFormat pixelFormat;
    NvCVImage_ComponentType componentType;
    unsigned char pixelBytes;
    unsigned char componentBytes;
    unsigned char numComponents;
    unsigned char planar;
    unsigned char gpuMem;
    unsigned char colorspace;
    unsigned char reserved[2];
    void* pixels;
    void* deletePtr;
    void (*deleteProc)(void* p);
    unsigned long long bufferBytes;
};

static_assert(sizeof(NvCVImage) == 64, "NvCVImage ABI mismatch");

using NvVFX_Handle = void*;
using NvVFX_StateObjectHandle = void*;
using NvVFX_CudaStream = void*;

class NvidiaVfxApi
{
public:
    using CreateEffectFn = NvCV_Status (*)(const char*, NvVFX_Handle*);
    using DestroyEffectFn = NvCV_Status (*)(NvVFX_Handle);
    using SetStringFn = NvCV_Status (*)(NvVFX_Handle, const char*, const char*);
    using SetU32Fn = NvCV_Status (*)(NvVFX_Handle, const char*, unsigned int);
    using SetCudaStreamFn = NvCV_Status (*)(NvVFX_Handle, const char*, NvVFX_CudaStream);
    using SetImageFn = NvCV_Status (*)(NvVFX_Handle, const char*, NvCVImage*);
    using LoadEffectFn = NvCV_Status (*)(NvVFX_Handle);
    using RunEffectFn = NvCV_Status (*)(NvVFX_Handle, int);
    using AllocateStateFn = NvCV_Status (*)(NvVFX_Handle, NvVFX_StateObjectHandle*);
    using DeallocateStateFn = NvCV_Status (*)(NvVFX_Handle, NvVFX_StateObjectHandle);
    using ResetStateFn = NvCV_Status (*)(NvVFX_Handle, NvVFX_StateObjectHandle);
    using SetStateArrayFn = NvCV_Status (*)(NvVFX_Handle, const char*, NvVFX_StateObjectHandle*);
    using GetVersionFn = NvCV_Status (*)(unsigned int*);

    using ImageAllocFn = NvCV_Status (*)(NvCVImage*, unsigned int, unsigned int,
                                          NvCVImage_PixelFormat, NvCVImage_ComponentType,
                                          unsigned int, unsigned int, unsigned int);
    using ImageDeallocFn = void (*)(NvCVImage*);
    using ErrorStringFn = const char* (*)(NvCV_Status);

    NvidiaVfxApi() = default;
    ~NvidiaVfxApi();
    NvidiaVfxApi(const NvidiaVfxApi&) = delete;
    NvidiaVfxApi& operator=(const NvidiaVfxApi&) = delete;

    bool Load();
    void Unload();
    bool IsLoaded() const { return videoEffectsModule_ != nullptr && cvImageModule_ != nullptr; }

    const std::filesystem::path& RuntimeDirectory() const { return runtimeDirectory_; }
    std::filesystem::path ModelDirectory() const { return runtimeDirectory_ / L"models"; }
    const std::string& LastError() const { return lastError_; }
    std::string StatusString(NvCV_Status status) const;

    CreateEffectFn CreateEffect = nullptr;
    DestroyEffectFn DestroyEffect = nullptr;
    SetStringFn SetString = nullptr;
    SetU32Fn SetU32 = nullptr;
    SetCudaStreamFn SetCudaStream = nullptr;
    SetImageFn SetImage = nullptr;
    LoadEffectFn LoadEffect = nullptr;
    RunEffectFn RunEffect = nullptr;
    AllocateStateFn AllocateState = nullptr;
    DeallocateStateFn DeallocateState = nullptr;
    ResetStateFn ResetState = nullptr;
    SetStateArrayFn SetStateArray = nullptr;
    GetVersionFn GetVersion = nullptr;

    ImageAllocFn ImageAlloc = nullptr;
    ImageDeallocFn ImageDealloc = nullptr;
    ErrorStringFn ErrorString = nullptr;

private:
    template<typename T>
    bool LoadSymbol(HMODULE module, const char* name, T& target);

    std::filesystem::path FindRuntimeDirectory() const;
    void SetWindowsError(const char* operation);

    HMODULE videoEffectsModule_ = nullptr;
    HMODULE cvImageModule_ = nullptr;
    std::filesystem::path runtimeDirectory_;
    std::string lastError_;
};

