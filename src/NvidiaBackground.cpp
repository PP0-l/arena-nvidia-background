#include "NvidiaBackground.h"

#include "CudaKernels.h"

#include <cuda_gl_interop.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <sstream>

using namespace ffglex;

namespace
{
enum ParameterId : FFUInt32
{
    PARAM_MODE,
    PARAM_THRESHOLD,
    PARAM_SOFTNESS,
    PARAM_FEATHER,
    PARAM_MIX,
    PARAM_INVERT,
    PARAM_VIEW_MASK,
    PARAM_RESET_STATE,
    PARAM_EDGE_CLEANUP
};

int SelectedMode(float value)
{
    return std::clamp(static_cast<int>(value + 0.5f), 0, 3);
}

CFFGLPluginInfo PluginInfo(
    PluginFactory<NvidiaBackground>,
    "NVB1",
    "NVIDIA Background",
    2,
    1,
    0,
    100,
    FF_EFFECT,
    "NVIDIA Video Effects 0.7.6 AI background removal with alpha output",
    "FFGL effect for Resolume Arena"
);

const char* kVertexShader = R"GLSL(#version 410 core
uniform vec2 MaxUV;

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec2 vUV;

out vec2 inputUV;
out vec2 maskUV;

void main()
{
    gl_Position = vPosition;
    inputUV = vUV * MaxUV;
    maskUV = vUV;
}
)GLSL";

const char* kFragmentShader = R"GLSL(#version 410 core
uniform sampler2D InputTexture;
uniform sampler2D MaskTexture;
uniform vec2 MaxUV;
uniform vec2 MaskTexel;
uniform float Threshold;
uniform float Softness;
uniform float Feather;
uniform float MixAmount;
uniform float EdgeCleanup;
uniform bool InvertMask;
uniform bool ViewMask;

in vec2 inputUV;
in vec2 maskUV;
out vec4 fragColor;

float filteredMask(vec2 uv)
{
    float radius = Feather * 8.0;
    vec2 d = MaskTexel * radius;
    float value = texture(MaskTexture, uv).r * 0.28;
    value += texture(MaskTexture, uv + vec2( d.x, 0.0)).r * 0.12;
    value += texture(MaskTexture, uv + vec2(-d.x, 0.0)).r * 0.12;
    value += texture(MaskTexture, uv + vec2(0.0,  d.y)).r * 0.12;
    value += texture(MaskTexture, uv + vec2(0.0, -d.y)).r * 0.12;
    value += texture(MaskTexture, uv + vec2( d.x,  d.y)).r * 0.06;
    value += texture(MaskTexture, uv + vec2(-d.x,  d.y)).r * 0.06;
    value += texture(MaskTexture, uv + vec2( d.x, -d.y)).r * 0.06;
    value += texture(MaskTexture, uv + vec2(-d.x, -d.y)).r * 0.06;
    return value;
}

void main()
{
    vec4 source = texture(InputTexture, inputUV);
    float rawMask = filteredMask(maskUV);
    float halfSoftness = max(Softness * 0.5, 0.0005);
    float alpha = smoothstep(Threshold - halfSoftness,
                             Threshold + halfSoftness,
                             rawMask);
    if (InvertMask)
        alpha = 1.0 - alpha;

    if (ViewMask)
    {
        fragColor = vec4(vec3(alpha), 1.0);
        return;
    }

    float outputAlpha = source.a * mix(1.0, alpha, MixAmount);
    vec3 straightColor = source.a > 0.00001 ? source.rgb / source.a : vec3(0.0);

    if (EdgeCleanup > 0.0001 && alpha > 0.001)
    {
        vec2 dx = vec2(MaskTexel.x, 0.0);
        vec2 dy = vec2(0.0, MaskTexel.y);
        vec2 gradient = vec2(
            filteredMask(maskUV + dx) - filteredMask(maskUV - dx),
            filteredMask(maskUV + dy) - filteredMask(maskUV - dy));
        float gradientLength = length(gradient);
        if (gradientLength > 0.00001)
        {
            vec2 inward = gradient / gradientLength;
            float reach = mix(1.0, 4.0, EdgeCleanup);
            vec2 innerMaskUV = clamp(maskUV + inward * MaskTexel * reach,
                                     vec2(0.0), vec2(1.0));
            vec2 innerInputUV = clamp(inputUV + inward * MaskTexel * MaxUV * reach,
                                      vec2(0.0), MaxUV);
            float innerMask = filteredMask(innerMaskUV);
            vec4 innerSource = texture(InputTexture, innerInputUV);
            vec3 innerColor = innerSource.a > 0.00001
                ? innerSource.rgb / innerSource.a
                : straightColor;
            float inwardConfidence = smoothstep(rawMask + 0.02,
                                                rawMask + 0.25,
                                                innerMask);
            float edgeBand = smoothstep(0.01, 0.35, alpha) *
                             (1.0 - smoothstep(0.78, 1.0, rawMask));
            straightColor = mix(straightColor, innerColor,
                                EdgeCleanup * edgeBand * inwardConfidence);
        }
    }

    fragColor = vec4(straightColor * outputAlpha, outputAlpha);
}
)GLSL";

