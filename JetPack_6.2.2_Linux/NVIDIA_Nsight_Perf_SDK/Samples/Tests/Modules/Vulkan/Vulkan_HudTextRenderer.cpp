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

// required for rapidyaml
#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#include "VulkanUtilities.h"

#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <ios>
#include <iostream>
#include <thread>

#include <NvPerfPeriodicSamplerVulkan.h>
#include <NvPerfScopeExitGuard.h>
#include <NvPerfVulkan.h>

#define RYML_SINGLE_HDR_DEFINE_NOW
#include <NvPerfHudDataModel.h>
#include <NvPerfHudTextRenderer.h>


namespace nv { namespace perf { namespace test {

    using namespace nv::perf::sampler;

    NVPW_TEST_SUITE_BEGIN("Vulkan");

    NVPW_TEST_CASE("PeriodicSamplerHudTextRenderer")
    {
        VkInstance instance = VulkanCreateInstance("NvPerf Vulkan Samples Mini Trace Test");
        NVPW_REQUIRE(instance != (VkInstance)VK_NULL_HANDLE);
        auto destroyInstance = ScopeExitGuard([&]() {
            vkDestroyInstance(instance, nullptr);
        });

        PhysicalDevice physicalDevice = VulkanFindNvidiaPhysicalDevice(instance);
        NVPW_REQUIRE(physicalDevice.physicalDevice != (VkPhysicalDevice)VK_NULL_HANDLE);

        LogicalDevice logicalDevice = VulkanCreateLogicalDeviceWithGraphicsAndAsyncQueues(instance, physicalDevice);
        NVPW_REQUIRE(logicalDevice.device != (VkDevice)VK_NULL_HANDLE);
        NVPW_REQUIRE(logicalDevice.gfxQueue != (VkQueue)VK_NULL_HANDLE);
        NVPW_REQUIRE(logicalDevice.asyncComputeQueue != (VkQueue)VK_NULL_HANDLE);
        auto destroyDevice = ScopeExitGuard([&]() {
            vkDestroyDevice(logicalDevice.device, nullptr);
        });

        NVPW_REQUIRE(VulkanLoadDriver(instance));
        if (!VulkanIsGpuSupported(instance, physicalDevice.physicalDevice, logicalDevice.device
#if defined(VK_NO_PROTOTYPES)
                                 , vkGetInstanceProcAddr
                                 , vkGetDeviceProcAddr
#endif
                                 ))
        {
            NVPW_TEST_MESSAGE("Current device is unsupported, test is skipped.");
            return;
        }

        const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice.physicalDevice, logicalDevice.device
#if defined(VK_NO_PROTOTYPES)
                                                             , vkGetInstanceProcAddr
                                                             , vkGetDeviceProcAddr
#endif
                                                             );
        NVPW_REQUIRE(deviceIndex != (size_t)~0);
        const DeviceIdentifiers deviceIdentifiers = GetDeviceIdentifiers(deviceIndex);

        PeriodicSamplerTimeHistoryVulkan sampler;
#if defined(VK_NO_PROTOTYPES)
        NVPW_REQUIRE(sampler.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
        NVPW_REQUIRE(sampler.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
        const uint32_t samplingFrequency = 60; // 60 Hz
        const uint32_t samplingIntervalInNanoSeconds = 1000 * 1000 * 1000 / samplingFrequency; // sampling frequency is fixed @ 60 Hz
        const uint32_t maxDecodeLatencyInNanoSeconds = 1000 * 1000 * 1000; // tolerate stutter frame up to 1 second
        const size_t maxFrameLatency = 3;
        NVPW_REQUIRE(sampler.BeginSession(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, samplingIntervalInNanoSeconds, maxDecodeLatencyInNanoSeconds, maxFrameLatency));
        auto endSessionGuard = ScopeExitGuard([&]() {
            NVPW_REQUIRE(sampler.EndSession());
        });

        // initialize hud data model
        nv::perf::hud::HudPresets hudPresets;
        nv::perf::hud::HudDataModel hudDataModel;
        hudPresets.Initialize(deviceIdentifiers.pChipName);
        const double plotTimeWidthInSeconds = 4;
        hudDataModel.Load(hudPresets.GetPreset("Graphics General Triage"));
        hudDataModel.Initialize(1.0 / (double)samplingFrequency, plotTimeWidthInSeconds);
        // text renderer
        nv::perf::hud::HudTextRenderer hudTextRenderer;
        const auto printFn = [](const char* format, va_list list)
        {
            //vfprintf(stdout, format, list);
            char buffer[256];
            int ret = vsnprintf(buffer, 256, format, list);
            NVPW_REQUIRE(ret != -1);
        };
        hudTextRenderer.SetConsoleOutput(printFn);
        hudTextRenderer.SetColumnSeparator("|");

        NVPW_REQUIRE(sampler.SetConfig(&hudDataModel.GetCounterConfiguration()));
        hudDataModel.PrepareSampleProcessing(sampler.GetCounterData());
        hudTextRenderer.Initialize(hudDataModel);

        for(int frameNum = 0; frameNum < 10; frameNum++)
        {
            NVPW_CHECK(sampler.DecodeCounters());

            sampler.ConsumeSamples([&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop) {
                stop = false;
                return hudDataModel.AddSample(pCounterDataImage, counterDataImageSize, rangeIndex);
            });
            for (auto& frameDelimiter : sampler.GetFrameDelimiters())
            {
                hudDataModel.AddFrameDelimiter(frameDelimiter.frameEndTime);
            }
            hudTextRenderer.Render();

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            NVPW_REQUIRE(sampler.OnFrameEnd());
        }
        NVPW_REQUIRE(sampler.StopSampling());
    }

    NVPW_TEST_SUITE_END();
}}} // namespace nv::perf::test
