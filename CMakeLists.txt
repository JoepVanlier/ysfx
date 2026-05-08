# Copyright 2021 Jean Pierre Cimalando
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.15)

set(YSFX64_MACOS_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version supported by ysfx64.")
set(YSFX64_MACOS_ARCHITECTURES "x86_64" CACHE STRING "macOS architectures supported by ysfx64.")
set(YSFX64_MACOS_INTEL_CPU "core2" CACHE STRING "Oldest Intel CPU family that ysfx64 may optimize for on macOS.")

if(CMAKE_HOST_APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${YSFX64_MACOS_DEPLOYMENT_TARGET}" CACHE STRING "Minimum macOS version supported by ysfx64." FORCE)
    set(CMAKE_OSX_ARCHITECTURES "${YSFX64_MACOS_ARCHITECTURES}" CACHE STRING "macOS architectures supported by ysfx64." FORCE)
endif()

project(ysfx64 VERSION "0.0.45" LANGUAGES C CXX ASM)

if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        message(FATAL_ERROR "ysfx64 is an Intel-only macOS build. Set CMAKE_OSX_ARCHITECTURES=x86_64.")
    endif()

    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET VERSION_EQUAL "10.15")
        message(FATAL_ERROR "ysfx64 macOS builds must target macOS 10.15 for old Intel compatibility.")
    endif()

    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-march=${YSFX64_MACOS_INTEL_CPU}>")
endif()

# check if we are building from this project, or are imported by another
if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(YSFX_BUILD_FROM_HERE TRUE)
else()
    set(YSFX_BUILD_FROM_HERE FALSE)
endif()

option(YSFX_PORTABLE "Disable architecture-dependent code" OFF)
option(YSFX_GFX "Build graphics support" ON)
option(YSFX_PLUGIN "Build audio plugin" "${YSFX_BUILD_FROM_HERE}")
option(YSFX_PLUGIN_LTO "Enable link-time optimization for plugin" OFF)
option(YSFX_PLUGIN_FORCE_DEBUG "Build debug features in plugin" OFF)
option(YSFX_PLUGIN_COPY "Copy the plugin after build" ON)
option(YSFX_PLUGIN_USE_SYSTEM_JUCE "Use preinstalled JUCE on the system" OFF)
set(YSFX_PLUGIN_VST3_SDK_PATH "" CACHE PATH "Custom path to the VST3 SDK")
option(YSFX_STATIC_CRT "Use static CRT (Windows only)" OFF)
option(YSFX_TESTS "Build unit tests" OFF)
option(YSFX_TOOLS "Build tools" OFF)
option(YSFX_SKIP_CHECKSUM "Skip the checksum" OFF)
option(YSFX_DEEP_STRIP "Strip the library clean of non-API symbols (if platform supports)" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(BuildType)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET "hidden")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")
set(CMAKE_ASM_VISIBILITY_PRESET "hidden")
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(YSFX_FILE_OFFSET_BITS_GENEXP "$<$<NOT:$<BOOL:${WIN32}>>:_FILE_OFFSET_BITS=64>")
if(YSFX_STATIC_CRT)
    if(MSVC)
        message(STATUS "Static CRT enabled")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

if(NOT MSVC)
    add_compile_options(
        "-Wall"
        "-Werror=return-type")
endif()
if(MSVC)
    add_definitions("-D_CRT_SECURE_NO_WARNINGS")
endif()

###
include("cmake.deps.txt")
include("cmake.wdl.txt")
include("cmake.ysfx.txt")
if(YSFX_TESTS)
    include("cmake.tests.txt")
endif()
if(YSFX_PLUGIN)
    include("cmake.plugin.txt")
endif()
if(YSFX_TOOLS)
    include("cmake.tools.txt")
endif()