std::string PathToUtf8(const std::filesystem::path& path)
{
    return path.u8string();
}
}

NvidiaBackground::NvidiaBackground()
    : CFFGLPlugin(true)
{
    SetMinInputs(1);
    SetMaxInputs(1);

    SetOptionParamInfo(PARAM_MODE, "Mode", 4, mode_);
    SetParamElementInfo(PARAM_MODE, 0, "Quality + Chairs", 0.0f);
    SetParamElementInfo(PARAM_MODE, 1, "Performance + Chairs", 1.0f);
    SetParamElementInfo(PARAM_MODE, 2, "Quality Person Only", 2.0f);
    SetParamElementInfo(PARAM_MODE, 3, "Performance Person Only", 3.0f);
    SetParamInfo(PARAM_THRESHOLD, "Threshold", FF_TYPE_STANDARD, threshold_);
    SetParamInfo(PARAM_SOFTNESS, "Softness", FF_TYPE_STANDARD, softness_);
    SetParamInfo(PARAM_FEATHER, "Feather", FF_TYPE_STANDARD, feather_);
    SetParamInfo(PARAM_MIX, "Mix", FF_TYPE_STANDARD, mix_);
    SetParamInfo(PARAM_INVERT, "Invert", FF_TYPE_BOOLEAN, false);
    SetParamInfo(PARAM_VIEW_MASK, "View Mask", FF_TYPE_BOOLEAN, false);
    SetParamInfo(PARAM_RESET_STATE, "Reset State", FF_TYPE_EVENT, false);
    SetParamInfo(PARAM_EDGE_CLEANUP, "Edge Cleanup", FF_TYPE_STANDARD, edgeCleanup_);
}

NvidiaBackground::~NvidiaBackground() = default;

FFResult NvidiaBackground::InitGL(const FFGLViewportStruct* viewport)
{
    if (!shader_.Compile(kVertexShader, kFragmentShader) || !quad_.Initialise())
    {
        DeInitGL();
        return FF_FAIL;
    }

    glGenTextures(1, &whiteMaskTexture_);
    glBindTexture(GL_TEXTURE_2D, whiteMaskTexture_);
    const unsigned char white = 255;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!vfx_.Load())
        LogErrorOnce("NVIDIA VFX SDK: " + vfx_.LastError());
    else
        cudaReady_ = InitialiseCudaForCurrentOpenGLContext();

    return CFFGLPlugin::InitGL(viewport);
}

bool NvidiaBackground::InitialiseCudaForCurrentOpenGLContext()
{
    std::array<int, 8> devices{};
    unsigned int count = 0;
    cudaError_t error = cudaGLGetDevices(&count, devices.data(),
                                         static_cast<unsigned int>(devices.size()),
                                         cudaGLDeviceListAll);
    if (error != cudaSuccess || count == 0)
    {
        LogCudaError("cudaGLGetDevices", error != cudaSuccess ? error : cudaErrorNoDevice);
        return false;
    }

    error = cudaSetDevice(devices[0]);
    if (error != cudaSuccess)
    {
        LogCudaError("cudaSetDevice", error);
        return false;
    }

    unsigned int version = 0;
    if (vfx_.GetVersion != nullptr && vfx_.GetVersion(&version) == NVCV_SUCCESS)
    {
        std::ostringstream message;
        message << "NVIDIA Background: Video Effects SDK loaded (version 0x"
                << std::hex << version << ")";
        FFGLLog::LogToHost(message.str().c_str());
    }
    return true;
}

