#include <Windows.h>

#include <FFGL.h>

#include <iostream>
#include <string>

extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;

namespace
{
using PluginMain = FFMixed (__stdcall*)(FFUInt32, FFMixed, FFInstanceID);
using SetLogCallbackProc = void (__stdcall*)(PFNLog);

void __stdcall PluginLog(char* message)
{
    if (message != nullptr)
        std::cout << "[plugin] " << message << '\n';
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2)
    {
        std::wcerr << L"Usage: NvidiaBackgroundOpenGLSmokeTest plugin.dll\n";
        return 1;
    }

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"NvidiaBackgroundOpenGLSmokeWindow";
    WNDCLASSW windowClass{};
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    if (RegisterClassW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        std::cerr << "RegisterClassW failed.\n";
        return 2;
    }

    HWND window = CreateWindowExW(0, className, L"FFGL smoke test", WS_OVERLAPPEDWINDOW,
                                  0, 0, 64, 64, nullptr, nullptr, instance, nullptr);
    if (window == nullptr)
    {
        std::cerr << "CreateWindowExW failed.\n";
        return 3;
    }

    HDC deviceContext = GetDC(window);
    PIXELFORMATDESCRIPTOR descriptor{};
    descriptor.nSize = sizeof(descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = 32;
    descriptor.cAlphaBits = 8;
    descriptor.cDepthBits = 24;
    descriptor.iLayerType = PFD_MAIN_PLANE;
    const int pixelFormat = ChoosePixelFormat(deviceContext, &descriptor);
    if (pixelFormat == 0 || !SetPixelFormat(deviceContext, pixelFormat, &descriptor))
    {
        std::cerr << "Unable to configure the OpenGL pixel format.\n";
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        return 4;
    }

    HGLRC context = wglCreateContext(deviceContext);
    if (context == nullptr || !wglMakeCurrent(deviceContext, context))
    {
        std::cerr << "Unable to create the OpenGL context.\n";
        if (context != nullptr)
            wglDeleteContext(context);
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        return 5;
    }

    HMODULE pluginModule = LoadLibraryW(argv[1]);
    if (pluginModule == nullptr)
    {
        std::cerr << "LoadLibraryW failed with error " << GetLastError() << ".\n";
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context);
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        return 6;
    }

    auto pluginMain = reinterpret_cast<PluginMain>(GetProcAddress(pluginModule, "plugMain"));
    auto setLogCallback = reinterpret_cast<SetLogCallbackProc>(
        GetProcAddress(pluginModule, "SetLogCallback"));
    if (pluginMain == nullptr || setLogCallback == nullptr)
    {
        std::cerr << "Required FFGL exports were not found.\n";
        FreeLibrary(pluginModule);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context);
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        return 7;
    }

    setLogCallback(PluginLog);
    FFMixed input{};
    if (pluginMain(FF_INITIALISE_V2, input, nullptr).UIntValue != FF_SUCCESS)
    {
        std::cerr << "FF_INITIALISE_V2 failed.\n";
        FreeLibrary(pluginModule);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context);
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        return 8;
    }

    input.UIntValue = 0;
    const FFMixed modeElements = pluginMain(FF_GET_NUM_PARAMETER_ELEMENTS, input, nullptr);
    if (modeElements.UIntValue != 4)
    {
        std::cerr << "Expected four Mode elements, got " << modeElements.UIntValue << ".\n";
        pluginMain(FF_DEINITIALISE, FFMixed{}, nullptr);
        FreeLibrary(pluginModule);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context);
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        return 9;
    }

    FFGLViewportStruct viewport{0, 0, 1280, 720};
    input.PointerValue = &viewport;
    const FFMixed instantiated = pluginMain(FF_INSTANTIATE_GL, input, nullptr);
    if (instantiated.PointerValue == nullptr ||
        instantiated.PointerValue == reinterpret_cast<void*>(static_cast<uintptr_t>(FF_FAIL)))
    {
        std::cerr << "FF_INSTANTIATE_GL failed.\n";
        pluginMain(FF_DEINITIALISE, FFMixed{}, nullptr);
        FreeLibrary(pluginModule);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(context);
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        return 10;
    }

    const FFInstanceID pluginInstance = instantiated.PointerValue;
    const FFMixed deinstantiated = pluginMain(FF_DEINSTANTIATE_GL, FFMixed{}, pluginInstance);
    const FFMixed deinitialised = pluginMain(FF_DEINITIALISE, FFMixed{}, nullptr);

    FreeLibrary(pluginModule);
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context);
    ReleaseDC(window, deviceContext);
    DestroyWindow(window);
    UnregisterClassW(className, instance);

    if (deinstantiated.UIntValue != FF_SUCCESS || deinitialised.UIntValue != FF_SUCCESS)
    {
        std::cerr << "FFGL cleanup failed.\n";
        return 11;
    }

    std::cout << "FFGL OpenGL smoke test passed: four modes, shader compilation, CUDA/GL init.\n";
    return 0;
}
