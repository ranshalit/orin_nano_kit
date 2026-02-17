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
#include <array>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>
#include <NvPerfPeriodicSamplerGpu.h>
#include <NvPerfScopeExitGuard.h>
#include <NvPerfMetricsEvaluator.h>
#include <NvPerfMetricsConfigBuilder.h>
#include <NvPerfCounterConfiguration.h>
#include <NvPerfCounterData.h>
#include "DeviceUtilities.h"

namespace nv { namespace perf { namespace test {

    using namespace nv::perf::sampler;

    NVPW_TEST_SUITE_BEGIN("GpuPeriodicSampler");

    NVPW_TEST_CASE("BeginEndSession")
    {
        const size_t deviceIndex = GetCompatibleGpuDeviceIndex();
        if (deviceIndex == size_t(~0))
        {
            NVPW_TEST_MESSAGE("Current device is unsupported, test is skipped.");
            return;
        }

        const size_t MaxNumUndecodedSamplingRanges = 1;
        const size_t RecordBufferSize = 256 * 1024 * 1024; // 256 MB

        NVPW_SUBCASE("InvalidDeviceIndex")
        {
            ScopedNvPerfLogDisabler logDisabler;
            GpuPeriodicSampler sampler;
            const size_t InvalidDeviceIndex = 1024;
            NVPW_CHECK(!sampler.Initialize(InvalidDeviceIndex));
        }

        NVPW_SUBCASE("NoSession")
        {
            GpuPeriodicSampler sampler;
            NVPW_CHECK(sampler.Initialize(deviceIndex));
            NVPW_CHECK(sampler.GetDeviceIndex() == deviceIndex);
            NVPW_CHECK(sampler.GetSupportedTriggers().size());
            NVPW_CHECK(sampler.IsTriggerSupported(NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_CPU_SYSCALL));
            NVPW_CHECK(sampler.IsTriggerSupported(NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_SYSCLK_INTERVAL));
            NVPW_CHECK(sampler.IsTriggerSupported(NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_ENGINE_TRIGGER));
        }

        NVPW_SUBCASE("BeginSession + EndSession")
        {
            GpuPeriodicSampler sampler;
            NVPW_CHECK(sampler.Initialize(deviceIndex));
            NVPW_CHECK(sampler.BeginSession(RecordBufferSize, MaxNumUndecodedSamplingRanges, { NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_CPU_SYSCALL }, 0));
            NVPW_CHECK(sampler.EndSession());
        }

        NVPW_SUBCASE("BeginSession + Reset")
        {
            GpuPeriodicSampler sampler;
            NVPW_CHECK(sampler.Initialize(deviceIndex));
            NVPW_CHECK(sampler.IsInitialized());
            NVPW_CHECK(sampler.BeginSession(RecordBufferSize, MaxNumUndecodedSamplingRanges, { NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_CPU_SYSCALL }, 0));
            sampler.Reset();
            NVPW_CHECK(!sampler.IsInitialized());
        }

        NVPW_SUBCASE("MoveCtor-1")
        {
            GpuPeriodicSampler sampler1;
            NVPW_CHECK(sampler1.Initialize(deviceIndex));
            NVPW_CHECK(sampler1.IsInitialized());
            GpuPeriodicSampler sampler2(std::move(sampler1));
            NVPW_CHECK(sampler2.IsInitialized());
            NVPW_CHECK(!sampler1.IsInitialized());
        }

        NVPW_SUBCASE("MoveCtor-2")
        {
            GpuPeriodicSampler sampler1;
            NVPW_CHECK(sampler1.Initialize(deviceIndex));
            NVPW_CHECK(sampler1.IsInitialized());
            NVPW_CHECK(sampler1.BeginSession(RecordBufferSize, MaxNumUndecodedSamplingRanges, { NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_CPU_SYSCALL }, 0));
            GpuPeriodicSampler sampler2(std::move(sampler1));
            NVPW_CHECK(sampler2.IsInitialized());
            NVPW_CHECK(!sampler1.IsInitialized());
        }

        NVPW_SUBCASE("MoveAssignment-1")
        {
            GpuPeriodicSampler sampler1;
            NVPW_CHECK(sampler1.Initialize(deviceIndex));
            NVPW_CHECK(sampler1.IsInitialized());
            GpuPeriodicSampler sampler2;
            sampler2 = std::move(sampler1);
            NVPW_CHECK(sampler2.IsInitialized());
            NVPW_CHECK(!sampler1.IsInitialized());
        }

        NVPW_SUBCASE("MoveAssignment-2")
        {
            GpuPeriodicSampler sampler1;
            NVPW_CHECK(sampler1.Initialize(deviceIndex));
            NVPW_CHECK(sampler1.IsInitialized());
            NVPW_CHECK(sampler1.BeginSession(RecordBufferSize, MaxNumUndecodedSamplingRanges, { NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_CPU_SYSCALL }, 0));
            GpuPeriodicSampler sampler2;
            sampler2 = std::move(sampler1);
            NVPW_CHECK(sampler2.IsInitialized());
            NVPW_CHECK(!sampler1.IsInitialized());
        }
    }

