cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(moonlight_hdr_launcher LANGUAGES CXX)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Boost REQUIRED filesystem)

add_executable(MassEffectAndromeda main.cpp)

target_compile_definitions(MassEffectAndromeda PRIVATE -D_WIN32_WINNT=0x0601)
target_compile_features(MassEffectAndromeda PRIVATE cxx_std_17)
target_link_libraries(MassEffectAndromeda PRIVATE Boost::headers Boost::filesystem)
