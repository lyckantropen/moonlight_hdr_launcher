cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(moonlight_hdr_launcher LANGUAGES CXX)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(src)
