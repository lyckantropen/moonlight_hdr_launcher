cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(moonlight_hdr_launcher VERSION 1.0.0 LANGUAGES CXX)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(MHDRL_PRODUCT_NUMBER 1)
set(MHDRL_PRODUCT_VERSION 0)
set(MHDRL_BUILD_NUMBER 0)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY)

add_subdirectory(src)
