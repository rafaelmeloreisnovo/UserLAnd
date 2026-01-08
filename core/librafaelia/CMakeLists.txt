cmake_minimum_required(VERSION 3.22.1)
project(rafaelia C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(RAFAELIA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# RAFCODE-Φ Engine: Core native sources
add_library(rafaelia SHARED
  ${CMAKE_CURRENT_SOURCE_DIR}/impl/raf_cpu_parser.c
  ${CMAKE_CURRENT_SOURCE_DIR}/raf_crc_ecc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../add2/bitstack_Version2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/rafaelia_jni.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/lowlevel/tensor_core.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/baremetal_asm.S
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/modules/crypto/voynich_cipher.S
)

target_include_directories(rafaelia PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/impl
  ${RAFAELIA_ROOT}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/lowlevel
)

target_compile_options(rafaelia PRIVATE
  -O2
  -fno-exceptions
  -fvisibility=hidden
  -Wall
  -Wextra
  -Wno-unused-parameter
)

# Android 15/16 compatibility: 16KB page size support
# This prevents crashes on Android 16+ with 16KB memory pages
target_link_options(rafaelia PRIVATE
  -Wl,-z,max-page-size=16384
)

# Enable Pthread support for native threading (Phantom Process workaround)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

find_library(log-lib log)
target_link_libraries(rafaelia ${log-lib} Threads::Threads)
