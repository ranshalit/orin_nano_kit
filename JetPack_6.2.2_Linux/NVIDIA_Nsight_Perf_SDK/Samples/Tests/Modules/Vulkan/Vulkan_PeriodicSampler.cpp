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
#include <iostream>
#include <array>
#include <ios>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cmath>
#include <NvPerfVulkan.h>
#include <NvPerfPeriodicSamplerVulkan.h>
#include <NvPerfScopeExitGuard.h>
#include <nvperf_target.h>

namespace nv { namespace perf { namespace test {

    using namespace nv::perf::sampler;

    NVPW_TEST_SUITE_BEGIN("Vulkan");

    NVPW_TEST_CASE("PeriodicSamplerTimeHistory")
    {
        const std::array<const char*, 3> metrics = {
            "gr__cycles_elapsed.sum",
            "gr__cycles_active.avg",
            "smsp__warps_launched.sum"
        };
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

        MetricsEvaluator metricsEvaluator;
        {
            std::vector<uint8_t> metricsEvaluatorScratchBuffer;
            NVPW_MetricsEvaluator* pMetricsEvaluator = DeviceCreateMetricsEvaluator(metricsEvaluatorScratchBuffer, deviceIdentifiers.pChipName);
            NVPW_REQUIRE(pMetricsEvaluator);
            metricsEvaluator = MetricsEvaluator(pMetricsEvaluator, std::move(metricsEvaluatorScratchBuffer)); // transfer ownership to metricsEvaluator
        }

        std::vector<NVPW_MetricEvalRequest> metricEvalRequests;
        for (size_t ii = 0; ii < metrics.size(); ++ii)
        {
            NVPW_MetricEvalRequest request;
            NVPW_REQUIRE(ToMetricEvalRequest(metricsEvaluator, metrics[ii], request));
            metricEvalRequests.push_back(request);
        }

        CounterConfiguration configuration;
        {
            NVPA_RawMetricsConfig* pRawMetricsConfig = DeviceCreateRawMetricsConfig(deviceIdentifiers.pChipName);
            NVPW_REQUIRE(pRawMetricsConfig);

            MetricsConfigBuilder configBuilder;
            NVPW_REQUIRE(configBuilder.Initialize(metricsEvaluator, pRawMetricsConfig, deviceIdentifiers.pChipName));
            NVPW_REQUIRE(configBuilder.AddMetrics(metricEvalRequests.data(), metricEvalRequests.size()));
            NVPW_REQUIRE(nv::perf::CreateConfiguration(configBuilder, configuration));
            NVPW_REQUIRE(configuration.numPasses == 1u);
        }

        PeriodicSamplerTimeHistoryVulkan sampler;
#if defined(VK_NO_PROTOTYPES)
        NVPW_REQUIRE(sampler.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
        NVPW_REQUIRE(sampler.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
        const uint32_t samplingIntervalInNanoSeconds = 1000 * 1000 * 1000 / 60; // sampling frequency is fixed @ 60 Hz
        const uint32_t maxDecodeLatencyInNanoSeconds = 1000 * 1000 * 1000; // tolerate stutter frame up to 1 second
        const size_t maxFrameLatency = 3;
        NVPW_REQUIRE(sampler.BeginSession(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, samplingIntervalInNanoSeconds, maxDecodeLatencyInNanoSeconds, maxFrameLatency));
        auto endSessionGuard = ScopeExitGuard([&]() {
            NVPW_REQUIRE(sampler.EndSession());
        });
        NVPW_REQUIRE(sampler.SetConfig(&configuration));

        {
            const std::vector<uint8_t>& counterData = sampler.GetCounterData();
            NVPW_REQUIRE(MetricsEvaluatorSetDeviceAttributes(metricsEvaluator, counterData.data(), counterData.size()));
        }
        NVPW_REQUIRE(sampler.StartSampling());
        std::vector<double> metricValues(metricEvalRequests.size());
        size_t numSamplesCollected = 0;
        uint64_t lastTimestamp = 0;
        while (numSamplesCollected < 150)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            NVPW_CHECK(sampler.DecodeCounters());

            NVPW_CHECK(sampler.ConsumeSamples([&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop) {
                stop = false;

                const bool success = EvaluateToGpuValues(
                    metricsEvaluator,
                    pCounterDataImage,
                    counterDataImageSize,
                    rangeIndex,
                    metricEvalRequests.size(),
                    metricEvalRequests.data(),
                    metricValues.data());
                NVPW_CHECK(success);

                SampleTimestamp timestamp{};
                NVPW_CHECK(CounterDataGetSampleTime(pCounterDataImage, rangeIndex, timestamp));

                const bool verbosePrint = false;
                if (verbosePrint)
                {
                    std::cout << "[" << numSamplesCollected << "]" << timestamp.end << ", " << std::fixed << std::setprecision(2);
                    for (size_t ii = 0; ii < metricValues.size(); ++ii)
                    {
                        std::cout << metricValues[ii] << ", ";
                    }
                    std::cout << std::endl;
                }

                for (size_t ii = 0; ii < metricValues.size(); ++ii)
                {
                    NVPW_CHECK(!std::isnan(metricValues[ii]));
                }
                NVPW_CHECK(timestamp.end > lastTimestamp);
                lastTimestamp = timestamp.end;

                ++numSamplesCollected;
                return true;
            }));
        }
        NVPW_REQUIRE(sampler.StopSampling());
    }

    NVPW_TEST_SUITE_END();
}}} // namespace nv::perf::test
