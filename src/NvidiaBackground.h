#pragma once

#include <FFGLSDK.h>

#include "NvidiaVfxApi.h"

#include <cuda_runtime_api.h>

#include <string>

struct cudaGraphicsResource;

class NvidiaBackground final : public CFFGLPlugin
{
public:
    NvidiaBackground();
    ~NvidiaBackground() override;

    FFResult InitGL(const FFGLViewportStruct* viewport) override;
    FFResult ProcessOpenGL(ProcessOpenGLStruct* process) override;
    FFResult DeInitGL() override;

    FFResult SetFloatParameter(unsigned int index, float value) override;
    float GetFloatParameter(unsigned int index) override;

private:
    bool InitialiseCudaForCurrentOpenGLContext();
    bool EnsurePipeline(unsigned int width, unsigned int height);
    bool CreatePipeline(unsigned int width, unsigned int height);
    void ReleasePipeline();
    bool CopyInputToBridge(const FFGLTextureStruct& input);
    bool RunSegmentation();
    void RenderResult(const FFGLTextureStruct& input, GLuint maskTexture, bool maskIsValid);
    void LogErrorOnce(const std::string& message);
    void LogNvError(const char* stage, NvCV_Status status);
    void LogCudaError(const char* stage, cudaError_t status);

    ffglex::FFGLShader shader_;
    ffglex::FFGLScreenQuad quad_;
    NvidiaVfxApi vfx_;

    GLuint readFramebuffer_ = 0;
    GLuint bridgeFramebuffer_ = 0;
    GLuint bridgeTexture_ = 0;
    GLuint maskTexture_ = 0;
    GLuint whiteMaskTexture_ = 0;

    cudaGraphicsResource* bridgeCudaResource_ = nullptr;
    cudaGraphicsResource* maskCudaResource_ = nullptr;
    cudaStream_t stream_ = nullptr;

    NvVFX_Handle effect_ = nullptr;
    NvVFX_StateObjectHandle state_ = nullptr;
    NvCVImage inputImage_{};
    NvCVImage maskImage_{};

    unsigned int processingWidth_ = 0;
    unsigned int processingHeight_ = 0;
    int activeMode_ = -1;
    bool cudaReady_ = false;
    bool pipelineReady_ = false;
    bool rebuildRequested_ = false;
    bool resetRequested_ = false;

    float mode_ = 2.0f;
    float threshold_ = 0.45f;
    float softness_ = 0.12f;
    float feather_ = 0.15f;
    float mix_ = 1.0f;
    float invert_ = 0.0f;
    float viewMask_ = 0.0f;
    float resetState_ = 0.0f;
    float edgeCleanup_ = 0.35f;

    std::string lastLoggedError_;
};