bool NvidiaBackground::EnsurePipeline(unsigned int width, unsigned int height)
{
    const int requestedMode = SelectedMode(mode_);
    if (pipelineReady_ && !rebuildRequested_ && processingWidth_ == width &&
        processingHeight_ == height && activeMode_ == requestedMode)
        return true;

    rebuildRequested_ = false;
    return CreatePipeline(width, height);
}

bool NvidiaBackground::CreatePipeline(unsigned int width, unsigned int height)
{
    ReleasePipeline();
    if (!cudaReady_ || !vfx_.IsLoaded())
        return false;

    processingWidth_ = width;
    processingHeight_ = height;
    activeMode_ = SelectedMode(mode_);

    glGenFramebuffers(1, &readFramebuffer_);
    glGenFramebuffers(1, &bridgeFramebuffer_);

    glGenTextures(1, &bridgeTexture_);
    glBindTexture(GL_TEXTURE_2D, bridgeTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &maskTexture_);
    glBindTexture(GL_TEXTURE_2D, maskTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLint oldDrawFramebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bridgeFramebuffer_);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, bridgeTexture_, 0);
    const GLenum framebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(oldDrawFramebuffer));
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        LogErrorOnce("NVIDIA Background: failed to create the OpenGL bridge framebuffer");
        ReleasePipeline();
        return false;
    }

    cudaError_t cudaError = cudaGraphicsGLRegisterImage(
        &bridgeCudaResource_, bridgeTexture_, GL_TEXTURE_2D,
        cudaGraphicsRegisterFlagsReadOnly);
    if (cudaError != cudaSuccess)
    {
        LogCudaError("cudaGraphicsGLRegisterImage(input)", cudaError);
        ReleasePipeline();
        return false;
    }
    cudaError = cudaGraphicsGLRegisterImage(
        &maskCudaResource_, maskTexture_, GL_TEXTURE_2D,
        cudaGraphicsRegisterFlagsSurfaceLoadStore | cudaGraphicsRegisterFlagsWriteDiscard);
    if (cudaError != cudaSuccess)
    {
        LogCudaError("cudaGraphicsGLRegisterImage(mask)", cudaError);
        ReleasePipeline();
        return false;
    }

    cudaError = cudaStreamCreateWithFlags(&stream_, cudaStreamNonBlocking);
    if (cudaError != cudaSuccess)
    {
        LogCudaError("cudaStreamCreateWithFlags", cudaError);
        ReleasePipeline();
        return false;
    }

    NvCV_Status status = vfx_.CreateEffect("GreenScreen", &effect_);
    if (status != NVCV_SUCCESS)
    {
        LogNvError("NvVFX_CreateEffect", status);
        ReleasePipeline();
        return false;
    }

    const std::string modelDirectory = PathToUtf8(vfx_.ModelDirectory());
    auto setU32 = [this](const char* selector, unsigned int value) {
        return vfx_.SetU32(effect_, selector, value);
    };
    status = vfx_.SetString(effect_, "ModelDir", modelDirectory.c_str());
    if (status == NVCV_SUCCESS)
        status = vfx_.SetCudaStream(effect_, "CudaStream", reinterpret_cast<void*>(stream_));
    if (status == NVCV_SUCCESS)
        status = setU32("Mode", static_cast<unsigned int>(activeMode_));
    if (status == NVCV_SUCCESS)
        status = setU32("CudaGraph", 1);
    if (status == NVCV_SUCCESS)
        status = setU32("MaxInputWidth", width);
    if (status == NVCV_SUCCESS)
        status = setU32("MaxInputHeight", height);
    if (status == NVCV_SUCCESS)
        status = setU32("MaxNumberStreams", 1);
    if (status != NVCV_SUCCESS)
    {
        LogNvError("configure NVIDIA effect", status);
        ReleasePipeline();
        return false;
    }

    status = vfx_.LoadEffect(effect_);
    if (status != NVCV_SUCCESS)
    {
        LogNvError("NvVFX_Load", status);
        ReleasePipeline();
        return false;
    }

    status = vfx_.AllocateState(effect_, &state_);
    if (status != NVCV_SUCCESS)
    {
        LogNvError("NvVFX_AllocateState", status);
        ReleasePipeline();
        return false;
    }

    status = vfx_.ImageAlloc(&inputImage_, width, height, NVCV_BGR, NVCV_U8,
                             NVCV_CHUNKY, NVCV_GPU, 1);
    if (status == NVCV_SUCCESS)
        status = vfx_.ImageAlloc(&maskImage_, width, height, NVCV_A, NVCV_U8,
                                 NVCV_CHUNKY, NVCV_GPU, 1);
    if (status == NVCV_SUCCESS)
        status = vfx_.SetImage(effect_, "SrcImage0", &inputImage_);
    if (status == NVCV_SUCCESS)
        status = vfx_.SetImage(effect_, "DstImage0", &maskImage_);
    if (status != NVCV_SUCCESS)
    {
        LogNvError("allocate NVIDIA image buffers", status);
        ReleasePipeline();
        return false;
    }

    pipelineReady_ = true;
    lastLoggedError_.clear();
    FFGLLog::LogToHost("NVIDIA Background: GPU pipeline ready");
    return true;
}

