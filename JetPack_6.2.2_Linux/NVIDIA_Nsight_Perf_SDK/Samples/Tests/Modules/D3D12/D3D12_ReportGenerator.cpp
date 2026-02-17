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
#include <NvPerfReportGeneratorD3D12.h>
#include <NvPerfScopeExitGuard.h>

namespace nv { namespace perf { namespace test {
    NVPW_TEST_SUITE_BEGIN("D3D12");

    NVPW_TEST_CASE("ReportGenerator")
    {
        Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
        HRESULT hResult = D3D12CreateNvidiaDevice(&pDevice);
        NVPW_REQUIRE(SUCCEEDED(hResult));

        if (!profiler::D3D12IsGpuSupported(pDevice.Get()))
        {
            NVPW_TEST_MESSAGE("Current device is unsupported, test is skipped.");
            return;
        }

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> pQueue;
        hResult = pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pQueue));
        NVPW_REQUIRE(SUCCEEDED(hResult));

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator;
        hResult = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCommandAllocator));
        NVPW_REQUIRE(SUCCEEDED(hResult));

        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList;
        hResult = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&pCommandList));
        NVPW_REQUIRE(SUCCEEDED(hResult));
        hResult = pCommandList->Close();
        NVPW_REQUIRE(SUCCEEDED(hResult));

        UINT64 fenceValue = 0;
        Microsoft::WRL::ComPtr<ID3D12Fence> pFence;
        hResult = pDevice->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));
        NVPW_REQUIRE(SUCCEEDED(hResult));

        HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        NVPW_REQUIRE(fenceEvent);
        auto closeEventHandle = ScopeExitGuard([&]() {
            NVPW_REQUIRE(CloseHandle(fenceEvent));
        });

        auto WaitForGPU = [&]() {
            pQueue->Signal(pFence.Get(), ++fenceValue);
            if (pFence->GetCompletedValue() < fenceValue)
            {
                pFence->SetEventOnCompletion(fenceValue, fenceEvent);
                WaitForSingleObject(fenceEvent, INFINITE);
            }
        };

        profiler::ReportGeneratorD3D12 nvperf;
        NVPW_REQUIRE(nvperf.InitializeReportGenerator(pDevice.Get()));
        auto resetNvPerf = ScopeExitGuard([&]() {
            nvperf.Reset();
        });
        nvperf.SetFrameLevelRangeName("Frame");
        nvperf.SetNumNestingLevels(2);

        NVPW_SUBCASE("EmptyFrame")
        {    
            nvperf.outputOptions.directoryName = "HtmlReports\\TestD3D12\\ReportGenerator_EmptyFrame";
            nvperf.StartCollectionOnNextFrame();
            do
            {
                auto waitForGPU = ScopeExitGuard([&]() {
                    WaitForGPU();
                });
                hResult = pCommandAllocator->Reset();
                NVPW_REQUIRE(SUCCEEDED(hResult));

                hResult = pCommandList->Reset(pCommandAllocator.Get(), nullptr);
                NVPW_REQUIRE(SUCCEEDED(hResult));

                NVPW_REQUIRE(nvperf.rangeCommands.PushRange(pCommandList.Get(), "Draw"));
                NVPW_REQUIRE(nvperf.rangeCommands.PopRange(pCommandList.Get()));

                hResult = pCommandList->Close();
                NVPW_REQUIRE(SUCCEEDED(hResult));

                NVPW_REQUIRE(nvperf.OnFrameStart(pQueue.Get()));
                ID3D12CommandList* pCommandLists[] = {pCommandList.Get()};
                pQueue->ExecuteCommandLists(1, pCommandLists);
                NVPW_REQUIRE(nvperf.OnFrameEnd());

            } while (nvperf.IsCollectingReport());

            auto fileExists = [&](const char* fileName) {
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
