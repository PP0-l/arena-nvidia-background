cmake_minimum_required(VERSION 3.24)

project(ArenaNvidiaBackground
    VERSION 0.1.0
    LANGUAGES CXX
)

include(CTest)

if(NOT WIN32)
    message(FATAL_ERROR "This plugin targets Resolume Arena on Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/EHsc)
endif()

find_package(CUDAToolkit 12.8 REQUIRED)

# CUDA 12.8 is required for Blackwell (SM 12.0 / sm_120) support and predates
# the latest VS 2022 toolset installed on this machine.
# Compile the one CUDA translation unit explicitly so NVIDIA's compatibility
# switches apply before CMake performs any CUDA compiler-version checks.
set(CUDA_KERNEL_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/cuda/CudaKernels.obj")
add_custom_command(
    OUTPUT "${CUDA_KERNEL_OBJECT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/cuda"
    COMMAND "${CUDAToolkit_NVCC_EXECUTABLE}"
        --use-local-env
        --allow-unsupported-compiler
        -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
        --std=c++17
        -O3
        -c
        -Xcompiler=/MT
        -I"${CMAKE_CURRENT_SOURCE_DIR}/src"
        -gencode=arch=compute_75,code=sm_75
        -gencode=arch=compute_86,code=sm_86
        -gencode=arch=compute_89,code=sm_89
        -gencode=arch=compute_120,code=sm_120
        -ccbin "${CMAKE_CXX_COMPILER}"
        -o "${CUDA_KERNEL_OBJECT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/CudaKernels.cu"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/CudaKernels.cu"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/CudaKernels.h"
    VERBATIM
)
set_source_files_properties("${CUDA_KERNEL_OBJECT}" PROPERTIES
    GENERATED TRUE
    EXTERNAL_OBJECT TRUE
)

set(FFGL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffgl" CACHE PATH
    "Path to the Resolume FFGL SDK")
set(FFGL_GLEW_ROOT "${FFGL_ROOT}/deps/glew-2.1.0")

if(NOT EXISTS "${FFGL_ROOT}/source/lib/FFGLSDK.cpp")
    message(FATAL_ERROR
        "FFGL SDK was not found. Clone with --recurse-submodules or run "
        "'git submodule update --init --recursive'.")
endif()

add_library(ffgl-sdk STATIC
    "${FFGL_ROOT}/source/lib/FFGLSDK.cpp"
)
set_target_properties(ffgl-sdk PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
target_include_directories(ffgl-sdk PUBLIC
    "${FFGL_ROOT}/source/lib"
    "${FFGL_GLEW_ROOT}/include"
)
target_compile_definitions(ffgl-sdk PUBLIC GLEW_STATIC)
target_link_libraries(ffgl-sdk PUBLIC
    "${FFGL_GLEW_ROOT}/lib/Release/x64/glew32s.lib"
    opengl32
)

add_library(NvidiaBackground SHARED
    src/CudaKernels.h
    "${CUDA_KERNEL_OBJECT}"
    src/NvidiaBackground.cpp
    src/NvidiaBackground.h
    src/NvidiaVfxApi.cpp
    src/NvidiaVfxApi.h
    src/FFGLPlugin.def
)
target_include_directories(NvidiaBackground PRIVATE src)
target_link_libraries(NvidiaBackground PRIVATE
    ffgl-sdk
    CUDA::cudart_static
    opengl32
)
target_compile_definitions(NvidiaBackground PRIVATE
    GLEW_STATIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
set_target_properties(NvidiaBackground PROPERTIES
    OUTPUT_NAME "NVIDIA Background"
    PREFIX ""
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

add_executable(NvidiaVfxSmokeTest
    tests/VfxSmokeTest.cpp
    src/NvidiaVfxApi.cpp
    src/NvidiaVfxApi.h
)
target_include_directories(NvidiaVfxSmokeTest PRIVATE src)
target_link_libraries(NvidiaVfxSmokeTest PRIVATE CUDA::cudart_static)
target_compile_definitions(NvidiaVfxSmokeTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(NvidiaVfxSmokeTest PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

add_executable(NvidiaVfxRawSequenceTest
    tests/VfxRawSequenceTest.cpp
    src/NvidiaVfxApi.cpp
    src/NvidiaVfxApi.h
)
target_include_directories(NvidiaVfxRawSequenceTest PRIVATE src)
target_link_libraries(NvidiaVfxRawSequenceTest PRIVATE CUDA::cudart_static)
target_compile_definitions(NvidiaVfxRawSequenceTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
set_target_properties(NvidiaVfxRawSequenceTest PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

add_executable(NvidiaBackgroundOpenGLSmokeTest
    tests/PluginOpenGLSmokeTest.cpp
)
target_include_directories(NvidiaBackgroundOpenGLSmokeTest PRIVATE
    "${FFGL_ROOT}/source/lib"
    "${FFGL_ROOT}/source/lib/ffgl"
    "${FFGL_GLEW_ROOT}/include"
)
target_link_libraries(NvidiaBackgroundOpenGLSmokeTest PRIVATE opengl32)
target_compile_definitions(NvidiaBackgroundOpenGLSmokeTest PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
set_target_properties(NvidiaBackgroundOpenGLSmokeTest PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

install(TARGETS NvidiaBackground RUNTIME DESTINATION .)

if(BUILD_TESTING)
    add_test(NAME NvidiaVfxSmokeTest
        COMMAND NvidiaVfxSmokeTest
    )
    add_test(NAME NvidiaBackgroundOpenGLSmokeTest
        COMMAND NvidiaBackgroundOpenGLSmokeTest $<TARGET_FILE:NvidiaBackground>
    )
    set_tests_properties(
        NvidiaVfxSmokeTest
        NvidiaBackgroundOpenGLSmokeTest
        PROPERTIES TIMEOUT 60
    )
endif()
