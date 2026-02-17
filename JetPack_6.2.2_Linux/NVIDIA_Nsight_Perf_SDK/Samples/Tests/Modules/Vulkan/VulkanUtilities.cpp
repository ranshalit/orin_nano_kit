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
#include "NvPerfPeriodicSamplerVulkan.h"

#if defined(VK_NO_PROTOTYPES)
#if defined(_WIN32)
#include <libloaderapi.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif
#endif

namespace nv { namespace perf { namespace test {
    VkInstance VulkanCreateInstance(const char* appName)
    {
        VkInstance instance;
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext = nullptr;
        appInfo.pApplicationName = appName;
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "NvPerf Vulkan Test Engine";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_0;

        std::vector<const char*> instanceExtensionNames;
        
        if (!VulkanAppendInstanceRequiredExtensions(instanceExtensionNames, appInfo.apiVersion))
        {
            return VK_NULL_HANDLE;
        }

        VkInstanceCreateInfo instInfo = {};
        instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instInfo.pNext = nullptr;
        instInfo.flags = 0;
        instInfo.pApplicationInfo = &appInfo;
        instInfo.enabledExtensionCount = (uint32_t)instanceExtensionNames.size();
        instInfo.ppEnabledExtensionNames = instanceExtensionNames.size() ? instanceExtensionNames.data() : nullptr;
        instInfo.enabledLayerCount = 0;
        instInfo.ppEnabledLayerNames =  nullptr;

        VkResult res = vkCreateInstance(&instInfo, nullptr, &instance);
        if (res != VK_SUCCESS)
        {
            return VK_NULL_HANDLE;
        }

#if defined(VK_NO_PROTOTYPES)
        if (!LoadInstanceLevelVulkanFunctions(instance))
        {
            return VK_NULL_HANDLE;
        }
        if (!LoadDeviceLevelVulkanFunctions(instance))
        {
            return VK_NULL_HANDLE;
        }
#endif

        return instance;
    }

    PhysicalDevice VulkanFindNvidiaPhysicalDevice(VkInstance instance)
    {
        PhysicalDevice physicalDevice;

        uint32_t numPhysicalDevices = 0;
        VkResult res = vkEnumeratePhysicalDevices(instance, &numPhysicalDevices, nullptr);
        if (res != VK_SUCCESS)
        {
            return physicalDevice;
        }

        std::vector<VkPhysicalDevice> physicalDevices(numPhysicalDevices);
        res = vkEnumeratePhysicalDevices(instance, &numPhysicalDevices, physicalDevices.data());
        if (res != VK_SUCCESS)
        {
            return physicalDevice;
        }

        uint32_t deviceIdx=0;
        for (;deviceIdx < numPhysicalDevices; ++deviceIdx)
        {
            if (VulkanIsNvidiaDevice(physicalDevices[deviceIdx]
#if defined(VK_NO_PROTOTYPES)
                                    , instance
                                    , vkGetInstanceProcAddr
#endif
                                    ))
            {
                break;
            }
        }

        if (deviceIdx == numPhysicalDevices)
        {
            return physicalDevice;
        }

        physicalDevice.physicalDevice = physicalDevices[deviceIdx];

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[deviceIdx], &queueFamilyCount, nullptr);
        physicalDevice.queueFamilyProperties.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[deviceIdx], &queueFamilyCount, physicalDevice.queueFamilyProperties.data());

        vkGetPhysicalDeviceMemoryProperties(physicalDevices[deviceIdx], &physicalDevice.memoryProperties);

        return physicalDevice;
    }

