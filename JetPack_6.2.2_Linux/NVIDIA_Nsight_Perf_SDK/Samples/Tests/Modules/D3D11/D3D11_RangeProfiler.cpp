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
#include "D3D11Utilities.h"
#include <NvPerfRangeProfilerD3D11.h>
#include <NvPerfScopeExitGuard.h>

namespace nv { namespace perf { namespace test {
    NVPW_TEST_SUITE_BEGIN("D3D11");

    NVPW_TEST_CASE("RangeProfiler_DeferredContextNegativeTest")
    {
        Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> pImmediateContext;
        HRESULT hr = D3D11CreateNvidiaDevice(&pDevice, &pImmediateContext);
        NVPW_REQUIRE(SUCCEEDED(hr));

        Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeferredContext;
        hr = pDevice->CreateDeferredContext(0, &pDeferredContext);
        NVPW_REQUIRE(SUCCEEDED(hr));

        if (!profiler::D3D11IsGpuSupported(pDevice.Get()))
        {
            NVPW_TEST_MESSAGE("Current device is unsupported, test is skipped.");
            return;
        }

        nv::perf::profiler::RangeProfilerD3D11 rangeProfiler;

        nv::perf::profiler::SessionOptions sessionOptions;
        // Profiler session cannot be started with deferred context
        {
            ScopedNvPerfLogDisabler logDisabler;
            NVPW_REQUIRE(!rangeProfiler.BeginSession(pDeferredContext.Get(), sessionOptions));
        }

        NVPW_SUBCASE("ProfilerNotInBadState")
        {
            NVPW_REQUIRE(rangeProfiler.BeginSession(pImmediateContext.Get(), sessionOptions));
            NVPW_REQUIRE(rangeProfiler.EndSession());
        }
    }

    NVPW_TEST_CASE("RangeProfiler_FrameSynchronous")
    {
        Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> pImmediateContext;
        HRESULT hr = D3D11CreateNvidiaDevice(&pDevice, &pImmediateContext);
        NVPW_REQUIRE(SUCCEEDED(hr));

        if (!profiler::D3D11IsGpuSupported(pDevice.Get()))
        {
            NVPW_TEST_MESSAGE("Current device is unsupported, test is skipped.");
            return;
        }

        nv::perf::profiler::RangeProfilerD3D11 rangeProfiler;

        NVPW_SUBCASE("EmptyFrame")
        {
            const nv::perf::DeviceIdentifiers deviceIdentifiers = nv::perf::D3D11GetDeviceIdentifiers(pDevice.Get());
            NVPW_REQUIRE(deviceIdentifiers.pChipName);

            const size_t scratchBufferSize = nv::perf::D3D11CalculateMetricsEvaluatorScratchBufferSize(deviceIdentifiers.pChipName);
            NVPW_REQUIRE(scratchBufferSize != 0u);
            std::vector<uint8_t> scratchBuffer(scratchBufferSize);

            NVPW_MetricsEvaluator* pMetricsEvaluator = nv::perf::D3D11CreateMetricsEvaluator(scratchBuffer.data(), scratchBuffer.size(), deviceIdentifiers.pChipName);
            NVPW_REQUIRE(pMetricsEvaluator);

            nv::perf::MetricsEvaluator metricsEvaluator(pMetricsEvaluator, std::move(scratchBuffer));

            NVPA_RawMetricsConfig* rawMetricsConfig = nv::perf::profiler::D3D11CreateRawMetricsConfig(deviceIdentifiers.pChipName);
            NVPW_REQUIRE(rawMetricsConfig);

            nv::perf::MetricsConfigBuilder configBuilder;
            NVPW_REQUIRE(configBuilder.Initialize(metricsEvaluator, rawMetricsConfig, deviceIdentifiers.pChipName));

            std::vector<const char*> metricNames;
            NVPW_SUBCASE("OnePass")
            {
                metricNames = {
                    "sm__cycles_active.avg",
                };
            }
            NVPW_SUBCASE("MultiPass")
            {
                metricNames = {
                    "smsp__inst_executed_pipe_adu.sum",
                    "smsp__inst_executed_pipe_alu.sum",
                    "smsp__inst_executed_pipe_cbu.sum",
                    "smsp__inst_executed_pipe_fma.sum",
                    "smsp__inst_executed_pipe_fp64.sum",
                    "smsp__inst_executed_pipe_ipa.sum",
                    "smsp__inst_executed_pipe_lsu.sum",
                    "smsp__inst_executed_pipe_tensor.sum",
                    "smsp__inst_executed_pipe_tex.sum",
                    "smsp__inst_executed_pipe_xu.sum",
                };
            }

            for (const char* metricName : metricNames)
            {
                NVPW_REQUIRE(configBuilder.AddMetric(metricName));
            }

            nv::perf::CounterConfiguration counterConfig;
            NVPW_REQUIRE(nv::perf::CreateConfiguration(configBuilder, counterConfig));

            nv::perf::profiler::SessionOptions sessionOptions;
            NVPW_REQUIRE(rangeProfiler.BeginSession(pImmediateContext.Get(), sessionOptions));
            auto endSessionGuard = ScopeExitGuard([&]() {
                NVPW_REQUIRE(D3D11Finish(pDevice.Get(), pImmediateContext.Get()));
                NVPW_REQUIRE(rangeProfiler.EndSession());
            });

            const uint16_t numNestingLevels = 1u;
            NVPW_REQUIRE(rangeProfiler.EnqueueCounterCollection(counterConfig, numNestingLevels));

            // Collect frames
            nv::perf::profiler::DecodeResult decodeResult = {};
            do {
                NVPW_REQUIRE(rangeProfiler.BeginPass());
                NVPW_REQUIRE(rangeProfiler.PushRange("Frame"));

                NVPW_REQUIRE(rangeProfiler.PopRange()); // Frame
                NVPW_REQUIRE(rangeProfiler.EndPass());

                NVPW_REQUIRE(D3D11Finish(pDevice.Get(), pImmediateContext.Get()));
                NVPW_REQUIRE(rangeProfiler.DecodeCounters(decodeResult));
            } while (!decodeResult.allStatisticalSamplesCollected);

            nv::perf::MetricsEvaluatorSetDeviceAttributes(metricsEvaluator, decodeResult.counterDataImage.data(), decodeResult.counterDataImage.size());

            std::vector<NVPW_MetricEvalRequest> metricEvalRequests;
            metricEvalRequests.reserve(metricNames.size());
            for (const char* metricName : metricNames)
            {
                NVPW_MetricEvalRequest metricEvalRequest;
                NVPW_REQUIRE(nv::perf::ToMetricEvalRequest(metricsEvaluator, metricName, metricEvalRequest));
                metricEvalRequests.push_back(metricEvalRequest);
            }

            const size_t numRanges = nv::perf::CounterDataGetNumRanges(decodeResult.counterDataImage.data());
            std::vector<double> metricValues(metricEvalRequests.size());
            for (size_t rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex)
            {
                NVPW_REQUIRE(nv::perf::EvaluateToGpuValues(pMetricsEvaluator, decodeResult.counterDataImage.data(), decodeResult.counterDataImage.size(), rangeIndex, metricEvalRequests.size(), metricEvalRequests.data(), metricValues.data()));
                for (double value : metricValues)
                {
                    NVPW_REQUIRE(!std::isnan(value));
                }
            }
        }
    }

    NVPW_TEST_SUITE_END();
}}} // namespace nv::perf::test