void NvidiaBackground::ReleasePipeline()
{
    pipelineReady_ = false;

    if (stream_ != nullptr)
        cudaStreamSynchronize(stream_);

    if (bridgeCudaResource_ != nullptr)
    {
        cudaGraphicsUnregisterResource(bridgeCudaResource_);
        bridgeCudaResource_ = nullptr;
    }
    if (maskCudaResource_ != nullptr)
    {
        cudaGraphicsUnregisterResource(maskCudaResource_);
        maskCudaResource_ = nullptr;
    }

    if (state_ != nullptr && effect_ != nullptr && vfx_.DeallocateState != nullptr)
    {
        vfx_.DeallocateState(effect_, state_);
        state_ = nullptr;
    }
    if (effect_ != nullptr && vfx_.DestroyEffect != nullptr)
    {
        vfx_.DestroyEffect(effect_);
        effect_ = nullptr;
    }

    if (inputImage_.pixels != nullptr && vfx_.ImageDealloc != nullptr)
        vfx_.ImageDealloc(&inputImage_);
    if (maskImage_.pixels != nullptr && vfx_.ImageDealloc != nullptr)
        vfx_.ImageDealloc(&maskImage_);
    inputImage_ = {};
    maskImage_ = {};

    if (stream_ != nullptr)
    {
        cudaStreamDestroy(stream_);
        stream_ = nullptr;
    }

    if (bridgeTexture_ != 0)
    {
        glDeleteTextures(1, &bridgeTexture_);
        bridgeTexture_ = 0;
    }
    if (maskTexture_ != 0)
    {
        glDeleteTextures(1, &maskTexture_);
        maskTexture_ = 0;
    }
    if (readFramebuffer_ != 0)
    {
        glDeleteFramebuffers(1, &readFramebuffer_);
        readFramebuffer_ = 0;
    }
    if (bridgeFramebuffer_ != 0)
    {
        glDeleteFramebuffers(1, &bridgeFramebuffer_);
        bridgeFramebuffer_ = 0;
    }

    processingWidth_ = 0;
    processingHeight_ = 0;
    activeMode_ = -1;
}