    uint32_t GetMemoryTypeIndex(PhysicalDevice physicalDevice, uint32_t typeBits, VkMemoryPropertyFlags properties)
    {
        for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < physicalDevice.memoryProperties.memoryTypeCount; memoryTypeIndex++)
        {
            uint32_t currentTypeBit = 1 << memoryTypeIndex;
            if (typeBits & currentTypeBit)
            {
                if ((physicalDevice.memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & properties) == properties)
                {
                    return memoryTypeIndex;
                }
            }
        }
        return (uint32_t)(-1);
    }

    LogicalDevice VulkanCreateLogicalDeviceWithGraphicsAndAsyncQueues(VkInstance instance, PhysicalDevice& physicalDevice)
    {
        LogicalDevice logicalDevice;

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;
        bool foundGraphics = false;
        bool foundAsyncCompute = false;
        for (uint32_t propIdx = 0; propIdx < physicalDevice.queueFamilyProperties.size(); ++propIdx)
        {
            const VkQueueFamilyProperties& prop = physicalDevice.queueFamilyProperties[propIdx];
            if (prop.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                queueCreateInfos.push_back({VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                            nullptr,
                                            0x0,
                                            propIdx,
                                            1,
                                            &queuePriority});
                foundGraphics = true;
            }
            else if (prop.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                queueCreateInfos.push_back({VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                            nullptr,
                                            0x0,
                                            propIdx,
                                            1,
                                            &queuePriority});
                foundAsyncCompute = true;
            }
        }

        if (!foundGraphics || !foundAsyncCompute)
        {
            return logicalDevice;
        }

        std::vector<const char*> deviceExtensionNames;
        if (!VulkanAppendDeviceRequiredExtensions(instance, physicalDevice.physicalDevice, (void*)vkGetInstanceProcAddr, deviceExtensionNames))
        {
            return logicalDevice;
        }

        const uint32_t apiVersion = VulkanGetPhysicalDeviceApiVersion(physicalDevice.physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                                                     , instance
                                                                     , vkGetInstanceProcAddr
#endif
                                                                     );
        sampler::PeriodicSamplerTimeHistoryVulkan::AppendDeviceRequiredExtensions(apiVersion, deviceExtensionNames);

        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = nullptr;
        deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensionNames.size();
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensionNames.size() ? deviceExtensionNames.data() : nullptr;
        deviceCreateInfo.enabledLayerCount = 0;
        deviceCreateInfo.ppEnabledLayerNames = nullptr;
        deviceCreateInfo.pEnabledFeatures = nullptr;

        VkResult res = vkCreateDevice(physicalDevice.physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice.device);
        if (res != VK_SUCCESS)
        {
            return logicalDevice;
        }

        for (uint32_t idx = 0; idx < queueCreateInfos.size(); ++idx)
        {
            uint32_t familyIdx = queueCreateInfos[idx].queueFamilyIndex;
            const VkQueueFamilyProperties& prop = physicalDevice.queueFamilyProperties[familyIdx];
            if (prop.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                logicalDevice.gfxQueueFamilyIndex = familyIdx;
                vkGetDeviceQueue(logicalDevice.device, familyIdx, 0, &logicalDevice.gfxQueue);
            }
            else if (prop.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                logicalDevice.asyncComputeQueueFamilyIndex = familyIdx;
                vkGetDeviceQueue(logicalDevice.device, familyIdx, 0, &logicalDevice.asyncComputeQueue);
            }
        }
        
        return logicalDevice;
    }

    CommandBuffer VulkanCreateCommandBuffer(VkDevice device, uint32_t queueFamilyIndex)
    {
        CommandBuffer commandBuffer;

        VkCommandPoolCreateInfo cmdPoolCreateInfo{};
        cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolCreateInfo.pNext = nullptr;
        cmdPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
        cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkResult res = vkCreateCommandPool(device, &cmdPoolCreateInfo, nullptr, &commandBuffer.commandPool);
        if (res != VK_SUCCESS)
        {
            return commandBuffer;
        }

        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.pNext = nullptr;
        allocateInfo.commandPool = commandBuffer.commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer.commandBuffer);
        return commandBuffer;
    }

    Buffer VulkanCreateBuffer(PhysicalDevice& physicalDevice, VkDevice device, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size)
    {
        Buffer buffer;

        VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferCreateInfo.size = size;
        bufferCreateInfo.usage = usageFlags;
        VkResult result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer.buffer);
        if (result != VK_SUCCESS)
        {
            return Buffer();
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, buffer.buffer, &memReqs);

        VkMemoryAllocateInfo memAlloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = GetMemoryTypeIndex(physicalDevice, memReqs.memoryTypeBits, memoryPropertyFlags);
        if (memAlloc.memoryTypeIndex == (uint32_t)(-1))
        {
            vkDestroyBuffer(device, buffer.buffer, nullptr);
            return Buffer();
        }

        result = vkAllocateMemory(device, &memAlloc, nullptr, &buffer.deviceMemory);
        if (result != VK_SUCCESS)
        {
            vkDestroyBuffer(device, buffer.buffer, nullptr);
            return Buffer();
        }

        result = vkBindBufferMemory(device, buffer.buffer, buffer.deviceMemory, 0);
        if (result != VK_SUCCESS)
        {
            vkFreeMemory(device, buffer.deviceMemory, nullptr);
            vkDestroyBuffer(device, buffer.buffer, nullptr);
            return Buffer();
        }

        return buffer;
    }

#ifdef VK_NO_PROTOTYPES
#define VK_FUNC_DEF(name) PFN_##name name

#if defined(_WIN32)
        static HMODULE g_vulkanLibrary;
#elif defined(__linux__)
        static void* g_vulkanLibrary;
