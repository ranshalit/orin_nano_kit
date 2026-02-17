#[[
* Copyright 2014-2023 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
]]

cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
cmake_policy(VERSION 3.7)

project(NvPerfTests)

find_package(NvPerf        REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
find_package(NvPerfUtility REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)

add_definitions("-DDOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${CMAKE_BINARY_DIR}/bin/")
set(DOCTEST_INCLUDE_DIR             "${CMAKE_CURRENT_SOURCE_DIR}/../Imports/doctest-2.4.8")
set(NVPERF_TEST_INCLUDE_DIR         "${CMAKE_CURRENT_SOURCE_DIR}/../Include")

# Common Sources
# 1) NvPerf API
set(NVPERF_FILES)
foreach(NvPerf_INCLUDE_DIR IN LISTS NvPerf_INCLUDE_DIRS)
    file(GLOB FILES "${NvPerf_INCLUDE_DIR}/*.h")
    list(APPEND NVPERF_FILES ${FILES})
endforeach()
source_group("NvPerf" FILES ${NVPERF_FILES})
# 2) NvPerf Utility
set(NVPERF_UTILITY_FILES)
file(GLOB NVPERF_UTILITY_FILES "${NvPerfUtility_INCLUDE_DIRS}/*.h")
source_group("NvPerfUtility" FILES ${NVPERF_UTILITY_FILES})

# Common Include Directories
set(NVPERF_TEST_COMMON_INCLUDE_DIRS
    ${NvPerf_INCLUDE_DIRS}
    ${NvPerfUtility_INCLUDE_DIRS}
    ${DOCTEST_INCLUDE_DIR}
    ${NVPERF_TEST_INCLUDE_DIR}
)

# Add Tests
if(WIN32)
    add_subdirectory(D3D12)
    add_subdirectory(D3D11)
endif()
if(UNIX)
    add_subdirectory(EGL)
endif()

add_subdirectory(Device)
add_subdirectory(Offline)
add_subdirectory(OGL)
add_subdirectory(Vulkan)