bool NvidiaBackground::CopyInputToBridge(const FFGLTextureStruct& input)
{
    GLint oldReadFramebuffer = 0;
    GLint oldDrawFramebuffer = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFramebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFramebuffer);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer_);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, input.Handle, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const GLenum inputStatus = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bridgeFramebuffer_);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if (inputStatus == GL_FRAMEBUFFER_COMPLETE)
    {
        glBlitFramebuffer(0, 0, static_cast<GLint>(input.Width), static_cast<GLint>(input.Height),
                          0, 0, static_cast<GLint>(processingWidth_),
                          static_cast<GLint>(processingHeight_),
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(oldReadFramebuffer));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(oldDrawFramebuffer));

    if (inputStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        LogErrorOnce("NVIDIA Background: the Arena input texture could not be attached to an FBO");
        return false;
    }
    return glGetError() == GL_NO_ERROR;
}

bool NvidiaBackground::RunSegmentation()
{
    cudaGraphicsResource* resources[] = {bridgeCudaResource_, maskCudaResource_};
    cudaError_t cudaStatus = cudaGraphicsMapResources(2, resources, stream_);
    if (cudaStatus != cudaSuccess)
    {
        LogCudaError("cudaGraphicsMapResources", cudaStatus);
        return false;
    }

    bool success = true;
    cudaArray_t inputArray = nullptr;
    cudaArray_t outputArray = nullptr;
    cudaStatus = cudaGraphicsSubResourceGetMappedArray(&inputArray, bridgeCudaResource_, 0, 0);
    if (cudaStatus == cudaSuccess)
        cudaStatus = cudaGraphicsSubResourceGetMappedArray(&outputArray, maskCudaResource_, 0, 0);
    if (cudaStatus != cudaSuccess)
    {
        LogCudaError("cudaGraphicsSubResourceGetMappedArray", cudaStatus);
        success = false;
    }

    const bool flip = GetTextureOrientation() == TextureOrientation::BOTTOM_LEFT;
    if (success)
    {
        cudaStatus = LaunchRgbaTextureToBgr(
            inputArray, static_cast<unsigned char*>(inputImage_.pixels), inputImage_.pitch,
            processingWidth_, processingHeight_, flip, stream_);
        if (cudaStatus != cudaSuccess)
        {
            LogCudaError("RGBA to BGR kernel", cudaStatus);
            success = false;
        }
    }

    if (success && resetRequested_)
    {
        const NvCV_Status status = vfx_.ResetState(effect_, state_);
        resetRequested_ = false;
        if (status != NVCV_SUCCESS)
        {
            LogNvError("NvVFX_ResetState", status);
            success = false;
        }
    }

    NvVFX_StateObjectHandle states[] = {state_};
    if (success)
    {
        NvCV_Status status = vfx_.SetStateArray(effect_, "State", states);
        if (status == NVCV_SUCCESS)
            status = vfx_.RunEffect(effect_, 1);
        if (status != NVCV_SUCCESS)
        {
            LogNvError("NvVFX_Run", status);
            success = false;
        }
    }

    if (success)
    {
        cudaStatus = LaunchMaskToTexture(
            static_cast<const unsigned char*>(maskImage_.pixels), maskImage_.pitch,
            outputArray, processingWidth_, processingHeight_, flip, stream_);
        if (cudaStatus == cudaSuccess)
            cudaStatus = cudaStreamSynchronize(stream_);
        if (cudaStatus != cudaSuccess)
        {
            LogCudaError("mask output kernel", cudaStatus);
            success = false;
        }
    }

    const cudaError_t unmapStatus = cudaGraphicsUnmapResources(2, resources, stream_);
    if (unmapStatus != cudaSuccess)
    {
        LogCudaError("cudaGraphicsUnmapResources", unmapStatus);
        success = false;
    }
    return success;
}

void NvidiaBackground::RenderResult(const FFGLTextureStruct& input,
                                    GLuint maskTexture,
                                    bool maskIsValid)
{
    ScopedShaderBinding shaderBinding(shader_.GetGLID());
    ScopedSamplerActivation inputSampler(0);
    Scoped2DTextureBinding inputBinding(input.Handle);
    ScopedSamplerActivation maskSampler(1);
    Scoped2DTextureBinding maskBinding(maskTexture);

    shader_.Set("InputTexture", 0);
    shader_.Set("MaskTexture", 1);
    const FFGLTexCoords maxCoords = GetMaxGLTexCoords(input);
    shader_.Set("MaxUV", maxCoords.s, maxCoords.t);
    shader_.Set("MaskTexel",
                maskIsValid ? 1.0f / static_cast<float>(processingWidth_) : 1.0f,
                maskIsValid ? 1.0f / static_cast<float>(processingHeight_) : 1.0f);
    shader_.Set("Threshold", threshold_);
    shader_.Set("Softness", softness_);
    shader_.Set("Feather", feather_);
    shader_.Set("MixAmount", mix_);
    shader_.Set("EdgeCleanup", edgeCleanup_);
    shader_.Set("InvertMask", invert_ >= 0.5f);
    shader_.Set("ViewMask", viewMask_ >= 0.5f && maskIsValid);
    quad_.Draw();
}

