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
#include <NvPerfReportGeneratorD3D11.h>
#include <NvPerfScopeExitGuard.h>

namespace nv { namespace perf { namespace test {
    NVPW_TEST_SUITE_BEGIN("D3D11");

    NVPW_TEST_CASE("ReportGenerator_DeferredContextNegativeTest")
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

        Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeferredContext;
        hr = pDevice->CreateDeferredContext(0, &pDeferredContext);
        NVPW_REQUIRE(SUCCEEDED(hr));

        nv::perf::profiler::ReportGeneratorD3D11 nvperf;
        NVPW_REQUIRE(nvperf.InitializeReportGenerator(pDevice.Get()));

        NVPW_REQUIRE(nvperf.StartCollectionOnNextFrame());

        // Report generator cannot be used with deferred context
        {
            ScopedNvPerfLogDisabler logDisabler;
            NVPW_REQUIRE(!nvperf.OnFrameStart(pDeferredContext.Get()));
        }

        nvperf.Reset();
    }

    NVPW_TEST_CASE("ReportGenerator")
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

        nv::perf::profiler::ReportGeneratorD3D11 nvperf;
        NVPW_REQUIRE(nvperf.InitializeReportGenerator(pDevice.Get()));
        auto resetNvPerf = ScopeExitGuard([&]() {
            nvperf.Reset();
        });
        NVPW_CHECK(nvperf.GetFrameLevelRangeName() == "");
        NVPW_CHECK(nvperf.GetNumNestingLevels() == 1);

        nvperf.SetFrameLevelRangeName("Frame");
        NVPW_CHECK(nvperf.GetFrameLevelRangeName() == "Frame");

        nvperf.SetNumNestingLevels(2);
        NVPW_CHECK(nvperf.GetNumNestingLevels() == 2);

        NVPW_CHECK(!nvperf.IsCollectingReport());

        NVPW_SUBCASE("EmptyFrame")
        {
            nvperf.outputOptions.directoryName = "HtmlReports\\TestD3D11\\ReportGenerator";
            NVPW_REQUIRE(nvperf.StartCollectionOnNextFrame());

            do {
                auto finishD3D11 = ScopeExitGuard([&]() {
                    D3D11Finish(pDevice.Get(), pImmediateContext.Get());
                });
                NVPW_REQUIRE(nvperf.OnFrameStart(pImmediateContext.Get()));

                nvperf.PushRange("Draw");
                nvperf.PopRange();

                NVPW_REQUIRE(nvperf.OnFrameEnd());
            } while (nvperf.IsCollectingReport());

            auto fileExists = [](const char* fileName) {
                FILE* pFile = OpenFile(fileName, "rb");
                if (!pFile)
                {
                    return false;
                }
                fclose(pFile);
                return true;
            };

            const char* fileNames[] = {
                "00000_Frame.html",
                "00001_Draw.html",
                "nvperf_metrics.csv",
                "nvperf_metrics_summary.csv",
                "readme.html",
                "summary.html",
            };

            const std::string reportDirectoryName = nvperf.GetLastReportDirectoryName();
            for (const char* fileName : fileNames)
            {
                std::string fullPath = reportDirectoryName + fileName;
                NVPW_CHECK(fileExists(fullPath.c_str()));
            }
        }
    }

    NVPW_TEST_SUITE_END();
}}} // namespace nv::perf::test
