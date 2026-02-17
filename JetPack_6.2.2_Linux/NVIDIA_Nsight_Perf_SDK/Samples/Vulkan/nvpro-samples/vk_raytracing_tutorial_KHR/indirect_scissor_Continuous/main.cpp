/* Copyright (c) 2014-2023, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// ImGui - standalone example application for Glfw + Vulkan, using programmable
// pipeline If you are new to ImGui, see examples/README.txt and documentation
// at the top of imgui.cpp.

#include <array>
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"

#include "hello_vulkan.h"
#include "imgui/extras/imgui_camera_widget.h"
#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvpsystem.hpp"
#include "nvvk/appbase_vkpp.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/context_vk.hpp"

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
#include <nvperf_host_impl.h>
#include <NvPerfCounterConfiguration.h>
#include <NvPerfCounterData.h>
#include <NvPerfCpuMarkerTrace.h>
#include <NvPerfMetricsEvaluator.h>
#include <NvPerfPeriodicSamplerGpu.h>
#include <NvPerfVulkan.h>
#include <iomanip>
#include <iostream>
#include <inttypes.h>
#endif // NV_PERF_ENABLE_INSTRUMENTATION

//////////////////////////////////////////////////////////////////////////
#define UNUSED(x) (void)(x)
//////////////////////////////////////////////////////////////////////////

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
using namespace nv::perf;

inline void ThrowIfFalse(bool result, const char* pMessage)
{
    if (!result)
    {
        NV_PERF_LOG_ERR(10, "%s\n", pMessage);
        throw std::runtime_error(pMessage);
    }
}

// Note: The following metrics are for demonstration purposes only. For a more comprehensive set of single-pass metrics, please refer to the 'HudConfigurations'.
const char* Metrics[] = {
    "gpc__cycles_elapsed.avg.per_second",
    "sys__cycles_elapsed.avg.per_second",
    "lts__cycles_elapsed.avg.per_second",
};
const uint32_t SamplingFrequency = 120; // 120 Hz
const uint32_t MaxDecodeLatencyInNanoSeconds = 1000 * 1000 * 1000; // tolerate maximum DecodeCounters() latency up to 1 second
std::ostream& OutStream = std::cout;
#endif // NV_PERF_ENABLE_INSTRUMENTATION

// Default search path for shaders
std::vector<std::string> defaultSearchPaths;

// GLFW Callback functions
static void onErrorCallback(int error, const char* description)
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Extra UI
void renderUI(HelloVulkan& helloVk)
{
  ImGuiH::CameraWidget();
  if(ImGui::CollapsingHeader("Light"))
  {
    ImGui::RadioButton("Point", &helloVk.m_pushConstant.lightType, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Infinite", &helloVk.m_pushConstant.lightType, 1);

    ImGui::SliderFloat3("Position", &helloVk.m_pushConstant.lightPosition.x, -20.f, 20.f);
    ImGui::SliderFloat("Intensity", &helloVk.m_pushConstant.lightIntensity, 0.f, 150.f);
    ImGui::Checkbox("Lantern Debug", &helloVk.m_lanternDebug);
  }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
static int const SAMPLE_WIDTH  = 1440;
static int const SAMPLE_HEIGHT = 900;

//--------------------------------------------------------------------------------------------------
// Application Entry
//
int main(int argc, char** argv)
{
  UNUSED(argc);

  // Setup GLFW window
  glfwSetErrorCallback(onErrorCallback);
  if(!glfwInit())
  {
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window =
      glfwCreateWindow(SAMPLE_WIDTH, SAMPLE_HEIGHT, PROJECT_NAME, nullptr, nullptr);

  // Setup camera
  CameraManip.setWindowSize(SAMPLE_WIDTH, SAMPLE_HEIGHT);
  CameraManip.setLookat(nvmath::vec3f(5, 4, -4), nvmath::vec3f(0, 1, 0), nvmath::vec3f(0, 1, 0));

  // Setup Vulkan
  if(!glfwVulkanSupported())
  {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }

  // setup some basic things for the sample, logging file for example
  NVPSystem system(PROJECT_NAME);

  // Search path for shaders and other media
  defaultSearchPaths = {
      NVPSystem::exePath() + PROJECT_RELDIRECTORY,
      NVPSystem::exePath() + PROJECT_RELDIRECTORY "..",
      std::string(PROJECT_NAME),
  };

  // Requesting Vulkan extensions and layers
  nvvk::ContextCreateInfo contextInfo(true);
  contextInfo.setVersion(1, 2);
  contextInfo.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);
  contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);
  contextInfo.addInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef WIN32
  contextInfo.addInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
  contextInfo.addInstanceExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
  contextInfo.addInstanceExtension(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
  contextInfo.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
  // #VKRay: Activate the ray tracing extension
  vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelFeature;
  contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false,
                                 &accelFeature);
  vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature;
  contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false,
                                 &rtPipelineFeature);
  contextInfo.addDeviceExtension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  contextInfo.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);


  // Creating Vulkan base application
  nvvk::Context vkctx{};
  vkctx.initInstance(contextInfo);
  // Find all compatible devices
  auto compatibleDevices = vkctx.getCompatibleDevices(contextInfo);
  assert(!compatibleDevices.empty());
  // Use a compatible device
  vkctx.initDevice(compatibleDevices[0], contextInfo);


  // Create example
  HelloVulkan helloVk;

  // Window need to be opened to get the surface on which to draw
  const vk::SurfaceKHR surface = helloVk.getVkSurface(vkctx.m_instance, window);
  vkctx.setGCTQueueWithPresent(surface);

  helloVk.setup(vkctx.m_instance, vkctx.m_device, vkctx.m_physicalDevice,
                vkctx.m_queueGCT.familyIndex);
  helloVk.createSwapchain(surface, SAMPLE_WIDTH, SAMPLE_HEIGHT);
  helloVk.createDepthBuffer();
  helloVk.createRenderPass();
  helloVk.createFrameBuffers();

  // Setup Imgui
  helloVk.initGUI(0);  // Using sub-pass 0

  // Creation of the example
  helloVk.loadModel(nvh::findFile("media/scenes/Medieval_building.obj", defaultSearchPaths, true));
  helloVk.loadModel(nvh::findFile("media/scenes/plane.obj", defaultSearchPaths, true));
  helloVk.addLantern({8.000f, 1.100f, 3.600f}, {1.0f, 0.0f, 0.0f}, 0.4f, 4.0f);
  helloVk.addLantern({8.000f, 0.600f, 3.900f}, {0.0f, 1.0f, 0.0f}, 0.4f, 4.0f);
  helloVk.addLantern({8.000f, 1.100f, 4.400f}, {0.0f, 0.0f, 1.0f}, 0.4f, 4.0f);
  helloVk.addLantern({1.730f, 1.812f, -1.604f}, {0.0f, 0.4f, 0.4f}, 0.4f, 4.0f);
  helloVk.addLantern({1.730f, 1.862f, 1.916f}, {0.0f, 0.2f, 0.4f}, 0.3f, 3.0f);
  helloVk.addLantern({-2.000f, 1.900f, -0.700f}, {0.8f, 0.8f, 0.6f}, 0.4f, 3.9f);
  helloVk.addLantern({0.100f, 0.080f, -2.392f}, {1.0f, 0.0f, 1.0f}, 0.5f, 5.0f);
  helloVk.addLantern({1.948f, 0.080f, 0.598f}, {1.0f, 1.0f, 1.0f}, 0.6f, 6.0f);
  helloVk.addLantern({-2.300f, 0.080f, 2.100f}, {0.0f, 0.7f, 0.0f}, 0.6f, 6.0f);
  helloVk.addLantern({-1.400f, 4.300f, 0.150f}, {1.0f, 1.0f, 0.0f}, 0.7f, 7.0f);

  helloVk.createOffscreenRender();
  helloVk.createDescriptorSetLayout();
  helloVk.createGraphicsPipeline();
  helloVk.createUniformBuffer();
  helloVk.createSceneDescriptionBuffer();
  helloVk.updateDescriptorSet();

  // #VKRay
  helloVk.initRayTracing();
  helloVk.createBottomLevelAS();
  helloVk.createTopLevelAS();
  helloVk.createLanternIndirectBuffer();
  helloVk.createRtDescriptorSet();
  helloVk.createRtPipeline();
  helloVk.createLanternIndirectDescriptorSet();
  helloVk.createLanternIndirectCompPipeline();
  helloVk.createRtShaderBindingTable();

  helloVk.createPostDescriptor();
  helloVk.createPostPipeline();
  helloVk.updatePostDescriptorSet();


  nvmath::vec4f clearColor   = nvmath::vec4f(1, 1, 1, 1.00f);
  bool          useRaytracer = true;


  helloVk.setupGlfwCallbacks(window);
  ImGui_ImplGlfw_InitForVulkan(window, true);

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
    ThrowIfFalse(InitializeNvPerf(), "Failed InitializeNvPerf().");
    ThrowIfFalse(VulkanLoadDriver(vkctx.m_instance), "Failed VulkanLoadDriver().");

    ThrowIfFalse(VulkanIsNvidiaDevice(vkctx.m_physicalDevice), "GPU is not a Nvidia device.");
    const size_t deviceIndex = VulkanGetNvperfDeviceIndex(vkctx.m_instance, vkctx.m_physicalDevice, vkctx.m_device);
    ThrowIfFalse(deviceIndex != (size_t)~0, "Failed VulkanGetNvperfDeviceIndex().");

    // Initialize the periodic sampler
    sampler::GpuPeriodicSampler sampler;
    ThrowIfFalse(sampler.Initialize(deviceIndex), "Failed to initialize the periodic sampler.");
    const DeviceIdentifiers deviceIdentifiers = sampler.GetDeviceIdentifiers();

    // Create the metrics evaluator
    MetricsEvaluator metricsEvaluator;
    {
        std::vector<uint8_t> metricsEvaluatorScratchBuffer;
        NVPW_MetricsEvaluator* pMetricsEvaluator = sampler::DeviceCreateMetricsEvaluator(metricsEvaluatorScratchBuffer, deviceIdentifiers.pChipName);
        ThrowIfFalse(pMetricsEvaluator, "Failed to create the metrics evaluator.");
        metricsEvaluator = MetricsEvaluator(pMetricsEvaluator, std::move(metricsEvaluatorScratchBuffer)); // transfer ownership to m_metricsEvaluator
    }

    // Create the config builder, this is used to create a counter configuration
    MetricsConfigBuilder configBuilder;
    {
        NVPA_RawMetricsConfig* pRawMetricsConfig = sampler::DeviceCreateRawMetricsConfig(deviceIdentifiers.pChipName);
        ThrowIfFalse(pRawMetricsConfig, "Failed to create the raw metrics config.");
        ThrowIfFalse(configBuilder.Initialize(metricsEvaluator, pRawMetricsConfig, deviceIdentifiers.pChipName), "Failed to initialize the config builder."); // transfer pRawMetricsConfig's ownership to configBuilder
    }

    // Add metrics into config builder
    std::vector<NVPW_MetricEvalRequest> metricEvalRequests; // This is used in both scheduling and subsequently evaluating the values.
    for (size_t ii = 0; ii < sizeof(Metrics) / sizeof(Metrics[0]); ++ii)
    {
        const char* const pMetric = Metrics[ii];
        NVPW_MetricEvalRequest request{};
        ThrowIfFalse(ToMetricEvalRequest(metricsEvaluator, pMetric, request), "Failed to convert the metric to its NVPW_MetricEvalRequest.");
        // By setting "keepInstances" to false, the counter data will only store GPU-level values, reducing its size and improving the performance of metric evaluation.
        // However, this option has the drawback of making max/min submetrics non-evaluable.
        const bool keepInstances = false;
        ThrowIfFalse(configBuilder.AddMetrics(&request, 1, keepInstances), "Failed to add the metric into the config build.");
        metricEvalRequests.emplace_back(std::move(request));
    }

    // Create the counter configuration out of the config builder.
    CounterConfiguration counterConfiguration;
    ThrowIfFalse(CreateConfiguration(configBuilder, counterConfiguration), "Failed CreateConfiguration().");
    // Periodic sampler supports only single-pass configurations, meaning that all scheduled metrics must be collectable in a single pass.
    ThrowIfFalse(counterConfiguration.numPasses == 1u, "The scheduled config is not a single-pass configuration, so it is not compatible with the periodic sampler.");

    // Initialize the counter data
    // Below setting determines the maximum size of a counter data image. However, because the counter data here is requested to work in the ring buffer mode,
    // when the put pointer reaches the end, it will start from the beginning and overwrite previous data even if it hasn't been read yet.
    // Therefore, the size specified here must be sufficient to cover the latency.
    const uint32_t MaxSamples = 1024;
    const bool Validate = true; // Setting this to true enables extra validation, which is useful for debugging. In production environments, it can be set to false for improved performance.
    sampler::RingBufferCounterData counterData; // This is used to store the counter values collected during profiling.
    ThrowIfFalse(counterData.Initialize(MaxSamples, Validate, [&](uint32_t maxSamples, NVPW_PeriodicSampler_CounterData_AppendMode appendMode, std::vector<uint8_t>& counterData) {
        return sampler::GpuPeriodicSamplerCreateCounterData(
            deviceIndex,
            counterConfiguration.counterDataPrefix.data(),
            counterConfiguration.counterDataPrefix.size(),
            maxSamples,
            appendMode,
            counterData);
        }), "Failed counter data initialization.");

    // Update the metrics evaluator with the actual device's attributes stored in the counter data
    ThrowIfFalse(MetricsEvaluatorSetDeviceAttributes(metricsEvaluator, counterData.GetCounterData().data(), counterData.GetCounterData().size()), "Failed MetricsEvaluatorSetDeviceAttributes().");

    // Output the header in CSV format
    {
        OutStream << "StartTime, EndTime, Duration";
        const auto countersEnumerator = EnumerateCounters(metricsEvaluator);
        const auto ratiosEnumerator = EnumerateRatios(metricsEvaluator);
        const auto throughputsEnumerator = EnumerateThroughputs(metricsEvaluator);
        for (const NVPW_MetricEvalRequest& metricEvalRequest : metricEvalRequests)
        {
            OutStream << ", " << ToString(countersEnumerator, ratiosEnumerator, throughputsEnumerator, metricEvalRequest);
        }
        OutStream << "\n";
    }

    // Start a periodic sampler session
    const uint32_t samplingIntervalInNanoSeconds = 1000 * 1000 * 1000 / SamplingFrequency;
    const sampler::GpuPeriodicSampler::GpuPulseSamplingInterval samplingInterval = sampler.GetGpuPulseSamplingInterval(samplingIntervalInNanoSeconds);
    const uint32_t maxNumUndecodedSamples = MaxDecodeLatencyInNanoSeconds / samplingIntervalInNanoSeconds;
    size_t recordBufferSize = 0;
    ThrowIfFalse(sampler::GpuPeriodicSamplerCalculateRecordBufferSize(deviceIndex, counterConfiguration.configImage, maxNumUndecodedSamples, recordBufferSize), "Failed GpuPeriodicSamplerCalculateRecordBufferSize().");

    const size_t MaxNumUndecodedSamplingRanges = 1; // must be 1
    ThrowIfFalse(sampler.BeginSession(
        recordBufferSize,
        MaxNumUndecodedSamplingRanges,
        { samplingInterval.triggerSource },
        samplingInterval.samplingInterval), "Failed to start a periodic sampler session.");

    // Apply the previously generated counter configuration to the periodic sampler.
    const size_t passIndex = 0; // This is a single-pass configuration, so the pass index is fixed at 0.
    ThrowIfFalse(sampler.SetConfig(counterConfiguration.configImage, passIndex), "Failed to apply the counter configuration to the periodic sampler.");

    // Start sampling.
    // Ideally, sampling should only start right before executing the target workloads to prevent the record buffer from being occupied by records generated by GPU triggers before the target workloads.
    // However, in this use case, it is acceptable because the trigger source is set to "NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_ENGINE_TRIGGER", which doesn't automatically generate GPU triggers but
    // relies on clients manually pushing triggers through the command list. Furthermore, since the metric configuration used is for low-speed sampling, no "overflow prevention records" will be emitted.
    ThrowIfFalse(sampler.StartSampling(), "Failed to start sampling.");
#endif // NV_PERF_ENABLE_INSTRUMENTATION

  // Main loop
  while(!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if(helloVk.isMinimized())
      continue;

    // Start the Dear ImGui frame
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Show UI window.
    if(helloVk.showGui())
    {
      ImGuiH::Panel::Begin();
      ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&clearColor));
      ImGui::Checkbox("Ray Tracer mode", &useRaytracer);  // Switch between raster and ray tracing

      renderUI(helloVk);
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

      ImGuiH::Control::Info("", "", "(F10) Toggle Pane", ImGuiH::Control::Flags::Disabled);
      ImGuiH::Panel::End();
    }

    // Start rendering the scene
    helloVk.prepareFrame();

    // Start command buffer of this frame
    auto                     curFrame = helloVk.getCurFrame();
    const vk::CommandBuffer& cmdBuf   = helloVk.getCommandBuffers()[curFrame];

    cmdBuf.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // Updating camera buffer
    helloVk.updateUniformBuffer(cmdBuf);

    // Clearing screen
    vk::ClearValue clearValues[2];
    clearValues[0].setColor(
        std::array<float, 4>({clearColor[0], clearColor[1], clearColor[2], clearColor[3]}));
    clearValues[1].setDepthStencil({1.0f, 0});

    // Offscreen render pass
    {
      vk::RenderPassBeginInfo offscreenRenderPassBeginInfo;
      offscreenRenderPassBeginInfo.setClearValueCount(2);
      offscreenRenderPassBeginInfo.setPClearValues(clearValues);
      offscreenRenderPassBeginInfo.setRenderPass(helloVk.m_offscreenRenderPass);
      offscreenRenderPassBeginInfo.setFramebuffer(helloVk.m_offscreenFramebuffer);
      offscreenRenderPassBeginInfo.setRenderArea({{}, helloVk.getSize()});

      // Rendering Scene
      if(useRaytracer)
      {
        helloVk.raytrace(cmdBuf, clearColor);
      }
      else
      {
        cmdBuf.beginRenderPass(offscreenRenderPassBeginInfo, vk::SubpassContents::eInline);
        helloVk.rasterize(cmdBuf);
        cmdBuf.endRenderPass();
      }
    }

    // 2nd rendering pass: tone mapper, UI
    {
      vk::RenderPassBeginInfo postRenderPassBeginInfo;
      postRenderPassBeginInfo.setClearValueCount(2);
      postRenderPassBeginInfo.setPClearValues(clearValues);
      postRenderPassBeginInfo.setRenderPass(helloVk.getRenderPass());
      postRenderPassBeginInfo.setFramebuffer(helloVk.getFramebuffers()[curFrame]);
      postRenderPassBeginInfo.setRenderArea({{}, helloVk.getSize()});

      cmdBuf.beginRenderPass(postRenderPassBeginInfo, vk::SubpassContents::eInline);
      // Rendering tonemapper
      helloVk.drawPost(cmdBuf);
      // Rendering UI
      ImGui::Render();
      ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
      cmdBuf.endRenderPass();
    }

    // Submit for display
    cmdBuf.end();
    helloVk.submitFrame();

#ifdef NV_PERF_ENABLE_INSTRUMENTATION
    // Decode the record buffer and store the decoded counters into the counter data.
    const size_t NumSamplingRangesToDecode = 1;
    size_t numSamplingRangesDecoded = 0;
    bool recordBufferOverflow = false;
    size_t numSamplesDropped = 0;
    size_t numSamplesMerged = 0;
    ThrowIfFalse(sampler.DecodeCounters(
        counterData.GetCounterData(),
        NumSamplingRangesToDecode,
        numSamplingRangesDecoded,
        recordBufferOverflow,
        numSamplesDropped,
        numSamplesMerged), "Failed to decode counters.");
    ThrowIfFalse(!numSamplingRangesDecoded, "Unexpected numSamplingRangesDecoded."); // Since StopSampling() has not been called yet, this sampling range remains open, and it cannot be considered fully decoded.
    ThrowIfFalse(!recordBufferOverflow, "Record buffer has overflowed. Please ensure that the value of `maxNumUndecodedSamples` is sufficiently large.");
    ThrowIfFalse(!numSamplesDropped, "numSamplesDropped is not 0, this should not happen when the counter data operates in the ring buffer mode.");
    ThrowIfFalse(!numSamplesMerged, "Samples appear to be merged, this can reduce the accuracy of the collected samples. Please check for any back-to-back triggers!");
    ThrowIfFalse(counterData.UpdatePut(), "Failed to update counter data's put pointer.");

    const uint32_t numUnreadRanges = counterData.GetNumUnreadRanges();
    if (numUnreadRanges)
    {
        std::vector<double> metricValues(metricEvalRequests.size());
        uint32_t numRangesConsumed = 0;
        ThrowIfFalse(counterData.ConsumeData([&](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop) {
            sampler::SampleTimestamp timestamp{};
            if (!CounterDataGetSampleTime(pCounterDataImage, rangeIndex, timestamp))
            {
                return false;
            }

            if (!EvaluateToGpuValues(
                metricsEvaluator,
                pCounterDataImage,
                counterDataImageSize,
                rangeIndex,
                metricEvalRequests.size(),
                metricEvalRequests.data(),
                metricValues.data()))
            {
                return false;
            }
            {
                OutStream << std::fixed << std::setprecision(0) << timestamp.start << ", " << timestamp.end << ", " << (timestamp.end - timestamp.start);
                for (double metricValue : metricValues)
                {
                    OutStream << ", " << metricValue;
                }
                OutStream << "\n";
            }
            if (++numRangesConsumed == numUnreadRanges)
            {
                stop = true; // Inform counter data to stop iterating because all existing data has been consumed.
            }
            return true;
            }), "Failed to consume counter data.");
        OutStream << std::flush;
        ThrowIfFalse(counterData.UpdateGet(numRangesConsumed), "Counter data failed to update get pointer.");
    }
#endif // NV_PERF_ENABLE_INSTRUMENTATION
  }

  // Cleanup
#ifdef NV_PERF_ENABLE_INSTRUMENTATION
  ThrowIfFalse(sampler.StopSampling(), "Failed to stop sampling.");
  ThrowIfFalse(sampler.EndSession(), "Failed to end a periodic sampler session.");
  sampler.Reset();
#endif

  helloVk.getDevice().waitIdle();
  helloVk.destroyResources();
  helloVk.destroy();

  vkctx.deinit();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
