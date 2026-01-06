cmake_minimum_required(VERSION 3.22.1)
project(rafaelia C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(RAFAELIA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_library(rafaelia SHARED
  ${CMAKE_CURRENT_SOURCE_DIR}/impl/raf_cpu_parser.c
  ${CMAKE_CURRENT_SOURCE_DIR}/raf_crc_ecc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../add2/bitstack_Version2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/main/cpp/rafaelia_jni.c
)

target_include_directories(rafaelia PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/impl
  ${RAFAELIA_ROOT}/include
)

target_compile_options(rafaelia PRIVATE
  -O2
  -fno-exceptions
  -fvisibility=hidden
  -Wall
  -Wextra
  -Wno-unused-parameter
)

find_library(log-lib log)
target_link_libraries(rafaelia ${log-lib})
