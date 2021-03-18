cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

set(MHDRL_PRODUCT_NUMBER 1)
set(MHDRL_PRODUCT_VERSION 0)
set(MHDRL_BUILD_NUMBER 9)

cmake_policy(SET CMP0091 NEW)
project(moonlight_hdr_launcher VERSION ${MHDRL_PRODUCT_NUMBER}.${MHDRL_PRODUCT_VERSION}.${MHDRL_BUILD_NUMBER} LANGUAGES CXX)

file(WRITE ${CMAKE_SOURCE_DIR}/dist/version ${MHDRL_PRODUCT_NUMBER}.${MHDRL_PRODUCT_VERSION}.${MHDRL_BUILD_NUMBER})
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.txt.in
  ${CMAKE_CURRENT_SOURCE_DIR}/version.txt
  @ONLY)

add_subdirectory(src)
