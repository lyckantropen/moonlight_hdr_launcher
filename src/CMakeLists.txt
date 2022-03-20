find_package(Boost REQUIRED filesystem)

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
        URL https://github.com/getsentry/sentry-native/releases/download/0.4.15/sentry-native.zip
    )
    FetchContent_MakeAvailable(sentry)
    set(SENTRY_LIBRARIES sentry::sentry)
    set(SENTRY_COMPILE_DEFINITIONS -DSENTRY_DEBUG=1 -DSENTRY_BACKEND=crashpad)
    install(FILES ${sentry_BINARY_DIR}/crashpad_build/handler/crashpad_handler.exe DESTINATION dist)
endif()

set_property(TARGET nvapi PROPERTY IMPORTED_LOCATION ${NVAPI_PATH}/${nvlib})
set_property(TARGET nvapi PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${NVAPI_PATH})

add_executable(MassEffectAndromeda WIN32 main.cpp hdr_toggle.cpp "${MHDRL_RC_VERSION_FILE}")
target_compile_definitions(MassEffectAndromeda PRIVATE -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE -DMHDRL_VERSION="${MHDRL_PRODUCT_NUMBER}.${MHDRL_PRODUCT_VERSION}.${MHDRL_BUILD_NUMBER}" ${SENTRY_COMPILE_DEFINITIONS})
target_compile_options(MassEffectAndromeda PRIVATE /EHscr)
target_compile_features(MassEffectAndromeda PRIVATE cxx_std_20)
target_link_libraries(MassEffectAndromeda PRIVATE Boost::headers Boost::filesystem nvapi ${SENTRY_LIBRARIES})

install(TARGETS MassEffectAndromeda DESTINATION dist )
install(FILES $<TARGET_PDB_FILE:MassEffectAndromeda> DESTINATION dist OPTIONAL)
