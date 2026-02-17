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
#include "D3D12Utilities.h"
#include <iostream>
#include <array>
#include <ios>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cmath>
#include <NvPerfD3D12.h>
#include <NvPerfPeriodicSamplerD3D12.h>
#include <NvPerfScopeExitGuard.h>
#include <nvperf_target.h>

namespace nv { namespace perf { namespace test {

    using namespace nv::perf::sampler;
    using Microsoft::WRL::ComPtr;

    NVPW_TEST_SUITE_BEGIN("D3D12");

    NVPW_TEST_CASE("PeriodicSamplerTimeHistory")
    {
        const std::array<const char*, 3> metrics = {
            "gr__cycles_elapsed.sum",
            "gr__cycles_active.avg",
            "smsp__warps_launched.sum"
        };

        ComPtr<ID3D12Device> pDevice;
        NVPW_REQUIRE(SUCCEEDED(D3D12CreateNvidiaDevice(&pDevice)));

        if (!D3D12IsGpuSupported(pDevice.Get()))
        {
            NVPW_TEST_MESSAGE("Current device is unsupported, test is skipped.");
            return;
        }

        ComPtr<ID3D12CommandQueue> pCommandQueue;
        {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            NVPW_REQUIRE(SUCCEEDED(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue))));
        }

        const size_t deviceIndex = D3D12GetNvperfDeviceIndex(pDevice.Get(), 0);
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

        PeriodicSamplerTimeHistoryD3D12 sampler;
        NVPW_REQUIRE(sampler.Initialize(pDevice.Get()));
        const uint32_t samplingIntervalInNanoSeconds = 1000 * 1000 * 1000 / 60; // sampling frequency is fixed @ 60 Hz
        const uint32_t maxDecodeLatencyInNanoSeconds = 1000 * 1000 * 1000; // tolerate stutter frame up to 1 second
        const size_t maxFrameLatency = 3;
        NVPW_REQUIRE(sampler.BeginSession(pCommandQueue.Get(), samplingIntervalInNanoSeconds, maxDecodeLatencyInNanoSeconds, maxFrameLatency));
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

