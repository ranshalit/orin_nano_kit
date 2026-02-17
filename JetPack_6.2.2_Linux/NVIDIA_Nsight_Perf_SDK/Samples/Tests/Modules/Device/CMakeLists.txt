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

set(SOURCES
    HeaderSanity/HeaderSanity_NvPerfPeriodicSamplerCommon.cpp
    HeaderSanity/HeaderSanity_NvPerfPeriodicSamplerGpu.cpp
    Device_PeriodicSamplerGpu.cpp
    DeviceMain.cpp
    DeviceUtilities.cpp
)
add_executable(NvPerfDeviceTest
    ${SOURCES}
    ${NVPERF_FILES}
    ${NVPERF_UTILITY_FILES}
)
target_include_directories(NvPerfDeviceTest
    PRIVATE
        ${NVPERF_TEST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(NvPerfDeviceTest
    PRIVATE
        NvPerf
        NvPerfUtility
)

if(NOT WIN32)
    target_link_libraries(NvPerfDeviceTest PRIVATE pthread)
endif()

if(MSVC)
  target_compile_options(NvPerfDeviceTest PRIVATE /WX)
endif()

DeployNvPerf(NvPerfDeviceTest NvPerf)