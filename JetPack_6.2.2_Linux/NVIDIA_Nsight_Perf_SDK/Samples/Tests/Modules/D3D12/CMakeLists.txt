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
    D3D12_PeriodicSampler.cpp
    D3D12_ReportGenerator.cpp
    D3D12_MiniTrace.cpp
    D3D12Main.cpp
    D3D12Utilities.cpp
    HeaderSanity/HeaderSanity_NvPerfD3D.cpp
    HeaderSanity/HeaderSanity_NvPerfD3D12.cpp
    HeaderSanity/HeaderSanity_NvPerfMiniTraceD3D12.cpp
    HeaderSanity/HeaderSanity_NvPerfPeriodicSamplerD3D12.cpp
    HeaderSanity/HeaderSanity_NvPerfRangeProfilerD3D12.cpp
    HeaderSanity/HeaderSanity_NvPerfReportGeneratorD3D12.cpp
)
add_executable(NvPerfD3D12Test
    ${SOURCES}
    ${NVPERF_FILES}
    ${NVPERF_UTILITY_FILES}
)
target_include_directories(NvPerfD3D12Test
    PRIVATE
        ${NVPERF_TEST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(NvPerfD3D12Test
    PRIVATE
        NvPerf
        NvPerfUtility
        dxgi
        d3d12
        d3dcompiler
)

if(MSVC)
  target_compile_options(NvPerfD3D12Test PRIVATE /WX)
endif()

DeployNvPerf(NvPerfD3D12Test NvPerf)