find_package(Boost REQUIRED filesystem)

set(NVAPI_DL_PATH "${CMAKE_SOURCE_DIR}/R460-developer.zip" CACHE PATH "Path to the NVAPI zip file")
option(SENTRY_DEBUG "Use https://sentry.io to report crashes" OFF)

include(FetchContent)
FetchContent_Declare(
    ext_nvapi
    URL "${NVAPI_DL_PATH}"
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ""
    BUILD_COMMAND ""
)
FetchContent_Populate(ext_nvapi)

set(NVAPI_PATH ${ext_nvapi_SOURCE_DIR})

add_library(nvapi STATIC IMPORTED)

if(${CMAKE_SIZEOF_VOID_P} STREQUAL 8)
    set(nvlib amd64/nvapi64.lib)
else()
    set(nvlib x86/nvapi.lib)
endif()

if(SENTRY_DEBUG)
    set(SENTRY_BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(
        sentry
        URL https://github.com/getsentry/sentry-native/releases/download/0.4.4/sentry-native.zip
    )
    FetchContent_MakeAvailable(sentry)
    set(SENTRY_LIBRARIES sentry::sentry)
    set(SENTRY_COMPILE_DEFINITIONS -DSENTRY_DEBUG=1)
    find_file(CRASHPAD_HANDLER crashpad_handler.exe PATHS ${sentry_BINARY_DIR}/crashpad_build/handler)
    file(COPY ${CRASHPAD_HANDLER} DESTINATION ${CMAKE_SOURCE_DIR}/dist)
endif()

set_property(TARGET nvapi PROPERTY IMPORTED_LOCATION ${NVAPI_PATH}/${nvlib})
set_property(TARGET nvapi PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${NVAPI_PATH})

add_executable(MassEffectAndromeda WIN32 main.cpp hdr_toggle.cpp ${CMAKE_CURRENT_BINARY_DIR}/../version.rc)

target_compile_definitions(MassEffectAndromeda PRIVATE -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE -DMHDRL_VERSION="${MHDRL_PRODUCT_NUMBER}.${MHDRL_PRODUCT_VERSION}.${MHDRL_BUILD_NUMBER}" ${SENTRY_COMPILE_DEFINITIONS})
target_compile_options(MassEffectAndromeda PRIVATE /EHscr)
target_compile_features(MassEffectAndromeda PRIVATE cxx_std_17)
target_link_libraries(MassEffectAndromeda PRIVATE Boost::headers Boost::filesystem nvapi ${SENTRY_LIBRARIES})
set_target_properties(MassEffectAndromeda PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/dist)
set_target_properties(MassEffectAndromeda PROPERTIES RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_SOURCE_DIR}/dist)
set_target_properties(MassEffectAndromeda PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/dist)
set_target_properties(MassEffectAndromeda PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/dist)
