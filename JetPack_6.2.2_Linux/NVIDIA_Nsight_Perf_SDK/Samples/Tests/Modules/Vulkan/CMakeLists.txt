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

find_package(Vulkan        REQUIRED) # requires cmake 3.7
set(VULKAN_FILES)
file(GLOB VULKAN_FILES "${Vulkan_INCLUDE_DIRS}/vulkan/*.h")
source_group("Vulkan" FILES ${VULKAN_FILES})

set(RYML_INCLUDE_DIR "${NvPerfUtility_IMPORTS_DIR}/rapidyaml-0.4.0")

set(SOURCES
    HeaderSanity/HeaderSanity_NvPerfMiniTraceVulkan.cpp
    HeaderSanity/HeaderSanity_NvPerfVulkan.cpp
    HeaderSanity/HeaderSanity_NvPerfPeriodicSamplerVulkan.cpp
    HeaderSanity/HeaderSanity_NvPerfRangeProfilerVulkan.cpp
    HeaderSanity/HeaderSanity_NvPerfReportGeneratorVulkan.cpp
    Vulkan_HudTextRenderer.cpp
    Vulkan_MiniTrace.cpp
    Vulkan_PeriodicSampler.cpp
    Vulkan_ReportGenerator.cpp
    VulkanMain.cpp
    VulkanUtilities.cpp
)

function(CreateNvPerfVulkanTest TARGET)
    add_executable(${TARGET}
        ${SOURCES}
        ${NVPERF_FILES}
        ${NVPERF_UTILITY_FILES}
        ${VULKAN_FILES}
    )
    target_include_directories(${TARGET}
        PRIVATE
            ${NVPERF_TEST_COMMON_INCLUDE_DIRS}
            ${Vulkan_INCLUDE_DIRS}
            ${RYML_INCLUDE_DIR}
    )
    target_link_libraries(${TARGET}
        PRIVATE
            NvPerf
            NvPerfUtility
            ${Vulkan_LIBRARY}
    )

    if(NOT WIN32)
        target_link_libraries(${TARGET} PRIVATE pthread)
    endif()

    if(MSVC)
        target_compile_options(${TARGET} PRIVATE /WX)
    endif()
endfunction()

CreateNvPerfVulkanTest(NvPerfVulkanTest)

CreateNvPerfVulkanTest(NvPerfVulkanTest_HideSymbols)
target_compile_definitions(NvPerfVulkanTest_HideSymbols
    PRIVATE VK_NO_PROTOTYPES
)

DeployNvPerf(NvPerfVulkanTest NvPerf)
DeployNvPerf(NvPerfVulkanTest_HideSymbols NvPerf)