    NVPW_TEST_CASE("CpuTrigger")
    {
        const size_t deviceIndex = GetCompatibleGpuDeviceIndex();
        if (deviceIndex == size_t(~0))
        {
            NVPW_TEST_MESSAGE("Current device is unsupported, test is skipped.");
            return;
        }
        const DeviceIdentifiers deviceIdentifiers = GetDeviceIdentifiers(deviceIndex);

        const std::array<const char*, 2> metrics = { 
            "gr__cycles_active.avg",
            "smsp__warps_launched.sum"
        };

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

        const size_t MaxNumUndecodedSamplingRanges = 1;
        const size_t MaxNumUndecodedSamples = 1024;
        size_t recordBufferSize = 0;
        {
            NVPW_REQUIRE(GpuPeriodicSamplerCalculateRecordBufferSize(deviceIndex, configuration.configImage, MaxNumUndecodedSamples, recordBufferSize));
            NVPW_REQUIRE(recordBufferSize);
        }

        std::vector<uint8_t> counterDataImage;
        NVPW_REQUIRE(GpuPeriodicSamplerCreateCounterData(deviceIndex, configuration.counterDataPrefix.data(), configuration.counterDataPrefix.size(), MaxNumUndecodedSamples, NVPW_PERIODIC_SAMPLER_COUNTER_DATA_APPEND_MODE_LINEAR, counterDataImage));
        NVPW_REQUIRE(counterDataImage.size());

        GpuPeriodicSampler sampler;
        NVPW_REQUIRE(sampler.Initialize(deviceIndex));
        NVPW_REQUIRE(sampler.BeginSession(recordBufferSize, MaxNumUndecodedSamplingRanges, { NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_CPU_SYSCALL }, 0));
        NVPW_REQUIRE(sampler.SetConfig(configuration.configImage, 0));
        size_t totalSize = 0;
        size_t usedSize = 0;
        bool overflow = false; 
        NVPW_CHECK(sampler.GetRecordBufferStatus(totalSize, usedSize, overflow));
        NVPW_CHECK(totalSize);
        NVPW_CHECK(!usedSize);
        NVPW_CHECK(!overflow);

        NVPW_REQUIRE(sampler.StartSampling());
        const size_t NumTriggers = 4;
        for (size_t ii = 0; ii < NumTriggers; ++ii)
        {
            sampler.CpuTrigger();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        }
        NVPW_REQUIRE(sampler.StopSampling());

        {
            NVPW_CHECK(sampler.GetRecordBufferStatus(totalSize, usedSize, overflow));
            NVPW_CHECK(usedSize);
            NVPW_CHECK(!overflow);

            const size_t NumSamplingRangesToDecode = 1;
            size_t numSamplingRangesDecoded = 0;
            size_t numSamplesDropped = 0;
            size_t numSamplesMerged = 0;
            NVPW_REQUIRE(sampler.DecodeCounters(
                counterDataImage,
                NumSamplingRangesToDecode,
                numSamplingRangesDecoded,
                overflow,
                numSamplesDropped,
                numSamplesMerged));
            NVPW_CHECK(numSamplingRangesDecoded == NumSamplingRangesToDecode);
            NVPW_CHECK(!overflow);
            NVPW_CHECK(!numSamplesDropped);
            NVPW_CHECK(!numSamplesMerged);
        }

        const size_t numRanges = CounterDataGetNumRanges(counterDataImage.data());
        NVPW_CHECK(numRanges == NumTriggers);
        {
            CounterDataInfo counterDataInfo{};
            NVPW_CHECK(CounterDataGetInfo(counterDataImage.data(), counterDataImage.size(), counterDataInfo));
            NVPW_CHECK(counterDataInfo.numTotalRanges == MaxNumUndecodedSamples);
            NVPW_CHECK(counterDataInfo.numPopulatedRanges == NumTriggers);
            NVPW_CHECK(counterDataInfo.numCompletedRanges == NumTriggers);
        }
        {
            size_t counterDataImageTrimmedSize = 0;
            NVPW_CHECK(CounterDataTrimInPlace(counterDataImage.data(), counterDataImage.size(), counterDataImageTrimmedSize));
            NVPW_CHECK(counterDataImageTrimmedSize < counterDataImage.size());
            counterDataImage.erase(counterDataImage.begin() + counterDataImageTrimmedSize, counterDataImage.end());
        }
        {
            // check it again
            CounterDataInfo counterDataInfo{};
            NVPW_CHECK(CounterDataGetInfo(counterDataImage.data(), counterDataImage.size(), counterDataInfo));
            NVPW_CHECK(counterDataInfo.numTotalRanges == NumTriggers);
            NVPW_CHECK(counterDataInfo.numPopulatedRanges == NumTriggers);
            NVPW_CHECK(counterDataInfo.numCompletedRanges == NumTriggers);

            const size_t numRanges_ = CounterDataGetNumRanges(counterDataImage.data());
            NVPW_CHECK(numRanges_ == NumTriggers);
        }

        NVPW_REQUIRE(MetricsEvaluatorSetDeviceAttributes(metricsEvaluator, counterDataImage.data(), counterDataImage.size()));
        std::vector<double> metricValues(metricEvalRequests.size());
        SampleTimestamp lastTimestamp{};
        for (size_t rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex)
        {
            const bool success = EvaluateToGpuValues(
                metricsEvaluator,
                counterDataImage.data(),
                counterDataImage.size(),
                rangeIndex,
                metricEvalRequests.size(),
                metricEvalRequests.data(),
                metricValues.data());
            NVPW_CHECK(success);
            for (double value : metricValues)
            {
                NVPW_CHECK(!std::isnan(value));
            }

            SampleTimestamp timestamp{};
            NVPW_CHECK(CounterDataGetSampleTime(counterDataImage.data(), rangeIndex, timestamp));
            NVPW_CHECK(timestamp.end > timestamp.start);
            NVPW_CHECK(timestamp.end > lastTimestamp.end);
            lastTimestamp = timestamp;

            uint32_t triggerCount = 0;
            NVPW_CHECK(CounterDataGetTriggerCount(counterDataImage.data(), counterDataImage.size(), rangeIndex, triggerCount));
            NVPW_CHECK(triggerCount == rangeIndex + 1);
        }
    }

    NVPW_TEST_SUITE_END();

}}} // namespace nv::perf::test