cmake_minimum_required(VERSION 3.22.1)
project(rafaelia C ASM)

# ==============================================================================
# Cross-Architecture Compatibility Configuration
# ==============================================================================
# This CMake configuration supports building for multiple architectures:
# - Android: arm64-v8a, armeabi-v7a, x86_64, x86
# - Linux: aarch64, arm, x86_64, i386
# - macOS: arm64, x86_64
#
# Version compatibility:
# - Android API 21-35 (Android 5.0 - Android 15)
# - NDK r21+ (r27+ recommended for Android 15 16KB page support)
# ==============================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(RAFAELIA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# ==============================================================================
# Architecture Detection and Configuration
# ==============================================================================

# Detect target architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(RAF_ARCH "arm64")
    set(RAF_ARCH_BITS 64)
    set(RAF_HAS_NEON ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|arm")
    set(RAF_ARCH "arm")
    set(RAF_ARCH_BITS 32)
    set(RAF_HAS_NEON ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(RAF_ARCH "x86_64")
    set(RAF_ARCH_BITS 64)
    set(RAF_HAS_SSE ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686|i386|x86")
    set(RAF_ARCH "x86")
    set(RAF_ARCH_BITS 32)
    set(RAF_HAS_SSE ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    set(RAF_ARCH "riscv64")
    set(RAF_ARCH_BITS 64)
else()
    set(RAF_ARCH "unknown")
    set(RAF_ARCH_BITS ${CMAKE_SIZEOF_VOID_P})
endif()

message(STATUS "Detected architecture: ${RAF_ARCH} (${RAF_ARCH_BITS}-bit)")

# ==============================================================================
# Android-Specific Configuration
# ==============================================================================

if(ANDROID)
    message(STATUS "Building for Android")
    message(STATUS "  API Level: ${ANDROID_NATIVE_API_LEVEL}")
    message(STATUS "  ABI: ${ANDROID_ABI}")
    message(STATUS "  NDK: ${ANDROID_NDK}")
    
    # Map Android ABI to architecture flags
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(RAF_ANDROID_ARCH "arm64")
        add_compile_definitions(RAF_ANDROID_ARM64=1)
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(RAF_ANDROID_ARCH "arm")
        add_compile_definitions(RAF_ANDROID_ARM32=1)
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(RAF_ANDROID_ARCH "x86_64")
        add_compile_definitions(RAF_ANDROID_X86_64=1)
    elseif(ANDROID_ABI STREQUAL "x86")
        set(RAF_ANDROID_ARCH "x86")
        add_compile_definitions(RAF_ANDROID_X86=1)
    endif()
    
    # Android 15 (API 35) specific optimizations
    if(ANDROID_NATIVE_API_LEVEL GREATER_EQUAL 35)
        add_compile_definitions(RAF_ANDROID_15_COMPAT=1)
        message(STATUS "  Android 15 compatibility enabled")
    endif()
    
    # Android 14 (API 34) specific optimizations
    if(ANDROID_NATIVE_API_LEVEL GREATER_EQUAL 34)
        add_compile_definitions(RAF_ANDROID_14_COMPAT=1)
    endif()
endif()

# ==============================================================================
# SIMD Configuration
# ==============================================================================

# ARM NEON support
if(RAF_HAS_NEON OR ANDROID_ABI MATCHES "arm")
    add_compile_definitions(RAF_ENABLE_NEON=1)
    if(RAF_ARCH STREQUAL "arm")
        # ARMv7 with NEON
        add_compile_options(-mfpu=neon)
    endif()
    message(STATUS "NEON SIMD enabled")
endif()

# x86 SSE support
if(RAF_HAS_SSE OR RAF_ARCH MATCHES "x86")
    add_compile_definitions(RAF_ENABLE_SSE=1)
    if(RAF_ARCH STREQUAL "x86_64")
        # SSE2 is baseline for x86_64
        add_compile_options(-msse2)
    elseif(RAF_ARCH STREQUAL "x86")
        # SSE2 for 32-bit x86
        add_compile_options(-msse2)
    endif()
    message(STATUS "SSE SIMD enabled")
endif()

# ==============================================================================
# Source Files
# ==============================================================================

# RAFCODE-Φ Engine: Core native sources
set(RAF_CORE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/impl/raf_cpu_parser.c
  ${CMAKE_CURRENT_SOURCE_DIR}/raf_crc_ecc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/impl/vectra_vm_vector_field.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../add2/bitstack_Version2.c
)

if(ANDROID)
  list(APPEND RAF_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/rafaelia_jni.c
  )
endif()

# ARM64-only sources use AArch64 assembly/NEON instructions.
if(ANDROID_ABI STREQUAL "arm64-v8a" OR RAF_ARCH STREQUAL "arm64")
  list(APPEND RAF_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/lowlevel/tensor_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/baremetal_asm.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/modules/crypto/voynich_cipher.S
  )
endif()

add_library(rafaelia SHARED ${RAF_CORE_SOURCES})

# ==============================================================================
# Include Directories
# ==============================================================================

target_include_directories(rafaelia PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/impl
  ${RAFAELIA_ROOT}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/lowlevel
)

# ==============================================================================
# Compile Options
# ==============================================================================

target_compile_options(rafaelia PRIVATE
  -O2
  -fno-exceptions
  -fvisibility=hidden
  -Wall
  -Wextra
  -Wno-unused-parameter
)

# Architecture-specific optimizations
if(RAF_ARCH STREQUAL "arm64")
    target_compile_options(rafaelia PRIVATE
        -march=armv8-a
    )
elseif(RAF_ARCH STREQUAL "arm")
    target_compile_options(rafaelia PRIVATE
        -march=armv7-a
        -mthumb
    )
endif()

# ==============================================================================
# Link Options
# ==============================================================================

# Android 15/16 compatibility: 16KB page size support
# This prevents crashes on Android 16+ with 16KB memory pages
target_link_options(rafaelia PRIVATE
  -Wl,-z,max-page-size=16384
)

# Additional security hardening
target_link_options(rafaelia PRIVATE
  -Wl,-z,relro
  -Wl,-z,now
)

# ==============================================================================
# Threading Support
# ==============================================================================

# Enable Pthread support for native threading (Phantom Process workaround)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# ==============================================================================
# Link Libraries
# ==============================================================================

if(ANDROID)
  find_library(log-lib log)
  target_link_libraries(rafaelia ${log-lib} Threads::Threads)
else()
  target_link_libraries(rafaelia Threads::Threads)
endif()

# ==============================================================================
# Installation
# ==============================================================================

# Export architecture info for dependent projects
set(RAF_BUILD_ARCH ${RAF_ARCH} CACHE STRING "Target architecture for rafaelia")
set(RAF_BUILD_BITS ${RAF_ARCH_BITS} CACHE STRING "Target bit width for rafaelia")

message(STATUS "Rafaelia library configuration complete")
message(STATUS "  Architecture: ${RAF_ARCH}")
message(STATUS "  Bits: ${RAF_ARCH_BITS}")

# Native host tests keep deterministic helpers failsafe without Android runtime.
if(NOT ANDROID)
  enable_testing()
  add_executable(vectra_vm_vector_field_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vectra_vm_vector_field.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/vectra_vm_vector_field.c
  )
  target_include_directories(vectra_vm_vector_field_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/impl
  )
  target_compile_options(vectra_vm_vector_field_test PRIVATE
    -O2
    -Wall
    -Wextra
    -Werror
  )
  add_test(NAME vectra_vm_vector_field_test COMMAND vectra_vm_vector_field_test)
endif()
