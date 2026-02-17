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
#include <NvPerfMiniTraceD3D12.h>
#include <NvPerfScopeExitGuard.h>
#include <nvperf_target.h>

namespace nv { namespace perf { namespace test {

    using Microsoft::WRL::ComPtr;
    using namespace nv::perf::mini_trace;

    NVPW_TEST_SUITE_BEGIN("D3D12");

    NVPW_TEST_CASE("MiniTrace")
    {
        ComPtr<ID3D12Device> pDevice;
        NVPW_REQUIRE(SUCCEEDED(D3D12CreateNvidiaDevice(&pDevice)));

        if (!D3D12IsGpuSupported(pDevice.Get()))
        {
            NVPW_TEST_MESSAGE("Current device is unsupported, test is skipped.");
            return;
        }

        auto createCommandQueue = [&](D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandQueue>& pCommandQueue) {
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = type;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            HRESULT hr = pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue));
            if (FAILED(hr))
            {
                return false;
            }
            return true;
        };

        NVPW_SUBCASE("device state creation/destroy; queue register, unregister")
        {
            ComPtr<ID3D12CommandQueue> pCommandQueue1;
            NVPW_REQUIRE(createCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandQueue1));
            ComPtr<ID3D12CommandQueue> pCommandQueue2;
            NVPW_REQUIRE(createCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE, pCommandQueue2));
            {
                MiniTraceD3D12 trace;
                NVPW_CHECK(trace.Initialize(pDevice.Get()));
                NVPW_CHECK(trace.RegisterQueue(pCommandQueue1.Get()));
                NVPW_CHECK(trace.RegisterQueue(pCommandQueue2.Get()));
                {
                    ScopedNvPerfLogDisabler logDisabler;
                    NVPW_CHECK(!trace.RegisterQueue(pCommandQueue2.Get())); // register the same queue again
                }
            }
            {
                MiniTraceD3D12 trace;
                NVPW_CHECK(trace.Initialize(pDevice.Get()));
                NVPW_CHECK(trace.RegisterQueue(pCommandQueue1.Get()));
                NVPW_CHECK(trace.UnregisterQueue(pCommandQueue1.Get()));
                {
                    ScopedNvPerfLogDisabler logDisabler;
                    NVPW_CHECK(!trace.UnregisterQueue(pCommandQueue1.Get())); // unregister the same queue again
                }
                NVPW_CHECK(trace.RegisterQueue(pCommandQueue2.Get()));
            }
            {
                MiniTraceD3D12 trace;
                NVPW_CHECK(trace.Initialize(pDevice.Get()));
                NVPW_CHECK(trace.RegisterQueue(pCommandQueue1.Get()));
                NVPW_CHECK(trace.UnregisterQueue(pCommandQueue1.Get()));
            }
            {
                MiniTraceD3D12 trace1;
                NVPW_CHECK(trace1.Initialize(pDevice.Get()));
                MiniTraceD3D12 trace2;
                {
                    ScopedNvPerfLogDisabler logDisabler;
                    NVPW_CHECK(!trace2.Initialize(pDevice.Get())); // try to create a 2nd device state on the same D3D12Device
                }
                // no registered queues
            }
        }

        NVPW_SUBCASE("MarkerCpu")
        {
            struct UserData
            {
                uint32_t counter = 0;
            };
            UserData userData;

            ComPtr<ID3D12CommandQueue> pMainCommandQueue;
            NVPW_REQUIRE(createCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, pMainCommandQueue));

            MiniTraceD3D12 trace;
            NVPW_CHECK(trace.Initialize(pDevice.Get()));
            NVPW_CHECK(trace.RegisterQueue(pMainCommandQueue.Get()));

            CommandBuffer commandBuffer;
            NVPW_REQUIRE(commandBuffer.Initialize(pDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT));

            // WAR for closure type not being trivially copyable on certain compilers
            struct MarkerFunc1
            {
                uint32_t* pInvocationCounter1;
                ID3D12CommandQueue* pMainCommandQueue;
                MarkerFunc1(uint32_t* pInvocationCounter1_, ID3D12CommandQueue* pMainCommandQueue_) : pInvocationCounter1(pInvocationCounter1_), pMainCommandQueue(pMainCommandQueue_)
                {
                }
                void operator() (ID3D12CommandQueue* pCommandQueue, uint8_t* pUserData, size_t userDataSize)
                {
                    NVPW_CHECK_EQ(pCommandQueue, pMainCommandQueue);
                    UserData& userDataInCmdList = *reinterpret_cast<UserData*>(pUserData);
                    NVPW_CHECK_EQ(userDataInCmdList.counter, *pInvocationCounter1 * 2);
                    userDataInCmdList.counter += 2;
                    ++(*pInvocationCounter1);
                }
            };
            // note this will make a copy of `userData`
            uint32_t invocationCounter1 = 0;
            NVPW_CHECK(trace.MarkerCpu(commandBuffer.pCommandList.Get(), reinterpret_cast<const uint8_t*>(&userData), sizeof(userData), MarkerFunc1(&invocationCounter1, pMainCommandQueue.Get())));

            // WAR for closure type not being trivially copyable on certain compilers
            struct MarkerFunc2
            {
                uint32_t* pInvocationCounter2;
                ID3D12CommandQueue* pMainCommandQueue;
                MarkerFunc2(uint32_t* pInvocationCounter2_, ID3D12CommandQueue* pMainCommandQueue_) : pInvocationCounter2(pInvocationCounter2_), pMainCommandQueue(pMainCommandQueue_)
                {
                }
                void operator() (ID3D12CommandQueue* pCommandQueue, uint8_t* pUserData, size_t userDataSize)
                {
                    NVPW_CHECK_EQ(pCommandQueue, pMainCommandQueue);
                    UserData& userDataInCmdList = *reinterpret_cast<UserData*>(pUserData);
                    NVPW_CHECK_EQ(userDataInCmdList.counter, *pInvocationCounter2 * 3);
                    userDataInCmdList.counter += 3;
                    ++ (*pInvocationCounter2);
                }
            };
            // note this will make another copy of `userData`
            uint32_t invocationCounter2 = 0;
            NVPW_CHECK(trace.MarkerCpu(commandBuffer.pCommandList.Get(), reinterpret_cast<const uint8_t*>(&userData), sizeof(userData), MarkerFunc2(&invocationCounter2, pMainCommandQueue.Get())));

            // WAR for closure type not being trivially copyable on certain compilers
            struct MarkerFunc3
            {
                uint32_t* pInvocationCounter3;
                ID3D12CommandQueue* pMainCommandQueue;
                MarkerFunc3(uint32_t* pInvocationCounter3_, ID3D12CommandQueue* pMainCommandQueue_) : pInvocationCounter3(pInvocationCounter3_), pMainCommandQueue(pMainCommandQueue_)
                {
                }
                void operator() (ID3D12CommandQueue* pCommandQueue, uint8_t*, size_t)
                {
                    NVPW_CHECK_EQ(pCommandQueue, pMainCommandQueue);
                    ++ (*pInvocationCounter3);
                }
            };
            // test userData == null
            uint32_t invocationCounter3 = 0;
            NVPW_CHECK(trace.MarkerCpu(commandBuffer.pCommandList.Get(), nullptr, 0, MarkerFunc3(&invocationCounter3, pMainCommandQueue.Get())));

            NVPW_CHECK(commandBuffer.CloseList());
            commandBuffer.Execute(pMainCommandQueue.Get());
            NVPW_CHECK(commandBuffer.SignalFence(pMainCommandQueue.Get()));
            NVPW_CHECK(commandBuffer.WaitForCompletion(INFINITE));
            commandBuffer.Execute(pMainCommandQueue.Get());
            NVPW_CHECK(commandBuffer.SignalFence(pMainCommandQueue.Get()));
            NVPW_CHECK(commandBuffer.WaitForCompletion(INFINITE));
            NVPW_CHECK_EQ(userData.counter, 0u); // since we've all been dealing with copies, the original one is not touched
            NVPW_CHECK_EQ(invocationCounter1, 2u);
            NVPW_CHECK_EQ(invocationCounter2, 2u);
            NVPW_CHECK_EQ(invocationCounter3, 2u);
        }

        NVPW_SUBCASE("HostTimestamp")
        {
            for (D3D12_COMMAND_LIST_TYPE queueType : { D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_TYPE_DIRECT })
            {
                ComPtr<ID3D12Resource> pTraceBuffer;
                ComPtr<ID3D12Resource> pReadbackBuffer;
                {
                    D3D12_HEAP_PROPERTIES heapProperties{};
                    {
                        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
                        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
                        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
                        heapProperties.CreationNodeMask = 1;
                        heapProperties.VisibleNodeMask = 1;
                    }

                    D3D12_RESOURCE_DESC resourceDesc{};
                    {
                        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                        resourceDesc.Alignment = 0;
                        resourceDesc.Width = sizeof(NVPW_TimestampReport) * 16;
                        resourceDesc.Height = 1;
                        resourceDesc.DepthOrArraySize = 1;
                        resourceDesc.MipLevels = 1;
                        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
                        resourceDesc.SampleDesc = DXGI_SAMPLE_DESC{1, 0};
                        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
                    }

                    NVPW_REQUIRE(SUCCEEDED(pDevice->CreateCommittedResource(
                        &heapProperties,
                        D3D12_HEAP_FLAG_NONE,
                        &resourceDesc,
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                        nullptr,
                        IID_PPV_ARGS(&pTraceBuffer))));
                    
                    heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
                    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
                    NVPW_REQUIRE(SUCCEEDED(pDevice->CreateCommittedResource(
                        &heapProperties,
                        D3D12_HEAP_FLAG_NONE,
                        &resourceDesc,
                        D3D12_RESOURCE_STATE_COPY_DEST,
                        nullptr,
                        IID_PPV_ARGS(&pReadbackBuffer))));
                }

                ComPtr<ID3D12CommandQueue> pMainCommandQueue;
                NVPW_CHECK(createCommandQueue(queueType, pMainCommandQueue));

                MiniTraceD3D12 trace;
                NVPW_CHECK(trace.Initialize(pDevice.Get()));
                NVPW_CHECK(trace.RegisterQueue(pMainCommandQueue.Get()));

                CommandBuffer commandBuffer;
                uint64_t gpuVA = pTraceBuffer->GetGPUVirtualAddress();
                {
                    // WAR for closure type not being trivially copyable on certain compilers
                    struct AddressFunc1
                    {
                        uint64_t* pGpuVA;
                        AddressFunc1(uint64_t* pGpuVA_) : pGpuVA(pGpuVA_)
                        {
                        }
                        uint64_t operator() (ID3D12CommandQueue*)
                        {
                            const uint64_t ret = *pGpuVA;
                            *pGpuVA += sizeof(NVPW_TimestampReport);
                            return ret;
                        }
                    };
                    struct AddressFunc2
                    {
                        uint64_t operator() (ID3D12CommandQueue*)
                        {
                            return 0; // cancel the current timestamp
                        }
                    };

                    NVPW_REQUIRE(commandBuffer.Initialize(pDevice.Get(), queueType));
                    const uint32_t payload1 = 1;
                    NVPW_CHECK(trace.HostTimestamp(commandBuffer.pCommandList.Get(), payload1, AddressFunc1(&gpuVA)));
                    const uint32_t payload2 = 2;
                    NVPW_CHECK(trace.HostTimestamp(commandBuffer.pCommandList.Get(), payload2, AddressFunc1(&gpuVA)));
                    const uint32_t payload3 = 3;
                    NVPW_CHECK(trace.HostTimestamp(commandBuffer.pCommandList.Get(), payload3, AddressFunc2()));
                    NVPW_CHECK(commandBuffer.CloseList());
                }

                commandBuffer.Execute(pMainCommandQueue.Get());
                NVPW_CHECK(commandBuffer.SignalFence(pMainCommandQueue.Get()));
                NVPW_CHECK(commandBuffer.WaitForCompletion(INFINITE));
                commandBuffer.Execute(pMainCommandQueue.Get());
                NVPW_CHECK(commandBuffer.SignalFence(pMainCommandQueue.Get()));
                NVPW_CHECK(commandBuffer.WaitForCompletion(INFINITE));

                CommandBuffer readbackCommandBuffer;
                {
                    NVPW_REQUIRE(readbackCommandBuffer.Initialize(pDevice.Get(), queueType));
                    D3D12_RESOURCE_BARRIER resourceBarrier{};
                    {
                        resourceBarrier.Transition.pResource = pTraceBuffer.Get();
                        resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                        resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                        resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    }
                    readbackCommandBuffer.pCommandList->ResourceBarrier(1, &resourceBarrier);
                    readbackCommandBuffer.pCommandList->CopyResource(pReadbackBuffer.Get(), pTraceBuffer.Get());
                    NVPW_CHECK(readbackCommandBuffer.CloseList());
                }
                readbackCommandBuffer.Execute(pMainCommandQueue.Get());
                NVPW_CHECK(readbackCommandBuffer.SignalFence(pMainCommandQueue.Get()));
                NVPW_CHECK(readbackCommandBuffer.WaitForCompletion(INFINITE));
                {
                    NVPW_TimestampReport* pMappedBuffer = nullptr;
                    NVPW_REQUIRE(SUCCEEDED(pReadbackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pMappedBuffer))));
                    const NVPW_TimestampReport& timestamp0 = pMappedBuffer[0];
                    NVPW_CHECK(timestamp0.timestamp);
                    NVPW_CHECK_EQ(timestamp0.payload, 1u);
                    const NVPW_TimestampReport& timestamp1 = pMappedBuffer[1];
                    NVPW_CHECK_GE(timestamp1.timestamp, timestamp0.timestamp);
                    NVPW_CHECK_EQ(timestamp1.payload, 2u);
                    const NVPW_TimestampReport& timestamp2 = pMappedBuffer[2];
                    NVPW_CHECK_GE(timestamp2.timestamp, timestamp1.timestamp);
                    NVPW_CHECK_EQ(timestamp2.payload, 1u);
                    const NVPW_TimestampReport& timestamp3 = pMappedBuffer[3];
                    NVPW_CHECK_GE(timestamp3.timestamp, timestamp2.timestamp);
                    NVPW_CHECK_EQ(timestamp3.payload, 2u);
                    pReadbackBuffer->Unmap(0, nullptr);
                }
            }
        }
    }

    NVPW_TEST_CASE("MiniTracer")
    {
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

        NVPW_SUBCASE("Initialize & Begin/EndSession")
        {
            const size_t FrameLatency = 5;

            // Initialization
            {
                MiniTracerD3D12 tracer;
                NVPW_CHECK(!tracer.IsInitialized());
                NVPW_CHECK(tracer.Initialize(pDevice.Get()));
                NVPW_CHECK(tracer.IsInitialized());
                NVPW_CHECK(!tracer.InSession());
            }
            // Initialization + Reset
            {
                MiniTracerD3D12 tracer;
                NVPW_CHECK(tracer.Initialize(pDevice.Get()));
                tracer.Reset();
                NVPW_CHECK(!tracer.IsInitialized());
                NVPW_CHECK(!tracer.InSession());
            }
            // BeginSession before initialization
            {
                MiniTracerD3D12 tracer;
                {
                    ScopedNvPerfLogDisabler logDisabler;
                    NVPW_CHECK(!tracer.BeginSession(pCommandQueue.Get(), FrameLatency)); // BeginSession when !initialized
                }
                NVPW_CHECK(!tracer.IsInitialized());
            }
            // BeginSession + auto EndSession
            {
                MiniTracerD3D12 tracer;
                NVPW_CHECK(tracer.Initialize(pDevice.Get()));
                NVPW_CHECK(tracer.BeginSession(pCommandQueue.Get(), FrameLatency));
                NVPW_CHECK(tracer.IsInitialized());
                NVPW_CHECK(tracer.InSession());
            }
            // BeginSession + Reset
            {
                MiniTracerD3D12 tracer;
                NVPW_CHECK(tracer.Initialize(pDevice.Get()));
                NVPW_CHECK(tracer.BeginSession(pCommandQueue.Get(), FrameLatency));
                tracer.Reset();
                NVPW_CHECK(!tracer.IsInitialized());
                NVPW_CHECK(!tracer.InSession());
            }
            // BeginSession + EndSession
            {
                MiniTracerD3D12 tracer;
                NVPW_CHECK(tracer.Initialize(pDevice.Get()));
                NVPW_CHECK(tracer.BeginSession(pCommandQueue.Get(), FrameLatency));
                tracer.EndSession();
                NVPW_CHECK(tracer.IsInitialized());
                NVPW_CHECK(!tracer.InSession());
            }
            // BeginSession + EndSession + Reset
            {
                MiniTracerD3D12 tracer;
                NVPW_CHECK(tracer.Initialize(pDevice.Get()));
                NVPW_CHECK(tracer.BeginSession(pCommandQueue.Get(), FrameLatency));
                tracer.EndSession();
                tracer.Reset();
                NVPW_CHECK(!tracer.IsInitialized());
                NVPW_CHECK(!tracer.InSession());
            }
            // MoveCtor-1
            {
                MiniTracerD3D12 tracer1;
                NVPW_CHECK(tracer1.Initialize(pDevice.Get()));
                NVPW_CHECK(tracer1.IsInitialized());
                MiniTracerD3D12 tracer2(std::move(tracer1));
                NVPW_CHECK(tracer2.IsInitialized());
                NVPW_CHECK(!tracer2.InSession());
                NVPW_CHECK(!tracer1.IsInitialized());
                NVPW_CHECK(!tracer1.InSession());
            }
            // MoveCtor-2
            {
                MiniTracerD3D12 tracer1;
                NVPW_CHECK(tracer1.Initialize(pDevice.Get()));
                NVPW_CHECK(tracer1.BeginSession(pCommandQueue.Get(), FrameLatency));
                MiniTracerD3D12 tracer2(std::move(tracer1));
                NVPW_CHECK(tracer2.IsInitialized());
                NVPW_CHECK(tracer2.InSession());
                NVPW_CHECK(!tracer1.IsInitialized());
                NVPW_CHECK(!tracer1.InSession());
            }
            // MoveAssignment-1
            {
                MiniTracerD3D12 tracer1;
                NVPW_CHECK(tracer1.Initialize(pDevice.Get()));
                MiniTracerD3D12 tracer2;
                tracer2 = std::move(tracer1);
                NVPW_CHECK(tracer2.IsInitialized());
                NVPW_CHECK(!tracer2.InSession());
                NVPW_CHECK(!tracer1.IsInitialized());
                NVPW_CHECK(!tracer1.InSession());
            }
            // MoveAssignment-2
            {
                MiniTracerD3D12 tracer1;
                NVPW_CHECK(tracer1.Initialize(pDevice.Get()));
                NVPW_CHECK(tracer1.BeginSession(pCommandQueue.Get(), FrameLatency));
                MiniTracerD3D12 tracer2;
                tracer2 = std::move(tracer1);
                NVPW_CHECK(tracer2.IsInitialized());
                NVPW_CHECK(tracer2.InSession());
                NVPW_CHECK(!tracer1.IsInitialized());
                NVPW_CHECK(!tracer1.InSession());
            }
        }

        NVPW_SUBCASE("Functional")
        {
            const size_t FrameLatency = 5;

            MiniTracerD3D12 tracer;
            NVPW_CHECK(tracer.Initialize(pDevice.Get()));
            NVPW_CHECK(tracer.BeginSession(pCommandQueue.Get(), FrameLatency));
            auto endSessionGuard = ScopeExitGuard([&]() {
                tracer.EndSession();
            });

            std::vector<CommandBuffer> commandBuffers(FrameLatency);
            for (CommandBuffer& commandBuffer : commandBuffers)
            {
                NVPW_REQUIRE(commandBuffer.Initialize(pDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT));
            }

            auto waitForCompletion = ScopeExitGuard([&]() {
                auto& commandBuffer = commandBuffers[0];
                NVPW_CHECK(commandBuffer.SignalFence(pCommandQueue.Get()));
                NVPW_CHECK(commandBuffer.WaitForCompletion(INFINITE));
            });

            uint64_t lastFrameEndTime = 0;
            for (size_t frameIdx = 0; frameIdx < 1000; ++frameIdx)
            {
                CommandBuffer& commandBuffer = commandBuffers[frameIdx % FrameLatency];
                if (commandBuffer.fenceValue && !commandBuffer.IsCompleted())
                {
                    commandBuffer.WaitForCompletion(INFINITE);
                }
                // this should be user's workload, but for testing purpose only, we don't have any actual workload here
                NVPW_CHECK(tracer.OnFrameEnd());
                NVPW_CHECK(commandBuffer.SignalFence(pCommandQueue.Get())); // this actually pushes a fence onto the queue despite it's a CommandBuffer method
                while (true)
                {
                    MiniTracerD3D12::FrameData frameData{};
                    bool isDataReady = false;
                    NVPW_CHECK(tracer.GetOldestFrameData(isDataReady, frameData));
                    if (!isDataReady)
                    {
                        break;
                    }
                    NVPW_CHECK(frameData.frameEndTime > lastFrameEndTime);
                    lastFrameEndTime = frameData.frameEndTime;
                    NVPW_CHECK(tracer.ReleaseOldestFrame());
                }
            }
        }
    }

    NVPW_TEST_SUITE_END();
}}} // namespace nv::perf::test