#endif

    bool LoadVulkanLibrary()
    {
#if defined(_WIN32)
        g_vulkanLibrary = LoadLibrary("vulkan-1.dll");
        return !!g_vulkanLibrary;
#elif defined(__linux__)
        g_vulkanLibrary = dlopen("libvulkan.so.1", RTLD_NOW);
        return !!g_vulkanLibrary;
#else
        return false;
#endif
    }

    bool FreeVulkanLibrary()
    {
#if defined(_WIN32)
        return !!FreeLibrary(g_vulkanLibrary);
#elif defined(__linux__)
        return !dlclose(g_vulkanLibrary);
#else
        return false;
#endif
    }

    VK_FUNC_DEF(vkGetInstanceProcAddr);

    bool LoadExportedVulkanFunctions()
    {
#if defined(_WIN32)
        vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(g_vulkanLibrary, "vkGetInstanceProcAddr");
        if (!vkGetInstanceProcAddr)
        {
            return false;
        }
        return true;
#elif defined(__linux__)
        vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(g_vulkanLibrary, "vkGetInstanceProcAddr");
        if (!vkGetInstanceProcAddr)
        {
            return false;
        }
        return true;
#else
        return false;
#endif
    }

    VK_FUNC_DEF(vkCreateInstance);

    bool LoadGlobalLevelVulkanFunctions()
    {
#define LOAD_GLOBAL_LEVEL_VULKAN_FUNCTION(name)                                 \
        if (!(name = (PFN_##name)vkGetInstanceProcAddr(VK_NULL_HANDLE, #name))) \
        {                                                                       \
            return false;                                                       \
        }

        LOAD_GLOBAL_LEVEL_VULKAN_FUNCTION(vkCreateInstance);

        return true;
    }

    VK_FUNC_DEF(vkCreateDevice);
    VK_FUNC_DEF(vkDestroyInstance);
    VK_FUNC_DEF(vkEnumeratePhysicalDevices);
    VK_FUNC_DEF(vkGetDeviceProcAddr);
    VK_FUNC_DEF(vkGetPhysicalDeviceMemoryProperties);
    VK_FUNC_DEF(vkGetPhysicalDeviceQueueFamilyProperties);

    bool LoadInstanceLevelVulkanFunctions(VkInstance instance)
    {
#define LOAD_INSTANCE_LEVEL_VULKAN_FUNCTION(instance, name)                     \
        if (!(name = (PFN_##name)vkGetInstanceProcAddr(instance, #name)))       \
        {                                                                       \
            return false;                                                       \
        }

        LOAD_INSTANCE_LEVEL_VULKAN_FUNCTION(instance, vkCreateDevice);
        LOAD_INSTANCE_LEVEL_VULKAN_FUNCTION(instance, vkDestroyInstance);
        LOAD_INSTANCE_LEVEL_VULKAN_FUNCTION(instance, vkEnumeratePhysicalDevices);
        LOAD_INSTANCE_LEVEL_VULKAN_FUNCTION(instance, vkGetDeviceProcAddr);
        LOAD_INSTANCE_LEVEL_VULKAN_FUNCTION(instance, vkGetPhysicalDeviceMemoryProperties);
        LOAD_INSTANCE_LEVEL_VULKAN_FUNCTION(instance, vkGetPhysicalDeviceQueueFamilyProperties);

        return true;
    }

    VK_FUNC_DEF(vkAllocateCommandBuffers);
    VK_FUNC_DEF(vkAllocateMemory);
    VK_FUNC_DEF(vkBeginCommandBuffer);
    VK_FUNC_DEF(vkBindBufferMemory);
    VK_FUNC_DEF(vkCreateBuffer);
    VK_FUNC_DEF(vkCreateCommandPool);
    VK_FUNC_DEF(vkDestroyBuffer);
    VK_FUNC_DEF(vkDestroyCommandPool);
    VK_FUNC_DEF(vkDestroyDevice);
    VK_FUNC_DEF(vkEndCommandBuffer);
    VK_FUNC_DEF(vkFreeMemory);
    VK_FUNC_DEF(vkGetBufferMemoryRequirements);
    VK_FUNC_DEF(vkGetDeviceQueue);
    VK_FUNC_DEF(vkMapMemory);
    VK_FUNC_DEF(vkQueueSubmit);
    VK_FUNC_DEF(vkQueueWaitIdle);
    VK_FUNC_DEF(vkUnmapMemory);

    bool LoadDeviceLevelVulkanFunctions(VkInstance instance)
    {
        // We don't use vkGetDeviceProcAddr because it would require maintaining
        // the device level dispatch table for each logical device, which is
        // overcomplicated for the test, and additional function jump overhead
        // incurred by using functions returned by vkGetInstanceProcAddr is acceptable.
#define LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, name)                       \
        if (!(name = (PFN_##name)vkGetInstanceProcAddr(instance, #name)))       \
        {                                                                       \
            return false;                                                       \
        }

        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkAllocateCommandBuffers);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkAllocateMemory);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkBeginCommandBuffer);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkBindBufferMemory);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkCreateBuffer);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkCreateCommandPool);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkDestroyBuffer);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkDestroyCommandPool);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkDestroyDevice);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkEndCommandBuffer);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkFreeMemory);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkGetBufferMemoryRequirements);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkGetDeviceQueue);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkMapMemory);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkQueueSubmit);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkQueueWaitIdle);
        LOAD_DEVICE_LEVEL_VULKAN_FUNCTION(instance, vkUnmapMemory);

        return true;
    }
#endif

}}} // nv::perf::test
