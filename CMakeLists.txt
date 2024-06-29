cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

set(MHDRL_PRODUCT_NUMBER 1)
set(MHDRL_PRODUCT_VERSION 2)
set(MHDRL_BUILD_NUMBER 2)

cmake_policy(SET CMP0091 NEW)
set(VCPKG_FEATURE_FLAGS "versions")
project(moonlight_hdr_launcher VERSION ${MHDRL_PRODUCT_NUMBER}.${MHDRL_PRODUCT_VERSION}.${MHDRL_BUILD_NUMBER} LANGUAGES CXX)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/version ${MHDRL_PRODUCT_NUMBER}.${MHDRL_PRODUCT_VERSION}.${MHDRL_BUILD_NUMBER})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version DESTINATION dist)
install(FILES ${CMAKE_SOURCE_DIR}/dist/mass_effect_andromeda-box-art.jpg ${CMAKE_SOURCE_DIR}/dist/mass_effect_andromeda-box-art.png ${CMAKE_SOURCE_DIR}/dist/gamestream_steam_bp.ini DESTINATION dist)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(NVAPI_DL_PATH "${CMAKE_SOURCE_DIR}/R460-developer.zip" CACHE PATH "Path to the NVAPI zip file")
option(SENTRY_DEBUG "Use https://sentry.io to report crashes" OFF)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY)

set(MHDRL_RC_VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.txt.in
  ${CMAKE_CURRENT_SOURCE_DIR}/version.txt
  @ONLY)

add_subdirectory(src)