        {
            const std::vector<uint8_t> counterData = sampler.GetCounterData();
            CounterDataInfo counterDataInfo;
            NVPW_REQUIRE(CounterDataGetInfo(counterData.data(), counterData.size(), counterDataInfo));
            const size_t numRangesInCounterDataSrc = counterDataInfo.numTotalRanges;
            NVPW_REQUIRE(numRangesInCounterDataSrc > 10);

            std::vector<double> values; // one for each range
            std::vector<SampleTimestamp> timestamps; // one for each range
            for (size_t rangeIndex = 0; rangeIndex < 10; ++rangeIndex)
            {
                values.push_back(0);
                timestamps.push_back(SampleTimestamp());

                bool success = EvaluateToGpuValues(
                    metricsEvaluator,
                    counterData.data(),
                    counterData.size(),
                    rangeIndex,
                    1,
                    &metricEvalRequests[0],
                    &values.back());
                NVPW_CHECK(success);
                NVPW_CHECK(values.back()); // select a metric that always have non-zero values

                NVPW_CHECK(CounterDataGetSampleTime(counterData.data(), rangeIndex, timestamps.back()));
            }

            // Test SampleCombiner
            {
                /*
                    Timeline:
                                    +--------+--------+--------+
                    Sample          |   S0   |   S1   |   S2   |
                                    +--------+--------+--------+
                           ^     ^      ^    ^    ^       ^       ^   ^
                           |     |      |    |    |       |       |   |
                           T0    T1     T2   T3   T4      T5      T6  T7
                    Window      Expected Value
                    T0 - T1:    0
                    T1 - T2:    S0 * 0.5
                    T1 - T6:    S0       + S1       + S2
                    T2 - T5:    S0 * 0.5 + S1       + S2 * 0.5
                    T3 - T4:    S1 * 0.5
                    T5 - T6:    S2 * 0.5
                    T6 - T7:    0
                */
                NVPW_CHECK(timestamps[0].start > 2048);
                const uint64_t t0 = timestamps[0].start - 2048;
                const uint64_t t1 = timestamps[0].start - 1024;
                const uint64_t t2 = (timestamps[0].start + timestamps[0].end) / 2;
                const uint64_t t3 = timestamps[1].start;
                const uint64_t t4 = (timestamps[1].start + timestamps[1].end) / 2;
                const uint64_t t5 = (timestamps[2].start + timestamps[2].end) / 2;
                const uint64_t t6 = timestamps[2].end + 1024;
                const uint64_t t7 = timestamps[2].end + 2048;

                SampleCombiner<2> combiner;
                NVPW_CHECK(combiner.Initialize(configuration.counterDataPrefix, counterData));

                const std::vector<decltype(combiner)::SampleInfo> samples = {
                    { timestamps[0].start, timestamps[0].end, counterData.data(), 0 },
                    { timestamps[1].start, timestamps[1].end, counterData.data(), 1 },
                    { timestamps[2].start, timestamps[2].end, counterData.data(), 2 },
                };

                // T0 - T1:    0(not invoked)
                {
                    bool invoked = false;
                    NVPW_CHECK(combiner.MergeSamples(samples, t0, t1, [&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex) {
                        invoked = true;
                        return true;
                    }));
                    NVPW_CHECK(!invoked);
                }
                // T1 - T2:    S0 * 0.5
                {
                    double value = 0.0;
                    NVPW_CHECK(combiner.MergeSamples(samples, t1, t2, [&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex) {
                        NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, pCounterDataImage, counterDataImageSize, rangeIndex, 1, &metricEvalRequests[0], &value));
                        return true;
                    }));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[0] * 0.5).epsilon(0.01));
                }
                // T1 - T6:    S0       + S1       + S2
                {
                    double value = 0.0;
                    NVPW_CHECK(combiner.MergeSamples(samples, t1, t6, [&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex) {
                        NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, pCounterDataImage, counterDataImageSize, rangeIndex, 1, &metricEvalRequests[0], &value));
                        return true;
                    }));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[0] + values[1] + values[2]).epsilon(0.01));
                }
                // T2 - T5:    S0 * 0.5 + S1       + S2 * 0.5
                {
                    double value = 0.0;
                    NVPW_CHECK(combiner.MergeSamples(samples, t2, t5, [&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex) {
                        NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, pCounterDataImage, counterDataImageSize, rangeIndex, 1, &metricEvalRequests[0], &value));
                        return true;
                    }));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[0] * 0.5 + values[1] + values[2] * 0.5).epsilon(0.01));
                }
                // Test reset
                combiner.Reset();
                NVPW_CHECK(combiner.Initialize(configuration.counterDataPrefix, counterData));
                // T3 - T4:    S1 * 0.5
                {
                    double value = 0.0;
                    NVPW_CHECK(combiner.MergeSamples(samples, t3, t4, [&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex) {
                        NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, pCounterDataImage, counterDataImageSize, rangeIndex, 1, &metricEvalRequests[0], &value));
                        return true;
                    }));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[1] * 0.5).epsilon(0.01));
                }
                // T5 - T6:    S2 * 0.5
                {
                    double value = 0.0;
                    NVPW_CHECK(combiner.MergeSamples(samples, t5, t6, [&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex) {
                        NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, pCounterDataImage, counterDataImageSize, rangeIndex, 1, &metricEvalRequests[0], &value));
                        return true;
                    }));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[2] * 0.5).epsilon(0.01));
                }
                // T6 - T7:    0
                {
                    bool invoked = false;
                    NVPW_CHECK(combiner.MergeSamples(samples, t6, t7, [&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex) {
                        invoked = true;
                        return true;
                    }));
                    NVPW_CHECK(!invoked);
                }
                // consumeDataFunc returns false
                {
                    NVPW_CHECK(!combiner.MergeSamples(samples, t5, t6, [&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex) {
                        return false;
                    }));
                }
            }

            // Test FrameLevelSampleCombiner
            {
                /*
                                                 +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
                    Sample Timeline(S)           |   S0   |   S1   |   S2   |   S3   |   S4   |   S5   |   S6   |   S7   |   S8   |   S9   |
                                                 +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
                                          ^    ^     ^   ^         ^        ^    ^       ^                                            ^                     ^     ^
                                          |    |     |   |         |        |    |       |                                            |                     |     |
                    Frame Timeline(F)     F0   F1    F2  F3        F4       F5   F6      F7                                           F8                    F9    F10
                    Overlap Factor        0    0     0.5 0.4       0.1+1    1.0  0.5     0.5+0.5                                      0.5+1+1+1+1+0.5       0.5   0
                */
                FrameLevelSampleCombiner combiner;
                NVPW_CHECK(combiner.Initialize(configuration.counterDataPrefix, counterData, numRangesInCounterDataSrc));
                // F0
                NVPW_CHECK(timestamps[0].start > 2048);
                const uint64_t timestamp_f0 = timestamps[0].start - 2048;
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(!combiner.IsDataComplete(timestamp_f0));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f0, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == 0);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f0);
                    NVPW_CHECK(!frameInfo.numSamplesInFrame);
                }
                // F1
                const uint64_t timestamp_f1 = timestamps[0].start - 1024;
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(!combiner.IsDataComplete(timestamp_f1));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f1, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == timestamp_f0);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f1);
                    NVPW_CHECK(!frameInfo.numSamplesInFrame);
                }
                // SO
                NVPW_CHECK(combiner.AddSample(counterData.data(), counterData.size(), 0));
                // F2
                const uint64_t timestamp_f2 = (timestamps[0].start + timestamps[0].end) / 2;
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(combiner.IsDataComplete(timestamp_f2));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f2, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == timestamp_f1);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f2);
                    NVPW_CHECK(frameInfo.numSamplesInFrame == 1);

                    double value = 0.0;
                    NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, frameInfo.pCombinedCounterData, frameInfo.combinedCounterDataSize, frameInfo.combinedCounterDataRangeIndex, 1, &metricEvalRequests[0], &value));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[0] * 0.5).epsilon(0.01));
                }
                // S1
                NVPW_CHECK(combiner.AddSample(counterData.data(), counterData.size(), 1));
                // F3
                const uint64_t timestamp_f3 = timestamps[0].start + uint64_t((timestamps[0].end - timestamps[0].start) * 0.9);
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(combiner.IsDataComplete(timestamp_f3));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f3, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == timestamp_f2);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f3);
                    NVPW_CHECK(frameInfo.numSamplesInFrame == 1);

                    double value = 0.0;
                    NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, frameInfo.pCombinedCounterData, frameInfo.combinedCounterDataSize, frameInfo.combinedCounterDataRangeIndex, 1, &metricEvalRequests[0], &value));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[0] * 0.4).epsilon(0.01));
                }
                // S2
                NVPW_CHECK(combiner.AddSample(counterData.data(), counterData.size(), 2));
                // F4
                const uint64_t timestamp_f4 = timestamps[1].end;
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(combiner.IsDataComplete(timestamp_f4));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f4, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == timestamp_f3);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f4);
                    NVPW_CHECK(frameInfo.numSamplesInFrame == 2);

                    double value = 0.0;
                    NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, frameInfo.pCombinedCounterData, frameInfo.combinedCounterDataSize, frameInfo.combinedCounterDataRangeIndex, 1, &metricEvalRequests[0], &value));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[0] * 0.1 + values[1]).epsilon(0.01));
                }
                // S3
                NVPW_CHECK(combiner.AddSample(counterData.data(), counterData.size(), 3));
                // F5
                const uint64_t timestamp_f5 = timestamps[2].end;
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(combiner.IsDataComplete(timestamp_f5));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f5, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == timestamp_f4);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f5);
                    NVPW_CHECK(frameInfo.numSamplesInFrame == 1);

                    double value = 0.0;
                    NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, frameInfo.pCombinedCounterData, frameInfo.combinedCounterDataSize, frameInfo.combinedCounterDataRangeIndex, 1, &metricEvalRequests[0], &value));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[2]).epsilon(0.01));
                }
                // F6
                const uint64_t timestamp_f6 = (timestamps[3].start + timestamps[3].end) / 2;
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(combiner.IsDataComplete(timestamp_f6));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f6, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == timestamp_f5);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f6);
                    NVPW_CHECK(frameInfo.numSamplesInFrame == 1);

                    double value = 0.0;
                    NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, frameInfo.pCombinedCounterData, frameInfo.combinedCounterDataSize, frameInfo.combinedCounterDataRangeIndex, 1, &metricEvalRequests[0], &value));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[3] * 0.5).epsilon(0.01));
                }
                // S4
                NVPW_CHECK(combiner.AddSample(counterData.data(), counterData.size(), 4));
                // F7
                const uint64_t timestamp_f7 = (timestamps[4].start + timestamps[4].end) / 2;
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(combiner.IsDataComplete(timestamp_f7));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f7, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == timestamp_f6);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f7);
                    NVPW_CHECK(frameInfo.numSamplesInFrame == 2);

                    double value = 0.0;
                    NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, frameInfo.pCombinedCounterData, frameInfo.combinedCounterDataSize, frameInfo.combinedCounterDataRangeIndex, 1, &metricEvalRequests[0], &value));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[3] * 0.5 + values[4] * 0.5).epsilon(0.01));
                }
                // S5-9
                NVPW_CHECK(combiner.AddSample(counterData.data(), counterData.size(), 5));
                NVPW_CHECK(combiner.AddSample(counterData.data(), counterData.size(), 6));
                NVPW_CHECK(combiner.AddSample(counterData.data(), counterData.size(), 7));
                NVPW_CHECK(combiner.AddSample(counterData.data(), counterData.size(), 8));
                NVPW_CHECK(combiner.AddSample(counterData.data(), counterData.size(), 9));
                // F8
                const uint64_t timestamp_f8 = (timestamps[9].start + timestamps[9].end) / 2;
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(combiner.IsDataComplete(timestamp_f8));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f8, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == timestamp_f7);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f8);
                    NVPW_CHECK(frameInfo.numSamplesInFrame == 6);

                    double value = 0.0;
                    NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, frameInfo.pCombinedCounterData, frameInfo.combinedCounterDataSize, frameInfo.combinedCounterDataRangeIndex, 1, &metricEvalRequests[0], &value));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[4] * 0.5 + values[5] + values[6] + values[7] + values[8] + values[9] * 0.5).epsilon(0.01));
                }
                // F9
                const uint64_t timestamp_f9 = timestamps[9].end + 1024;
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(!combiner.IsDataComplete(timestamp_f9));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f9, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == timestamp_f8);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f9);
                    NVPW_CHECK(frameInfo.numSamplesInFrame == 1);

                    double value = 0.0;
                    NVPW_CHECK(EvaluateToGpuValues(metricsEvaluator, frameInfo.pCombinedCounterData, frameInfo.combinedCounterDataSize, frameInfo.combinedCounterDataRangeIndex, 1, &metricEvalRequests[0], &value));
                    NVPW_CHECK(!std::isnan(value));
                    NVPW_CHECK(value == doctest::Approx(values[9] * 0.5).epsilon(0.01));
                }
                // F10
                const uint64_t timestamp_f10 = timestamp_f9 + 1024;
                {
                    FrameLevelSampleCombiner::FrameInfo frameInfo{};
                    NVPW_CHECK(!combiner.IsDataComplete(timestamp_f10));
                    NVPW_CHECK(combiner.GetCombinedSamples(timestamp_f10, frameInfo));
                    NVPW_CHECK(frameInfo.beginTimestamp == timestamp_f9);
                    NVPW_CHECK(frameInfo.endTimestamp == timestamp_f10);
                    NVPW_CHECK(!frameInfo.numSamplesInFrame);
                }
            }
        }
    }

    NVPW_TEST_SUITE_END();
}}} // namespace nv::perf::test
