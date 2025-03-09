#include "dxvk_device.h"
#include "dxvk_instance.h"
#include "dxvk_latency_builtin.h"
#include "dxvk_latency_reflex.h"

namespace dxvk {
  
  DxvkDevice::DxvkDevice(
    const Rc<DxvkInstance>&         instance,
    const Rc<DxvkAdapter>&          adapter,
    const Rc<vk::DeviceFn>&         vkd,
    const DxvkDeviceFeatures&       features,
    const DxvkDeviceQueueSet&       queues,
    const DxvkQueueCallback&        queueCallback)
  : m_options           (instance->options()),
    m_instance          (instance),
    m_adapter           (adapter),
    m_vkd               (vkd),
    m_debugFlags        (instance->debugFlags()),
    m_queues            (queues),
    m_features          (features),
    m_properties        (adapter->devicePropertiesExt()),
    m_perfHints         (getPerfHints()),
    m_objects           (this),
    m_submissionQueue   (this, queueCallback) {

  }
  
  
  DxvkDevice::~DxvkDevice() {
    // If we are being destroyed during/after DLL process detachment
    // from TerminateProcess, etc, our CS threads are already destroyed
    // and we cannot synchronize against them.
    // The best we can do is just wait for the Vulkan device to be idle.
    if (this_thread::isInModuleDetachment())
      return;

    // Wait for all pending Vulkan commands to be
    // executed before we destroy any resources.
    this->waitForIdle();

    // Stop workers explicitly in order to prevent
    // access to structures that are being destroyed.
    m_objects.pipelineManager().stopWorkerThreads();
  }


  VkSubresourceLayout DxvkDevice::queryImageSubresourceLayout(
    const DxvkImageCreateInfo&        createInfo,
    const VkImageSubresource&         subresource) {
    VkImageFormatListCreateInfo formatList = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO };

    VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    info.flags = createInfo.flags;
    info.imageType = createInfo.type;
    info.format = createInfo.format;
    info.extent = createInfo.extent;
    info.mipLevels = createInfo.mipLevels;
    info.arrayLayers = createInfo.numLayers;
    info.samples = createInfo.sampleCount;
    info.tiling = VK_IMAGE_TILING_LINEAR;
    info.usage = createInfo.usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    if (createInfo.viewFormatCount && (createInfo.viewFormatCount > 1u || createInfo.viewFormats[0] != createInfo.format)) {
      formatList.viewFormatCount = createInfo.viewFormatCount;
      formatList.pViewFormats = createInfo.viewFormats;

      info.pNext = &formatList;
    }

