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

set(IMGUI_INCLUDE_DIR "${NvPerfUtility_IMPORTS_DIR}/imgui-1.87")
set(IMPLOT_INCLUDE_DIR "${NvPerfUtility_IMPORTS_DIR}/implot-0.13")
set(JSON_INCLUDE_DIR "${NvPerfUtility_IMPORTS_DIR}/json-3.9.1")
set(RYML_INCLUDE_DIR "${NvPerfUtility_IMPORTS_DIR}/rapidyaml-0.4.0")

set(SOURCES
    HeaderSanity/HeaderSanity_NvPerfCommonHtmlTemplates.cpp
    HeaderSanity/HeaderSanity_NvPerfCounterConfiguration.cpp
    HeaderSanity/HeaderSanity_NvPerfCounterData.cpp
    HeaderSanity/HeaderSanity_NvPerfDeviceProperties.cpp
    HeaderSanity/HeaderSanity_NvPerfHudConfigurationsAD10X.cpp
    HeaderSanity/HeaderSanity_NvPerfHudConfigurationsGA10B.cpp
    HeaderSanity/HeaderSanity_NvPerfHudConfigurationsGA10X.cpp
    HeaderSanity/HeaderSanity_NvPerfHudConfigurationsHAL.cpp
    HeaderSanity/HeaderSanity_NvPerfHudConfigurationsTU10X.cpp
    HeaderSanity/HeaderSanity_NvPerfHudConfigurationsTU11X.cpp
    HeaderSanity/HeaderSanity_NvPerfHudDataModel.cpp
    HeaderSanity/HeaderSanity_NvPerfHudImPlotRenderer.cpp
    HeaderSanity/HeaderSanity_NvPerfInit.cpp
    HeaderSanity/HeaderSanity_NvPerfMetricsConfigBuilder.cpp
    HeaderSanity/HeaderSanity_NvPerfMetricsEvaluator.cpp
    HeaderSanity/HeaderSanity_NvPerfRangeProfiler.cpp
    HeaderSanity/HeaderSanity_NvPerfReportDefinition.cpp
    HeaderSanity/HeaderSanity_NvPerfReportDefinitionAD10X.cpp
    HeaderSanity/HeaderSanity_NvPerfReportDefinitionGA10X.cpp
    HeaderSanity/HeaderSanity_NvPerfReportDefinitionHAL.cpp
    HeaderSanity/HeaderSanity_NvPerfReportDefinitionTU10X.cpp
    HeaderSanity/HeaderSanity_NvPerfReportDefinitionTU11X.cpp
    HeaderSanity/HeaderSanity_NvPerfReportGenerator.cpp
    OfflineMain.cpp
    Offline_CounterData.cpp
    Offline_CpuMarkerTrace.cpp
    Offline_HudDataModel.cpp
    Offline_HtmlReport.cpp
    Offline_MetricsEvaluator.cpp
    Offline_ScopeExitGuard.cpp
)
add_executable(NvPerfOfflineTest
    ${SOURCES}
    ${NVPERF_FILES}
    ${NVPERF_UTILITY_FILES}
    ${VULKAN_FILES}
)
target_include_directories(NvPerfOfflineTest
    PRIVATE
        ${NVPERF_TEST_COMMON_INCLUDE_DIRS}
        ${Vulkan_INCLUDE_DIRS}
        ${JSON_INCLUDE_DIR}
        ${IMGUI_INCLUDE_DIR}
        ${IMPLOT_INCLUDE_DIR}
        ${RYML_INCLUDE_DIR}
)
target_link_libraries(NvPerfOfflineTest
    PRIVATE
        NvPerf
        NvPerfUtility
)

if(NOT WIN32)
    target_link_libraries(NvPerfOfflineTest PRIVATE dl)
endif()

if(MSVC)
  target_compile_options(NvPerfOfflineTest PRIVATE /WX)
endif()

DeployNvPerf(NvPerfOfflineTest NvPerf)