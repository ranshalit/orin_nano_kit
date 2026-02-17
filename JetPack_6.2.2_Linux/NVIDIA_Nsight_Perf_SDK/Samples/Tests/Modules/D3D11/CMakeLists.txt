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
    D3D11_DeviceFunctions.cpp
    D3D11_RangeProfiler.cpp
    D3D11_ReportGenerator.cpp
    D3D11Main.cpp
    D3D11Utilities.cpp
)
add_executable(NvPerfD3D11Test
    ${SOURCES}
    ${NVPERF_FILES}
    ${NVPERF_UTILITY_FILES}
)
target_include_directories(NvPerfD3D11Test
    PRIVATE
        ${NVPERF_TEST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(NvPerfD3D11Test
    PRIVATE
        NvPerf
        NvPerfUtility
        dxgi
        d3d11
)

DeployNvPerf(NvPerfD3D11Test NvPerf)