    if (m_features.khrMaintenance5.maintenance5) {
      VkImageSubresource2KHR subresourceInfo = { VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2_KHR };
      subresourceInfo.imageSubresource = subresource;

      VkDeviceImageSubresourceInfoKHR query = { VK_STRUCTURE_TYPE_DEVICE_IMAGE_SUBRESOURCE_INFO_KHR };
      query.pCreateInfo = &info;
      query.pSubresource = &subresourceInfo;

      VkSubresourceLayout2KHR layout = { VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2_KHR };
      m_vkd->vkGetDeviceImageSubresourceLayoutKHR(m_vkd->device(), &query, &layout);
      return layout.subresourceLayout;
    } else {
      // Technically, there is no guarantee that all images with the same
      // properties are going to have consistent subresource layouts if
      // maintenance5 is not supported, but the only such use case we care
      // about is RenderDoc.
      VkImage image = VK_NULL_HANDLE;
      VkResult vr = m_vkd->vkCreateImage(m_vkd->device(), &info, nullptr, &image);

      if (vr != VK_SUCCESS)
        throw DxvkError(str::format("Failed to create temporary image: ", vr));

      VkSubresourceLayout layout = { };
      m_vkd->vkGetImageSubresourceLayout(m_vkd->device(), image, &subresource, &layout);
      m_vkd->vkDestroyImage(m_vkd->device(), image, nullptr);
      return layout;
    }
  }


  bool DxvkDevice::isUnifiedMemoryArchitecture() const {
    return m_adapter->isUnifiedMemoryArchitecture();
  }


  bool DxvkDevice::canUseGraphicsPipelineLibrary() const {
    // Without graphicsPipelineLibraryIndependentInterpolationDecoration, we
    // cannot use this effectively in many games since no client API provides
    // interpoation qualifiers in vertex shaders.
    return m_features.extGraphicsPipelineLibrary.graphicsPipelineLibrary
        && m_properties.extGraphicsPipelineLibrary.graphicsPipelineLibraryIndependentInterpolationDecoration
        && m_options.enableGraphicsPipelineLibrary != Tristate::False;
  }


  bool DxvkDevice::canUsePipelineCacheControl() const {
    // Don't bother with this unless the device also supports shader module
    // identifiers, since decoding and hashing the shaders is slow otherwise
    // and likely provides no benefit over linking pipeline libraries.
    return m_features.vk13.pipelineCreationCacheControl
        && m_features.extShaderModuleIdentifier.shaderModuleIdentifier
        && m_options.enableGraphicsPipelineLibrary != Tristate::True;
  }


  bool DxvkDevice::mustTrackPipelineLifetime() const {
    switch (m_options.trackPipelineLifetime) {
      case Tristate::True:
        return canUseGraphicsPipelineLibrary();

      case Tristate::False:
        return false;

      default:
      case Tristate::Auto:
        if (!env::is32BitHostPlatform() || !canUseGraphicsPipelineLibrary())
          return false;

        // Disable lifetime tracking for drivers that do not have any
        // significant issues with 32-bit address space to begin with
        if (m_adapter->matchesDriver(VK_DRIVER_ID_MESA_RADV_KHR))
          return false;

        return true;
    }
  }


  DxvkFramebufferSize DxvkDevice::getDefaultFramebufferSize() const {
    return DxvkFramebufferSize {
      m_properties.core.properties.limits.maxFramebufferWidth,
      m_properties.core.properties.limits.maxFramebufferHeight,
      m_properties.core.properties.limits.maxFramebufferLayers };
  }


  VkPipelineStageFlags DxvkDevice::getShaderPipelineStages() const {
    VkPipelineStageFlags result = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                                | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    
    if (m_features.core.features.geometryShader)
      result |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    
    if (m_features.core.features.tessellationShader) {
      result |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
             |  VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    }

    return result;
  }
  
  
  Rc<DxvkCommandList> DxvkDevice::createCommandList() {
    Rc<DxvkCommandList> cmdList = m_recycledCommandLists.retrieveObject();
    
    if (cmdList == nullptr)
      cmdList = new DxvkCommandList(this);
    
    return cmdList;
  }


  Rc<DxvkContext> DxvkDevice::createContext() {
    return new DxvkContext(this);
  }


  Rc<DxvkEvent> DxvkDevice::createGpuEvent() {
    return new DxvkEvent(this);
  }


  Rc<DxvkQuery> DxvkDevice::createGpuQuery(
          VkQueryType           type,
          VkQueryControlFlags   flags,
          uint32_t              index) {
    return new DxvkQuery(this, type, flags, index);
  }


  Rc<DxvkGpuQuery> DxvkDevice::createRawQuery(
          VkQueryType           type) {
    return m_objects.queryPool().allocQuery(type);
  }


  Rc<DxvkFence> DxvkDevice::createFence(
    const DxvkFenceCreateInfo& fenceInfo) {
    return new DxvkFence(this, fenceInfo);
  }


  Rc<DxvkBuffer> DxvkDevice::createBuffer(
    const DxvkBufferCreateInfo& createInfo,
          VkMemoryPropertyFlags memoryType) {
    return new DxvkBuffer(this, createInfo, m_objects.memoryManager(), memoryType);
  }
  
  
  Rc<DxvkImage> DxvkDevice::createImage(
    const DxvkImageCreateInfo&  createInfo,
          VkMemoryPropertyFlags memoryType) {
    return new DxvkImage(this, createInfo, m_objects.memoryManager(), memoryType);
  }
  
  
  Rc<DxvkSampler> DxvkDevice::createSampler(
    const DxvkSamplerKey&         createInfo) {
    return m_objects.samplerPool().createSampler(createInfo);
  }


  DxvkLocalAllocationCache DxvkDevice::createAllocationCache(
          VkBufferUsageFlags    bufferUsage,
          VkMemoryPropertyFlags propertyFlags) {
    return m_objects.memoryManager().createAllocationCache(bufferUsage, propertyFlags);
  }


  Rc<DxvkSparsePageAllocator> DxvkDevice::createSparsePageAllocator() {
    return new DxvkSparsePageAllocator(m_objects.memoryManager());
  }


  DxvkStatCounters DxvkDevice::getStatCounters() {
    DxvkPipelineCount pipe = m_objects.pipelineManager().getPipelineCount();
    DxvkPipelineWorkerStats workers = m_objects.pipelineManager().getWorkerStats();
    
    DxvkStatCounters result;
    result.setCtr(DxvkStatCounter::PipeCountGraphics, pipe.numGraphicsPipelines);
    result.setCtr(DxvkStatCounter::PipeCountLibrary,  pipe.numGraphicsLibraries);
    result.setCtr(DxvkStatCounter::PipeCountCompute,  pipe.numComputePipelines);
    result.setCtr(DxvkStatCounter::PipeTasksDone,     workers.tasksCompleted);
    result.setCtr(DxvkStatCounter::PipeTasksTotal,    workers.tasksTotal);
    result.setCtr(DxvkStatCounter::GpuIdleTicks,      m_submissionQueue.gpuIdleTicks());

    std::lock_guard<sync::Spinlock> lock(m_statLock);
    result.merge(m_statCounters);
    return result;
  }
  
  
  Rc<DxvkBuffer> DxvkDevice::importBuffer(
    const DxvkBufferCreateInfo& createInfo,
    const DxvkBufferImportInfo& importInfo,
          VkMemoryPropertyFlags memoryType) {
    return new DxvkBuffer(this, createInfo,
      importInfo, m_objects.memoryManager(), memoryType);
  }


  Rc<DxvkImage> DxvkDevice::importImage(
    const DxvkImageCreateInfo&  createInfo,
          VkImage               image,
          VkMemoryPropertyFlags memoryType) {
    return new DxvkImage(this, createInfo, image,
      m_objects.memoryManager(), memoryType);
  }


  DxvkMemoryStats DxvkDevice::getMemoryStats(uint32_t heap) {
    return m_objects.memoryManager().getMemoryStats(heap);
  }


  DxvkSharedAllocationCacheStats DxvkDevice::getMemoryAllocationStats(DxvkMemoryAllocationStats& stats) {
    m_objects.memoryManager().getAllocationStats(stats);
    return m_objects.memoryManager().getAllocationCacheStats();
  }


  uint32_t DxvkDevice::getCurrentFrameId() const {
    return m_statCounters.getCtr(DxvkStatCounter::QueuePresentCount);
  }
  
  
  void DxvkDevice::registerShader(const Rc<DxvkShader>& shader) {
    m_objects.pipelineManager().registerShader(shader);
  }
  
  
  void DxvkDevice::requestCompileShader(
    const Rc<DxvkShader>&           shader) {
    m_objects.pipelineManager().requestCompileShader(shader);
  }


  Rc<DxvkLatencyTracker> DxvkDevice::createLatencyTracker(
    const Rc<Presenter>&            presenter) {
    if (m_options.latencySleep == Tristate::False)
      return nullptr;

    if (m_options.latencySleep == Tristate::Auto) {
      if (m_features.nvLowLatency2)
        return new DxvkReflexLatencyTrackerNv(presenter);
      else
        return nullptr;
    }

    return new DxvkBuiltInLatencyTracker(presenter,
      m_options.latencyTolerance, m_features.nvLowLatency2);
  }


  void DxvkDevice::presentImage(
    const Rc<Presenter>&            presenter,
    const Rc<DxvkLatencyTracker>&   tracker,
          uint64_t                  frameId,
          DxvkSubmitStatus*         status) {
    DxvkPresentInfo presentInfo = { };
    presentInfo.presenter = presenter;
    presentInfo.frameId = frameId;

    DxvkLatencyInfo latencyInfo;
    latencyInfo.tracker = tracker;
    latencyInfo.frameId = frameId;

    m_submissionQueue.present(presentInfo, latencyInfo, status);
    
    std::lock_guard<sync::Spinlock> statLock(m_statLock);
    m_statCounters.addCtr(DxvkStatCounter::QueuePresentCount, 1);
  }


  void DxvkDevice::submitCommandList(
    const Rc<DxvkCommandList>&      commandList,
    const Rc<DxvkLatencyTracker>&   tracker,
          uint64_t                  frameId,
          DxvkSubmitStatus*         status) {
    DxvkSubmitInfo submitInfo = { };
    submitInfo.cmdList = commandList;

    DxvkLatencyInfo latencyInfo;
    latencyInfo.tracker = tracker;
    latencyInfo.frameId = frameId;

    m_submissionQueue.submit(submitInfo, latencyInfo, status);

    std::lock_guard<sync::Spinlock> statLock(m_statLock);
    m_statCounters.merge(commandList->statCounters());
  }
  
  
  VkResult DxvkDevice::waitForSubmission(DxvkSubmitStatus* status) {
    VkResult result = status->result.load();

    if (result == VK_NOT_READY) {
      m_submissionQueue.synchronizeSubmission(status);
      result = status->result.load();
    }

    return result;
  }


  void DxvkDevice::waitForFence(sync::Fence& fence, uint64_t value) {
    if (fence.value() >= value)
      return;

    auto t0 = dxvk::high_resolution_clock::now();

    fence.wait(value);

    auto t1 = dxvk::high_resolution_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

    m_statCounters.addCtr(DxvkStatCounter::GpuSyncCount, 1);
    m_statCounters.addCtr(DxvkStatCounter::GpuSyncTicks, us.count());
  }


  void DxvkDevice::waitForResource(const DxvkPagedResource& resource, DxvkAccess access) {
    if (resource.isInUse(access)) {
      auto t0 = dxvk::high_resolution_clock::now();

      m_submissionQueue.synchronizeUntil([&resource, access] {
        return !resource.isInUse(access);
      });

      auto t1 = dxvk::high_resolution_clock::now();
      auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

      std::lock_guard<sync::Spinlock> lock(m_statLock);
      m_statCounters.addCtr(DxvkStatCounter::GpuSyncCount, 1);
      m_statCounters.addCtr(DxvkStatCounter::GpuSyncTicks, us.count());
    }
  }
  
  
  void DxvkDevice::waitForIdle() {
    m_submissionQueue.waitForIdle();
    m_submissionQueue.lockDeviceQueue();

    if (m_vkd->vkDeviceWaitIdle(m_vkd->device()) != VK_SUCCESS)
      Logger::err("DxvkDevice: waitForIdle: Operation failed");

    m_submissionQueue.unlockDeviceQueue();
  }
  
  
  DxvkDevicePerfHints DxvkDevice::getPerfHints() {
    DxvkDevicePerfHints hints;
    hints.preferFbDepthStencilCopy = m_features.extShaderStencilExport
      && (m_adapter->matchesDriver(VK_DRIVER_ID_MESA_RADV_KHR)
       || m_adapter->matchesDriver(VK_DRIVER_ID_AMD_OPEN_SOURCE_KHR)
       || m_adapter->matchesDriver(VK_DRIVER_ID_AMD_PROPRIETARY_KHR));

    // Older Nvidia drivers sometimes use the wrong format
    // to interpret the clear color in render pass clears.
    hints.renderPassClearFormatBug = m_adapter->matchesDriver(
      VK_DRIVER_ID_NVIDIA_PROPRIETARY, Version(), Version(560, 28, 3));

    // On tilers we need to respect render passes some more. Most of
    // these drivers probably can't run DXVK anyway, but might as well
    bool tilerMode = m_adapter->matchesDriver(VK_DRIVER_ID_MESA_TURNIP)
                  || m_adapter->matchesDriver(VK_DRIVER_ID_QUALCOMM_PROPRIETARY)
                  || m_adapter->matchesDriver(VK_DRIVER_ID_MESA_HONEYKRISP)
                  || m_adapter->matchesDriver(VK_DRIVER_ID_MOLTENVK)
                  || m_adapter->matchesDriver(VK_DRIVER_ID_MESA_PANVK)
                  || m_adapter->matchesDriver(VK_DRIVER_ID_ARM_PROPRIETARY)
                  || m_adapter->matchesDriver(VK_DRIVER_ID_MESA_V3DV)
                  || m_adapter->matchesDriver(VK_DRIVER_ID_BROADCOM_PROPRIETARY)
                  || m_adapter->matchesDriver(VK_DRIVER_ID_IMAGINATION_OPEN_SOURCE_MESA)
                  || m_adapter->matchesDriver(VK_DRIVER_ID_IMAGINATION_PROPRIETARY);

    applyTristate(tilerMode, m_options.tilerMode);
    hints.preferRenderPassOps = tilerMode;

    // Be less aggressive on secondary command buffer usage on
    // drivers that do not natively support them
    hints.preferPrimaryCmdBufs = m_adapter->matchesDriver(VK_DRIVER_ID_MESA_HONEYKRISP) || !tilerMode;
    return hints;
  }


  void DxvkDevice::recycleCommandList(const Rc<DxvkCommandList>& cmdList) {
    m_recycledCommandLists.returnObject(cmdList);
  }
  
}
