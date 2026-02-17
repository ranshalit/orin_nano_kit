/*
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
*/
#include "VulkanUtilities.h"
#include <NvPerfReportGeneratorVulkan.h>
#include <NvPerfScopeExitGuard.h>

namespace nv { namespace perf { namespace test {
    NVPW_TEST_SUITE_BEGIN("Vulkan");

    NVPW_TEST_CASE("ReportGenerator")
    {
        VkInstance instance = VulkanCreateInstance("NvPerf Vulkan Samples Report Generator Test");
        NVPW_REQUIRE(instance != (VkInstance)VK_NULL_HANDLE);
        auto destroyInstance = ScopeExitGuard([&]() {
            vkDestroyInstance(instance, nullptr);
        });

        PhysicalDevice physicalDevice = VulkanFindNvidiaPhysicalDevice(instance);
        NVPW_REQUIRE(physicalDevice.physicalDevice != (VkPhysicalDevice)VK_NULL_HANDLE);

        LogicalDevice logicalDevice = VulkanCreateLogicalDeviceWithGraphicsAndAsyncQueues(instance, physicalDevice);
        NVPW_REQUIRE(logicalDevice.device != (VkDevice)VK_NULL_HANDLE);
        NVPW_REQUIRE(logicalDevice.gfxQueue != (VkQueue)VK_NULL_HANDLE);
        auto destroyDevice = ScopeExitGuard([&]() {
            vkDestroyDevice(logicalDevice.device, nullptr);
        });

        NVPW_REQUIRE(VulkanLoadDriver(instance));
        if (!profiler::VulkanIsGpuSupported(instance, physicalDevice.physicalDevice, logicalDevice.device
#if defined(VK_NO_PROTOTYPES)
                                           , vkGetInstanceProcAddr
                                           , vkGetDeviceProcAddr
#endif
                                           ))
        {
            NVPW_TEST_MESSAGE("Current device is unsupported, test is skipped.");
            return;
        }

        CommandBuffer commandBuffer = VulkanCreateCommandBuffer(logicalDevice.device, logicalDevice.gfxQueueFamilyIndex);
        NVPW_REQUIRE(commandBuffer.commandBuffer != (VkCommandBuffer)VK_NULL_HANDLE);
        auto destroyCommandPool = ScopeExitGuard([&]() {
            vkDestroyCommandPool(logicalDevice.device, commandBuffer.commandPool, nullptr);
        });

        profiler::ReportGeneratorVulkan nvperf;
#if defined(VK_NO_PROTOTYPES)
        NVPW_REQUIRE(nvperf.InitializeReportGenerator(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
        NVPW_REQUIRE(nvperf.InitializeReportGenerator(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
        auto resetNvPerf = ScopeExitGuard([&]() {
            nvperf.Reset();
        });
        nvperf.SetFrameLevelRangeName("Frame");
        nvperf.SetNumNestingLevels(2);

        NVPW_SUBCASE("EmptyFrame")
        {
            auto runEmptyFrame = [&](const ReportOutputOptions& outputOptions) {
                nvperf.outputOptions = outputOptions;
                nvperf.StartCollectionOnNextFrame();
                do
                {
                    VkResult res;
                    auto waitIdle = ScopeExitGuard([&]() {
                        res = vkQueueWaitIdle(logicalDevice.gfxQueue);
                        NVPW_REQUIRE(res == VK_SUCCESS);
                    });
                    VkCommandBufferBeginInfo beginInfo{};
                    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                    beginInfo.pNext = nullptr;
                    beginInfo.flags = VkCommandBufferUsageFlagBits(0);
                    beginInfo.pInheritanceInfo = nullptr;

                    res = vkBeginCommandBuffer(commandBuffer.commandBuffer, &beginInfo);
                    NVPW_REQUIRE(res == VK_SUCCESS);

                    NVPW_REQUIRE(nvperf.rangeCommands.PushRange(commandBuffer.commandBuffer, "Draw"));
                    NVPW_REQUIRE(nvperf.rangeCommands.PopRange(commandBuffer.commandBuffer));

                    res = vkEndCommandBuffer(commandBuffer.commandBuffer);
                    NVPW_REQUIRE(res == VK_SUCCESS);

#if defined(VK_NO_PROTOTYPES)
                    NVPW_REQUIRE(nvperf.OnFrameStart(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                    NVPW_REQUIRE(nvperf.OnFrameStart(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex));
#endif

                    VkSubmitInfo submitInfo{};
                    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                    submitInfo.pNext = nullptr;
                    submitInfo.waitSemaphoreCount = 0;
                    submitInfo.pWaitSemaphores = nullptr;
                    submitInfo.pWaitDstStageMask = nullptr;
                    submitInfo.commandBufferCount = 1;
                    submitInfo.pCommandBuffers = &commandBuffer.commandBuffer;
                    submitInfo.signalSemaphoreCount = 0;
                    submitInfo.pSignalSemaphores = nullptr;

                    res = vkQueueSubmit(logicalDevice.gfxQueue, 1, &submitInfo, VK_NULL_HANDLE);
                    NVPW_REQUIRE(res == VK_SUCCESS);

                    NVPW_REQUIRE(nvperf.OnFrameEnd());

                } while (nvperf.IsCollectingReport());
            };
            
            auto verify = [&](const std::vector<const char*>& existingFileNames, const std::vector<const char*>& nonExistingFileNames) {
                auto fileExists = [&](const char* fileName) {
                    FILE* pFile = OpenFile(fileName, "rb");
                    if (!pFile)
                    {
                        return false;
                    }
                    fclose(pFile);
                    return true;
                };

                const std::string directoryName = nvperf.GetLastReportDirectoryName();
                for (const char* fileName : existingFileNames)
                {
                    std::string fullPath = directoryName + NV_PERF_PATH_SEPARATOR + fileName;
                    NVPW_CHECK(fileExists(fullPath.c_str()));
                }

                for (const char* fileName : nonExistingFileNames)
                {
                    std::string fullPath = directoryName + NV_PERF_PATH_SEPARATOR + fileName;
                    NVPW_CHECK(!fileExists(fullPath.c_str()));
                }
            };

            const std::vector<const char*> HtmlFileNames = {
                "00000_Frame.html",
                "00001_Draw.html",
                "readme.html",
                "summary.html",
            };

            const std::vector<const char*> CsvFileNames = {
                "nvperf_metrics.csv",
                "nvperf_metrics_summary.csv",
            };

#ifdef WIN32
            const std::string directoryName = "HtmlReports\\TestVulkan\\ReportGenerator_EmptyFrame";
#else
            const std::string directoryName = "HtmlReports/TestVulkan/ReportGenerator_EmptyFrame";
#endif

            // HTML + CSV
            {
                ReportOutputOptions outputOptions;
                outputOptions.directoryName = directoryName + "_HTML_CSV";
                outputOptions.appendDateTimeToDirName = AppendDateTime::no;
                runEmptyFrame(outputOptions);
                verify(HtmlFileNames, std::vector<const char*>());
                verify(CsvFileNames, std::vector<const char*>());
            }
            // HTML + CSV - Empty DirectoryName
            {
                for (AppendDateTime appendDataTimeToDirname : {AppendDateTime::yes, AppendDateTime::no})
                {
                    ReportOutputOptions outputOptions;
                    outputOptions.appendDateTimeToDirName = appendDataTimeToDirname;
                    runEmptyFrame(outputOptions);
                    verify(HtmlFileNames, std::vector<const char*>());
                    verify(CsvFileNames, std::vector<const char*>());
                }
            }
            // HTML
            {
                ReportOutputOptions outputOptions;
                outputOptions.directoryName = directoryName + "_HTML";
                outputOptions.enableCsvReport = false;
                runEmptyFrame(outputOptions);
                verify(HtmlFileNames, CsvFileNames);
            }
            // CSV
            {
                ReportOutputOptions outputOptions;
                outputOptions.directoryName = directoryName + "_CSV";
                outputOptions.enableHtmlReport = false;
                runEmptyFrame(outputOptions);
                verify(CsvFileNames, HtmlFileNames);
            }
            // Custom ReportWriter
            {
                ReportOutputOptions outputOptions;
                outputOptions.directoryName = directoryName + "_None"; // this will still create a directory but not write any files
                outputOptions.enableCsvReport = false;
                outputOptions.enableHtmlReport = false;
                bool invoked = false;
                auto customReportWriter = [&](const MetricsEvaluator&, const ReportLayout&, const ReportData&) {
                    invoked = true;
                };
                outputOptions.reportWriters.push_back(customReportWriter);
                runEmptyFrame(outputOptions);
                verify(std::vector<const char*>(), HtmlFileNames);
                verify(std::vector<const char*>(), CsvFileNames);
            }
        }
    }

    NVPW_TEST_SUITE_END();
}}} // namespace nv::perf::test
