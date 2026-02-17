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

#pragma once

#include <doctest_proxy.h>
#include <doctest_reporters.h>
#include <TestCommon.h>

#include <NvPerfInit.h>
#include <NvPerfVulkan.h>
#include <vulkan/vulkan.h>

namespace nv { namespace perf { namespace test {
    struct PhysicalDevice
    {
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        std::vector<VkQueueFamilyProperties> queueFamilyProperties;
        VkPhysicalDeviceMemoryProperties memoryProperties;
    };

    struct LogicalDevice
    {
        VkDevice device = VK_NULL_HANDLE;

        VkQueue gfxQueue = VK_NULL_HANDLE;
        uint32_t gfxQueueFamilyIndex = (uint32_t)~0;

        VkQueue asyncComputeQueue = VK_NULL_HANDLE;
        uint32_t asyncComputeQueueFamilyIndex = (uint32_t)~0;
    };

    struct CommandBuffer
    {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    };

    struct Buffer
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    };

    VkInstance VulkanCreateInstance(const char* appName);
    PhysicalDevice VulkanFindNvidiaPhysicalDevice(VkInstance instance);
    uint32_t GetMemoryTypeIndex(PhysicalDevice physicalDevice, uint32_t typeBits, VkMemoryPropertyFlags properties);
    LogicalDevice VulkanCreateLogicalDeviceWithGraphicsAndAsyncQueues(VkInstance instance, PhysicalDevice& physicalDevice);
    CommandBuffer VulkanCreateCommandBuffer(VkDevice device, uint32_t queueFamilyIndex);
    Buffer VulkanCreateBuffer(PhysicalDevice& physicalDevice, VkDevice device, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size);

#ifdef VK_NO_PROTOTYPES
#define VK_FUNC_DECL(name) extern PFN_##name name

    bool LoadVulkanLibrary();
    bool FreeVulkanLibrary();

    VK_FUNC_DECL(vkGetInstanceProcAddr);

    bool LoadExportedVulkanFunctions();

    VK_FUNC_DECL(vkCreateInstance);

    bool LoadGlobalLevelVulkanFunctions();

    VK_FUNC_DECL(vkCreateDevice);
    VK_FUNC_DECL(vkDestroyInstance);
    VK_FUNC_DECL(vkEnumeratePhysicalDevices);
    VK_FUNC_DECL(vkGetDeviceProcAddr);
    VK_FUNC_DECL(vkGetPhysicalDeviceMemoryProperties);
    VK_FUNC_DECL(vkGetPhysicalDeviceQueueFamilyProperties);

    bool LoadInstanceLevelVulkanFunctions(VkInstance instance);

    VK_FUNC_DECL(vkAllocateCommandBuffers);
    VK_FUNC_DECL(vkAllocateMemory);
    VK_FUNC_DECL(vkBeginCommandBuffer);
    VK_FUNC_DECL(vkBindBufferMemory);
    VK_FUNC_DECL(vkCreateBuffer);
    VK_FUNC_DECL(vkCreateCommandPool);
    VK_FUNC_DECL(vkDestroyBuffer);
    VK_FUNC_DECL(vkDestroyCommandPool);
    VK_FUNC_DECL(vkDestroyDevice);
    VK_FUNC_DECL(vkEndCommandBuffer);
    VK_FUNC_DECL(vkFreeMemory);
    VK_FUNC_DECL(vkGetBufferMemoryRequirements);
    VK_FUNC_DECL(vkGetDeviceQueue);
    VK_FUNC_DECL(vkMapMemory);
    VK_FUNC_DECL(vkQueueSubmit);
    VK_FUNC_DECL(vkQueueWaitIdle);
    VK_FUNC_DECL(vkUnmapMemory);

    bool LoadDeviceLevelVulkanFunctions(VkInstance instance);
#endif

}}} // nv::perf::test