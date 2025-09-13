cmake_minimum_required(VERSION 3.15)
project(msm5232vst VERSION 2.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure MSVC treats sources as UTF-8 to avoid codepage 932 warnings
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(BUILD_VST3 "Build VST3 plugin (requires Steinberg VST3 SDK)" OFF)
set(VST3_SDK_DIR "" CACHE PATH "Path to VST3_SDK directory (when BUILD_VST3=ON)")

if(BUILD_VST3)
    if(NOT VST3_SDK_DIR)
        message(FATAL_ERROR "BUILD_VST3=ON but VST3_SDK_DIR is not set")
    endif()
    if(NOT EXISTS "${VST3_SDK_DIR}/cmake/modules/SMTG_VST3_SDK.cmake")
        message(FATAL_ERROR "SMTG_VST3_SDK.cmake not found under ${VST3_SDK_DIR}. Ensure it points to the vst3sdk root.")
    endif()
    # Avoid writing outside the workspace during configure
    set(SMTG_PLUGIN_TARGET_USER_PATH "${CMAKE_BINARY_DIR}/vst3" CACHE PATH "Target path for built VST3 packages")
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "Disable auto-link to user VST3 folder during build")
    list(APPEND CMAKE_MODULE_PATH "${VST3_SDK_DIR}/cmake/modules")
    include(${VST3_SDK_DIR}/cmake/modules/SMTG_VST3_SDK.cmake)
    # Force again after include in case defaults override
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "Disable auto-link to user VST3 folder during build" FORCE)
    set(SMTG_PLUGIN_TARGET_USER_PATH "${CMAKE_BINARY_DIR}/vst3" CACHE PATH "Target path for built VST3 packages" FORCE)
    set(SMTG_CREATE_MODULE_INFO OFF CACHE BOOL "Disable moduleinfo generation" FORCE)
    set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "Disable validator run" FORCE)
    set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "Disable VSTGUI dependency" FORCE)
    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "Disable SDK plugin examples" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "Disable SDK hosting examples" FORCE)
    # Bring in SDK tools/targets (validator, moduleinfotool) if needed
    add_subdirectory(${VST3_SDK_DIR} ${CMAKE_BINARY_DIR}/vst3sdk_build EXCLUDE_FROM_ALL)
endif()

add_subdirectory(src)
