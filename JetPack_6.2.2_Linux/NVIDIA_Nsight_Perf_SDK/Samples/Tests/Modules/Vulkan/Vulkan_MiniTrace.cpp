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
#include <NvPerfMiniTraceVulkan.h>
#include <NvPerfScopeExitGuard.h>
#include <nvperf_target.h>

namespace nv { namespace perf { namespace test {

    using namespace nv::perf::mini_trace;

    NVPW_TEST_SUITE_BEGIN("Vulkan");

    NVPW_TEST_CASE("MiniTrace")
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

        NVPW_SUBCASE("device state creation/destroy; queue register, unregister")
        {
            {
                MiniTraceVulkan trace;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK(trace.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                NVPW_CHECK(trace.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
                NVPW_CHECK(trace.RegisterQueue(logicalDevice.gfxQueue));
                NVPW_CHECK(trace.RegisterQueue(logicalDevice.asyncComputeQueue));
                {
                    ScopedNvPerfLogDisabler logDisabler;
                    NVPW_CHECK(!trace.RegisterQueue(logicalDevice.asyncComputeQueue)); // register the same queue again
                }
            }
            {
                MiniTraceVulkan trace;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK(trace.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                NVPW_CHECK(trace.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
                NVPW_CHECK(trace.RegisterQueue(logicalDevice.gfxQueue));
                NVPW_CHECK(trace.UnregisterQueue(logicalDevice.gfxQueue));
                {
                    ScopedNvPerfLogDisabler logDisabler;
                    NVPW_CHECK(!trace.UnregisterQueue(logicalDevice.gfxQueue)); // unregister the same queue again
                }
                NVPW_CHECK(trace.RegisterQueue(logicalDevice.asyncComputeQueue));
            }
            {
                MiniTraceVulkan trace;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK(trace.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                NVPW_CHECK(trace.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
                NVPW_CHECK(trace.RegisterQueue(logicalDevice.gfxQueue));
                NVPW_CHECK(trace.UnregisterQueue(logicalDevice.gfxQueue));
            }
            {
                MiniTraceVulkan trace1;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK(trace1.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                NVPW_CHECK(trace1.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
                MiniTraceVulkan trace2;
                {
                    ScopedNvPerfLogDisabler logDisabler;
#if defined(VK_NO_PROTOTYPES)
                    NVPW_CHECK(!trace2.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr)); // try to create a 2nd device state on the same device
#else
                    NVPW_CHECK(!trace2.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device)); // try to create a 2nd device state on the same device
#endif
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

            MiniTraceVulkan trace;
#if defined(VK_NO_PROTOTYPES)
            NVPW_CHECK(trace.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
            NVPW_CHECK(trace.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
            NVPW_CHECK(trace.RegisterQueue(logicalDevice.gfxQueue));

            CommandBuffer commandBuffer = VulkanCreateCommandBuffer(logicalDevice.device, logicalDevice.gfxQueueFamilyIndex);
            NVPW_REQUIRE(commandBuffer.commandBuffer != (VkCommandBuffer)VK_NULL_HANDLE);
            auto destroyCommandPool = ScopeExitGuard([&]() {
                vkDestroyCommandPool(logicalDevice.device, commandBuffer.commandPool, nullptr);
            });

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.pNext = nullptr;
            beginInfo.flags = VkCommandBufferUsageFlagBits(0);
            beginInfo.pInheritanceInfo = nullptr;

            VkResult vkResult = vkBeginCommandBuffer(commandBuffer.commandBuffer, &beginInfo);
            NVPW_REQUIRE(vkResult == VK_SUCCESS);

            // WAR for closure type not being trivially copyable on certain compilers
            struct MarkerFunc1
            {
                uint32_t* pInvocationCounter1;
                VkQueue gfxQueue;
                MarkerFunc1(uint32_t* pInvocationCounter1_, VkQueue gfxQueue_) : pInvocationCounter1(pInvocationCounter1_), gfxQueue(gfxQueue_)
                {
                }
                void operator() (VkQueue queue, uint8_t* pUserData, size_t userDataSize)
                {
                    NVPW_CHECK_EQ(queue, gfxQueue);
                    UserData& userDataInCmdList = *reinterpret_cast<UserData*>(pUserData);
                    NVPW_CHECK_EQ(userDataInCmdList.counter, *pInvocationCounter1 * 2);
                    userDataInCmdList.counter += 2;
                    ++(*pInvocationCounter1);
                }
            };
            // note this will make a copy of `userData`
            uint32_t invocationCounter1 = 0;
            NVPW_CHECK(trace.MarkerCpu(commandBuffer.commandBuffer, reinterpret_cast<const uint8_t*>(&userData), sizeof(userData), MarkerFunc1(&invocationCounter1, logicalDevice.gfxQueue)));

            // WAR for closure type not being trivially copyable on certain compilers
            struct MarkerFunc2
            {
                uint32_t* pInvocationCounter2;
                VkQueue gfxQueue;
                MarkerFunc2(uint32_t* pInvocationCounter2_, VkQueue gfxQueue_) : pInvocationCounter2(pInvocationCounter2_), gfxQueue(gfxQueue_)
                {
                }
                void operator() (VkQueue queue, uint8_t* pUserData, size_t userDataSize)
                {
                    NVPW_CHECK_EQ(queue, gfxQueue);
                    UserData& userDataInCmdList = *reinterpret_cast<UserData*>(pUserData);
                    NVPW_CHECK_EQ(userDataInCmdList.counter, *pInvocationCounter2 * 3);
                    userDataInCmdList.counter += 3;
                    ++(*pInvocationCounter2);
                }
            };
            // note this will make another copy of `userData`
            uint32_t invocationCounter2 = 0;
            NVPW_CHECK(trace.MarkerCpu(commandBuffer.commandBuffer, reinterpret_cast<const uint8_t*>(&userData), sizeof(userData), MarkerFunc2(&invocationCounter2, logicalDevice.gfxQueue)));

            // WAR for closure type not being trivially copyable on certain compilers
            struct MarkerFunc3
            {
                uint32_t* pInvocationCounter3;
                VkQueue gfxQueue;
                MarkerFunc3(uint32_t* pInvocationCounter3_, VkQueue gfxQueue_) : pInvocationCounter3(pInvocationCounter3_), gfxQueue(gfxQueue_)
                {
                }
                void operator() (VkQueue queue, uint8_t*, size_t)
                {
                    NVPW_CHECK_EQ(queue, gfxQueue);
                    ++(*pInvocationCounter3);
                }
            };
            // test userData == null
            uint32_t invocationCounter3 = 0;
            NVPW_CHECK(trace.MarkerCpu(commandBuffer.commandBuffer, nullptr, 0, MarkerFunc3(&invocationCounter3, logicalDevice.gfxQueue)));

            vkResult = vkEndCommandBuffer(commandBuffer.commandBuffer);
            NVPW_REQUIRE(vkResult == VK_SUCCESS);

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

            vkResult = vkQueueSubmit(logicalDevice.gfxQueue, 1, &submitInfo, VK_NULL_HANDLE);
            NVPW_REQUIRE(vkResult == VK_SUCCESS);

            vkResult = vkQueueWaitIdle(logicalDevice.gfxQueue);
            NVPW_REQUIRE(vkResult == VK_SUCCESS);

            // execute it again
            vkResult = vkQueueSubmit(logicalDevice.gfxQueue, 1, &submitInfo, VK_NULL_HANDLE);
            NVPW_REQUIRE(vkResult == VK_SUCCESS);

            vkResult = vkQueueWaitIdle(logicalDevice.gfxQueue);
            NVPW_REQUIRE(vkResult == VK_SUCCESS);

            NVPW_CHECK_EQ(userData.counter, 0u); // since we've all been dealing with copies, the original one is not touched
            NVPW_CHECK_EQ(invocationCounter1, 2u);
            NVPW_CHECK_EQ(invocationCounter2, 2u);
            NVPW_CHECK_EQ(invocationCounter3, 2u);
        }

        NVPW_SUBCASE("HostTimestamp")
        {
            struct TestSetup
            {
                VkQueue queue = VK_NULL_HANDLE;
                uint32_t queueFamilyIndex = (uint32_t)~0;
            };

            VkResult vkResult = VK_SUCCESS;
            for (const TestSetup& testSetup : { TestSetup{logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex}, TestSetup{logicalDevice.asyncComputeQueue, logicalDevice.asyncComputeQueueFamilyIndex} })
            {
                VkQueue queue = testSetup.queue;
                const uint32_t queueFamilyIndex = testSetup.queueFamilyIndex;

                Buffer buffer = VulkanCreateBuffer(
                    physicalDevice,
                    logicalDevice.device,
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    sizeof(NVPW_TimestampReport) * 16);
                NVPW_REQUIRE(buffer.buffer != (VkBuffer)VK_NULL_HANDLE);
                NVPW_REQUIRE(buffer.deviceMemory != (VkDeviceMemory)VK_NULL_HANDLE);
                auto freeMemory = ScopeExitGuard([&]() {
                    vkDestroyBuffer(logicalDevice.device, buffer.buffer, nullptr);
                    vkFreeMemory(logicalDevice.device, buffer.deviceMemory, nullptr);
                });

                MiniTraceVulkan trace;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK(trace.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                NVPW_CHECK(trace.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
                NVPW_CHECK(trace.RegisterQueue(queue));

                CommandBuffer commandBuffer = VulkanCreateCommandBuffer(logicalDevice.device, queueFamilyIndex);
                NVPW_REQUIRE(commandBuffer.commandBuffer != (VkCommandBuffer)VK_NULL_HANDLE);
                auto destroyCommandPool = ScopeExitGuard([&]() {
                    vkDestroyCommandPool(logicalDevice.device, commandBuffer.commandPool, nullptr);
                });

                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.pNext = nullptr;
                beginInfo.flags = VkCommandBufferUsageFlagBits(0);
                beginInfo.pInheritanceInfo = nullptr;

                vkResult = vkBeginCommandBuffer(commandBuffer.commandBuffer, &beginInfo);
                NVPW_REQUIRE(vkResult == VK_SUCCESS);
                VkBufferDeviceAddressInfo bufferDeviceAddressInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
                bufferDeviceAddressInfo.buffer = buffer.buffer;

                VkDeviceAddress deviceAddress = 0;
                {
                    PFN_vkGetBufferDeviceAddress pfnVkGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddress)vkGetDeviceProcAddr(logicalDevice.device, "vkGetBufferDeviceAddress");
                    if (pfnVkGetBufferDeviceAddress)
                    {
                        deviceAddress = pfnVkGetBufferDeviceAddress(logicalDevice.device, &bufferDeviceAddressInfo);
                    }
                    else
                    {
                        PFN_vkGetBufferDeviceAddressEXT pfnVkGetBufferDeviceAddressEXT = (PFN_vkGetBufferDeviceAddressEXT)vkGetDeviceProcAddr(logicalDevice.device, "vkGetBufferDeviceAddressEXT");
                        NVPW_REQUIRE(pfnVkGetBufferDeviceAddressEXT);
                        deviceAddress = pfnVkGetBufferDeviceAddressEXT(logicalDevice.device, &bufferDeviceAddressInfo);
                    }
                }
                NVPW_REQUIRE(deviceAddress);
                uint64_t gpuVA = deviceAddress;
                {
                    // WAR for closure type not being trivially copyable on certain compilers
                    struct AddressFunc1
                    {
                        uint64_t* pGpuVA;
                        AddressFunc1(uint64_t* pGpuVA_) : pGpuVA(pGpuVA_)
                        {
                        }
                        uint64_t operator() (VkQueue)
                        {
                            const uint64_t ret = *pGpuVA;
                            *pGpuVA += sizeof(NVPW_TimestampReport);
                            return ret;
                        }
                    };
                    struct AddressFunc2
                    {
                        uint64_t operator() (VkQueue)
                        {
                            return 0; // cancel the current timestamp
                        }
                    };

                    const uint32_t payload1 = 1;
                    NVPW_CHECK(trace.HostTimestamp(commandBuffer.commandBuffer, payload1, AddressFunc1(&gpuVA)));
                    const uint32_t payload2 = 2;
                    NVPW_CHECK(trace.HostTimestamp(commandBuffer.commandBuffer, payload2, AddressFunc1(&gpuVA)));
                    const uint32_t payload3 = 3;
                    NVPW_CHECK(trace.HostTimestamp(commandBuffer.commandBuffer, payload3, AddressFunc2()));
                }
                vkResult = vkEndCommandBuffer(commandBuffer.commandBuffer);
                NVPW_REQUIRE(vkResult == VK_SUCCESS);

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

                vkResult = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
                NVPW_REQUIRE(vkResult == VK_SUCCESS);

                vkResult = vkQueueWaitIdle(queue);
                NVPW_REQUIRE(vkResult == VK_SUCCESS);

                vkResult = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
                NVPW_REQUIRE(vkResult == VK_SUCCESS);

                vkResult = vkQueueWaitIdle(queue);
                NVPW_REQUIRE(vkResult == VK_SUCCESS);

                {
                    NVPW_TimestampReport* pTimestampReports = nullptr;
                    vkResult = vkMapMemory(logicalDevice.device, buffer.deviceMemory, 0, VK_WHOLE_SIZE, 0, (void**)&pTimestampReports);
                    NVPW_REQUIRE(vkResult == VK_SUCCESS);
                    auto unmap = ScopeExitGuard([&]() {
                        vkUnmapMemory(logicalDevice.device, buffer.deviceMemory);
                    });

                    const NVPW_TimestampReport& timestamp0 = pTimestampReports[0];
                    NVPW_CHECK(timestamp0.timestamp);
                    NVPW_CHECK_EQ(timestamp0.payload, 1u);
                    const NVPW_TimestampReport& timestamp1 = pTimestampReports[1];
                    NVPW_CHECK_GE(timestamp1.timestamp, timestamp0.timestamp);
                    NVPW_CHECK_EQ(timestamp1.payload, 2u);
                    const NVPW_TimestampReport& timestamp2 = pTimestampReports[2];
                    NVPW_CHECK_GE(timestamp2.timestamp, timestamp1.timestamp);
                    NVPW_CHECK_EQ(timestamp2.payload, 1u);
                    const NVPW_TimestampReport& timestamp3 = pTimestampReports[3];
                    NVPW_CHECK_GE(timestamp3.timestamp, timestamp2.timestamp);
                    NVPW_CHECK_EQ(timestamp3.payload, 2u);
                }
            }
        }
    }

    NVPW_TEST_CASE("MiniTracer")
    {
        VkInstance instance = VulkanCreateInstance("NvPerf Vulkan Samples Mini Tracer Test");
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

        NVPW_SUBCASE("Initialize & Begin/EndSession")
        {
            const size_t FrameLatency = 5;

            // Initialization
            {
                MiniTracerVulkan tracer;
                NVPW_CHECK(!tracer.IsInitialized());
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK(tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                NVPW_CHECK(tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
                NVPW_CHECK(tracer.IsInitialized());
                NVPW_CHECK(!tracer.InSession());
            }
            // Initialization + Reset
            {
                MiniTracerVulkan tracer;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK((tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr)));
#else
                NVPW_CHECK((tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device)));
#endif
                tracer.Reset();
                NVPW_CHECK(!tracer.IsInitialized());
                NVPW_CHECK(!tracer.InSession());
            }
            // BeginSession before initialization
            {
                MiniTracerVulkan tracer;
                {
                    ScopedNvPerfLogDisabler logDisabler;
                    NVPW_CHECK(!tracer.BeginSession(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, FrameLatency));
                }
                NVPW_CHECK(!tracer.IsInitialized());
            }
            // BeginSession + auto EndSession
            {
                MiniTracerVulkan tracer;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK((tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr)));
#else
                NVPW_CHECK((tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device)));
#endif
                NVPW_CHECK(tracer.BeginSession(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, FrameLatency));
                NVPW_CHECK(tracer.IsInitialized());
                NVPW_CHECK(tracer.InSession());
            }
            // BeginSession + Reset
            {
                MiniTracerVulkan tracer;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK((tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr)));
#else
                NVPW_CHECK((tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device)));
#endif
                NVPW_CHECK(tracer.BeginSession(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, FrameLatency));
                tracer.Reset();
                NVPW_CHECK(!tracer.IsInitialized());
                NVPW_CHECK(!tracer.InSession());
            }
            // BeginSession + EndSession
            {
                MiniTracerVulkan tracer;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK((tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr)));
#else
                NVPW_CHECK((tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device)));
#endif
                NVPW_CHECK(tracer.BeginSession(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, FrameLatency));
                tracer.EndSession();
                NVPW_CHECK(tracer.IsInitialized());
                NVPW_CHECK(!tracer.InSession());
            }
            // BeginSession + EndSession + Reset
            {
                MiniTracerVulkan tracer;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK((tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr)));
#else
                NVPW_CHECK((tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device)));
#endif
                NVPW_CHECK(tracer.BeginSession(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, FrameLatency));
                tracer.EndSession();
                tracer.Reset();
                NVPW_CHECK(!tracer.IsInitialized());
                NVPW_CHECK(!tracer.InSession());
            }
            // MoveCtor-1
            {
                MiniTracerVulkan tracer1;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK(tracer1.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                NVPW_CHECK(tracer1.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
                NVPW_CHECK(tracer1.IsInitialized());
                MiniTracerVulkan tracer2(std::move(tracer1));
                NVPW_CHECK(tracer2.IsInitialized());
                NVPW_CHECK(!tracer2.InSession());
                NVPW_CHECK(!tracer1.IsInitialized());
                NVPW_CHECK(!tracer1.InSession());
            }
            // MoveCtor-2
            {
                MiniTracerVulkan tracer1;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK(tracer1.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                NVPW_CHECK(tracer1.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
                NVPW_CHECK(tracer1.BeginSession(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, FrameLatency));
                MiniTracerVulkan tracer2(std::move(tracer1));
                NVPW_CHECK(tracer2.IsInitialized());
                NVPW_CHECK(tracer2.InSession());
                NVPW_CHECK(!tracer1.IsInitialized());
                NVPW_CHECK(!tracer1.InSession());
            }
            // MoveAssignment-1
            {
                MiniTracerVulkan tracer1;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK(tracer1.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                NVPW_CHECK(tracer1.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
                MiniTracerVulkan tracer2;
                tracer2 = std::move(tracer1);
                NVPW_CHECK(tracer2.IsInitialized());
                NVPW_CHECK(!tracer2.InSession());
                NVPW_CHECK(!tracer1.IsInitialized());
                NVPW_CHECK(!tracer1.InSession());
            }
            // MoveAssignment-2
            {
                MiniTracerVulkan tracer1;
#if defined(VK_NO_PROTOTYPES)
                NVPW_CHECK(tracer1.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
                NVPW_CHECK(tracer1.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
                NVPW_CHECK(tracer1.BeginSession(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, FrameLatency));
                MiniTracerVulkan tracer2;
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

            VkResult vkResult = VK_SUCCESS;
            MiniTracerVulkan tracer;
#if defined(VK_NO_PROTOTYPES)
            NVPW_CHECK(tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device, vkGetInstanceProcAddr, vkGetDeviceProcAddr));
#else
            NVPW_CHECK(tracer.Initialize(instance, physicalDevice.physicalDevice, logicalDevice.device));
#endif
            NVPW_CHECK(tracer.BeginSession(logicalDevice.gfxQueue, logicalDevice.gfxQueueFamilyIndex, FrameLatency));
            auto endSessionGuard = ScopeExitGuard([&]() {
                tracer.EndSession();
            });

            uint64_t lastFrameEndTime = 0;
            for (size_t frameIdx = 0; frameIdx < 1000; ++frameIdx)
            {
                if (frameIdx % FrameLatency == FrameLatency - 1)
                {
                    vkResult = vkQueueWaitIdle(logicalDevice.gfxQueue);
                    NVPW_REQUIRE(vkResult == VK_SUCCESS);
                }
                // here should be user's workload, but for testing purpose only, we don't have any actual workload here
                NVPW_CHECK(tracer.OnFrameEnd());
                while (true)
                {
                    MiniTracerVulkan::FrameData frameData{};
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
            vkResult = vkQueueWaitIdle(logicalDevice.gfxQueue);
            NVPW_REQUIRE(vkResult == VK_SUCCESS);
        }
    }

    NVPW_TEST_SUITE_END();
}}} // namespace nv::perf::test