FFResult NvidiaBackground::ProcessOpenGL(ProcessOpenGLStruct* process)
{
    if (process == nullptr || process->numInputTextures < 1 ||
        process->inputTextures[0] == nullptr)
        return FF_FAIL;

    const FFGLTextureStruct& input = *process->inputTextures[0];
    const unsigned int width = std::max(512u, static_cast<unsigned int>(input.Width));
    const unsigned int height = std::max(288u, static_cast<unsigned int>(input.Height));

    bool maskIsValid = false;
    if (EnsurePipeline(width, height) && CopyInputToBridge(input))
        maskIsValid = RunSegmentation();

    RenderResult(input, maskIsValid ? maskTexture_ : whiteMaskTexture_, maskIsValid);
    return FF_SUCCESS;
}

FFResult NvidiaBackground::DeInitGL()
{
    ReleasePipeline();
    cudaReady_ = false;
    vfx_.Unload();

    if (whiteMaskTexture_ != 0)
    {
        glDeleteTextures(1, &whiteMaskTexture_);
        whiteMaskTexture_ = 0;
    }
    shader_.FreeGLResources();
    quad_.Release();
    return FF_SUCCESS;
}

FFResult NvidiaBackground::SetFloatParameter(unsigned int index, float value)
{
    switch (index)
    {
    case PARAM_MODE:
        if (SelectedMode(mode_) != SelectedMode(value))
            rebuildRequested_ = true;
        mode_ = value;
        break;
    case PARAM_THRESHOLD: threshold_ = value; break;
    case PARAM_SOFTNESS: softness_ = value; break;
    case PARAM_FEATHER: feather_ = value; break;
    case PARAM_MIX: mix_ = value; break;
    case PARAM_INVERT: invert_ = value; break;
    case PARAM_VIEW_MASK: viewMask_ = value; break;
    case PARAM_RESET_STATE:
        resetState_ = value;
        if (value != 0.0f)
            resetRequested_ = true;
        break;
    case PARAM_EDGE_CLEANUP: edgeCleanup_ = value; break;
    default:
        return FF_FAIL;
    }
    return FF_SUCCESS;
}

float NvidiaBackground::GetFloatParameter(unsigned int index)
{
    switch (index)
    {
    case PARAM_MODE: return mode_;
    case PARAM_THRESHOLD: return threshold_;
    case PARAM_SOFTNESS: return softness_;
    case PARAM_FEATHER: return feather_;
    case PARAM_MIX: return mix_;
    case PARAM_INVERT: return invert_;
    case PARAM_VIEW_MASK: return viewMask_;
    case PARAM_RESET_STATE: return resetState_;
    case PARAM_EDGE_CLEANUP: return edgeCleanup_;
    default: return 0.0f;
    }
}

void NvidiaBackground::LogErrorOnce(const std::string& message)
{
    if (message == lastLoggedError_)
        return;
    lastLoggedError_ = message;
    FFGLLog::LogToHost(message.c_str());
    OutputDebugStringA((message + "\n").c_str());
}

void NvidiaBackground::LogNvError(const char* stage, NvCV_Status status)
{
    LogErrorOnce(std::string("NVIDIA Background: ") + stage + " failed: " +
                 vfx_.StatusString(status));
}

void NvidiaBackground::LogCudaError(const char* stage, cudaError_t status)
{
    LogErrorOnce(std::string("NVIDIA Background: ") + stage + " failed: " +
                 cudaGetErrorString(status));
}
