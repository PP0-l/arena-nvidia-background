#include "NvidiaVfxApi.h"

#include <cstdlib>
#include <sstream>

namespace
{
std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}
}

NvidiaVfxApi::~NvidiaVfxApi()
{
    Unload();
}

std::filesystem::path NvidiaVfxApi::FindRuntimeDirectory() const
{
    wchar_t configuredPath[32768]{};
    const DWORD configuredLength = GetEnvironmentVariableW(
        L"NV_VIDEO_EFFECTS_PATH", configuredPath,
        static_cast<DWORD>(sizeof(configuredPath) / sizeof(configuredPath[0])));
    if (configuredLength > 0 && configuredLength < (sizeof(configuredPath) / sizeof(configuredPath[0])))
    {
        const std::filesystem::path candidate(configuredPath);
        if (std::filesystem::exists(candidate / L"NVVideoEffects.dll"))
            return candidate;
    }

    wchar_t programFiles[32768]{};
    const DWORD programFilesLength = GetEnvironmentVariableW(
        L"ProgramFiles", programFiles,
        static_cast<DWORD>(sizeof(programFiles) / sizeof(programFiles[0])));
    if (programFilesLength > 0 && programFilesLength < (sizeof(programFiles) / sizeof(programFiles[0])))
    {
        const std::filesystem::path candidate =
            std::filesystem::path(programFiles) / L"NVIDIA Corporation" / L"NVIDIA Video Effects";
        if (std::filesystem::exists(candidate / L"NVVideoEffects.dll"))
            return candidate;
    }

    const std::filesystem::path fallback =
        L"C:\\Program Files\\NVIDIA Corporation\\NVIDIA Video Effects";
    return fallback;
}

void NvidiaVfxApi::SetWindowsError(const char* operation)
{
    const DWORD code = GetLastError();
    wchar_t* message = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);

    std::ostringstream stream;
    stream << operation << " failed (Windows error " << code << ")";
    if (message != nullptr)
    {
        stream << ": " << WideToUtf8(message);
        LocalFree(message);
    }
    lastError_ = stream.str();
}

template<typename T>
bool NvidiaVfxApi::LoadSymbol(HMODULE module, const char* name, T& target)
{
    target = reinterpret_cast<T>(GetProcAddress(module, name));
    if (target != nullptr)
        return true;
    SetWindowsError(name);
    return false;
}

bool NvidiaVfxApi::Load()
{
    if (IsLoaded())
        return true;

    Unload();
    runtimeDirectory_ = FindRuntimeDirectory();
    if (!std::filesystem::exists(runtimeDirectory_ / L"models"))
    {
        lastError_ = "NVIDIA Video Effects models directory was not found: " +
                     WideToUtf8((runtimeDirectory_ / L"models").wstring());
        return false;
    }

    constexpr DWORD searchFlags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                  LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
    cvImageModule_ = LoadLibraryExW((runtimeDirectory_ / L"NVCVImage.dll").c_str(), nullptr, searchFlags);
    if (cvImageModule_ == nullptr)
    {
        SetWindowsError("LoadLibraryExW(NVCVImage.dll)");
        return false;
    }

    videoEffectsModule_ = LoadLibraryExW((runtimeDirectory_ / L"NVVideoEffects.dll").c_str(), nullptr, searchFlags);
    if (videoEffectsModule_ == nullptr)
    {
        SetWindowsError("LoadLibraryExW(NVVideoEffects.dll)");
        Unload();
        return false;
    }

    bool ok = true;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_CreateEffect", CreateEffect) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_DestroyEffect", DestroyEffect) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_SetString", SetString) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_SetU32", SetU32) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_SetCudaStream", SetCudaStream) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_SetImage", SetImage) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_Load", LoadEffect) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_Run", RunEffect) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_AllocateState", AllocateState) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_DeallocateState", DeallocateState) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_ResetState", ResetState) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_SetStateObjectHandleArray", SetStateArray) && ok;
    ok = LoadSymbol(videoEffectsModule_, "NvVFX_GetVersion", GetVersion) && ok;

    ok = LoadSymbol(cvImageModule_, "NvCVImage_Alloc", ImageAlloc) && ok;
    ok = LoadSymbol(cvImageModule_, "NvCVImage_Dealloc", ImageDealloc) && ok;
    ok = LoadSymbol(cvImageModule_, "NvCV_GetErrorStringFromCode", ErrorString) && ok;

    if (!ok)
    {
        Unload();
        return false;
    }

    lastError_.clear();
    return true;
}

void NvidiaVfxApi::Unload()
{
    CreateEffect = nullptr;
    DestroyEffect = nullptr;
    SetString = nullptr;
    SetU32 = nullptr;
    SetCudaStream = nullptr;
    SetImage = nullptr;
    LoadEffect = nullptr;
    RunEffect = nullptr;
    AllocateState = nullptr;
    DeallocateState = nullptr;
    ResetState = nullptr;
    SetStateArray = nullptr;
    GetVersion = nullptr;
    ImageAlloc = nullptr;
    ImageDealloc = nullptr;
    ErrorString = nullptr;

    if (videoEffectsModule_ != nullptr)
    {
        FreeLibrary(videoEffectsModule_);
        videoEffectsModule_ = nullptr;
    }
    if (cvImageModule_ != nullptr)
    {
        FreeLibrary(cvImageModule_);
        cvImageModule_ = nullptr;
    }
}

std::string NvidiaVfxApi::StatusString(NvCV_Status status) const
{
    if (ErrorString != nullptr)
    {
        if (const char* message = ErrorString(status); message != nullptr)
            return message;
    }
    return "NvCV status " + std::to_string(static_cast<int>(status));
}

