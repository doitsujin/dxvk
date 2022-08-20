#include "dxvk_device.h"
#include "dxvk_instance.h"

namespace dxvk {
  
  DxvkDevice::DxvkDevice(
    const Rc<DxvkInstance>&         instance,
    const Rc<DxvkAdapter>&          adapter,
    const Rc<vk::DeviceFn>&         vkd,
    const DxvkDeviceExtensions&     extensions,
    const DxvkDeviceFeatures&       features)
  : m_options           (instance->options()),
    m_instance          (instance),
    m_adapter           (adapter),
    m_vkd               (vkd),
    m_extensions        (extensions),
    m_features          (features),
    m_properties        (adapter->devicePropertiesExt()),
    m_perfHints         (getPerfHints()),
    m_objects           (this),
    m_submissionQueue   (this) {
    auto queueFamilies = m_adapter->findQueueFamilies();
    m_queues.graphics = getQueue(queueFamilies.graphics, 0);
    m_queues.transfer = getQueue(queueFamilies.transfer, 0);
    m_queues.sparse = getQueue(queueFamilies.sparse, 0);
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


  std::optional<DxvkFormatLimits> DxvkDevice::getFormatLimits(
          VkFormat                  format,
          VkImageType               type,
          VkImageTiling             tiling,
          VkImageUsageFlags         usage,
          VkImageCreateFlags        flags) const {
    auto vk = m_adapter->vki();

    VkPhysicalDeviceImageFormatInfo2 info = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2 };
    info.format = format;
    info.type   = type;
    info.tiling = tiling;
    info.usage  = usage;
    info.flags  = flags;

    VkImageFormatProperties2 properties = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 };

    VkResult vr = vk->vkGetPhysicalDeviceImageFormatProperties2(
      m_adapter->handle(), &info, &properties);

    if (vr != VK_SUCCESS)
      return std::nullopt;

    DxvkFormatLimits result;
    result.maxExtent        = properties.imageFormatProperties.maxExtent;
    result.maxMipLevels     = properties.imageFormatProperties.maxMipLevels;
    result.maxArrayLayers   = properties.imageFormatProperties.maxArrayLayers;
    result.sampleCounts     = properties.imageFormatProperties.sampleCounts;
    result.maxResourceSize  = properties.imageFormatProperties.maxResourceSize;
    return result;
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
    bool result = env::is32BitHostPlatform();
    applyTristate(result, m_options.trackPipelineLifetime);
    return result && canUseGraphicsPipelineLibrary();
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


  DxvkDeviceOptions DxvkDevice::options() const {
    DxvkDeviceOptions options;
    options.maxNumDynamicUniformBuffers = m_properties.core.properties.limits.maxDescriptorSetUniformBuffersDynamic;
    options.maxNumDynamicStorageBuffers = m_properties.core.properties.limits.maxDescriptorSetStorageBuffersDynamic;
    return options;
  }
  
  
  Rc<DxvkCommandList> DxvkDevice::createCommandList() {
    Rc<DxvkCommandList> cmdList = m_recycledCommandLists.retrieveObject();
    
    if (cmdList == nullptr)
      cmdList = new DxvkCommandList(this);
    
    return cmdList;
  }


  Rc<DxvkContext> DxvkDevice::createContext(DxvkContextType type) {
    return new DxvkContext(this, type);
  }


  Rc<DxvkGpuEvent> DxvkDevice::createGpuEvent() {
    return new DxvkGpuEvent(m_vkd);
  }


  Rc<DxvkGpuQuery> DxvkDevice::createGpuQuery(
          VkQueryType           type,
          VkQueryControlFlags   flags,
          uint32_t              index) {
    return new DxvkGpuQuery(m_vkd, type, flags, index);
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
  
  
  Rc<DxvkBufferView> DxvkDevice::createBufferView(
    const Rc<DxvkBuffer>&           buffer,
    const DxvkBufferViewCreateInfo& createInfo) {
    return new DxvkBufferView(m_vkd, buffer, createInfo);
  }
  
  
  Rc<DxvkImage> DxvkDevice::createImage(
    const DxvkImageCreateInfo&  createInfo,
          VkMemoryPropertyFlags memoryType) {
    return new DxvkImage(this, createInfo, m_objects.memoryManager(), memoryType);
  }
  
  
  Rc<DxvkImage> DxvkDevice::createImageFromVkImage(
    const DxvkImageCreateInfo&  createInfo,
          VkImage               image) {
    return new DxvkImage(this, createInfo, image);
  }
  
  Rc<DxvkImageView> DxvkDevice::createImageView(
    const Rc<DxvkImage>&            image,
    const DxvkImageViewCreateInfo&  createInfo) {
    return new DxvkImageView(m_vkd, image, createInfo);
  }
  
  
  Rc<DxvkSampler> DxvkDevice::createSampler(
    const DxvkSamplerCreateInfo&  createInfo) {
    return new DxvkSampler(this, createInfo);
  }
  
  
  Rc<DxvkSparsePageAllocator> DxvkDevice::createSparsePageAllocator() {
    return new DxvkSparsePageAllocator(this, m_objects.memoryManager());
  }


  DxvkStatCounters DxvkDevice::getStatCounters() {
    DxvkPipelineCount pipe = m_objects.pipelineManager().getPipelineCount();
    
    DxvkStatCounters result;
    result.setCtr(DxvkStatCounter::PipeCountGraphics, pipe.numGraphicsPipelines);
    result.setCtr(DxvkStatCounter::PipeCountLibrary,  pipe.numGraphicsLibraries);
    result.setCtr(DxvkStatCounter::PipeCountCompute,  pipe.numComputePipelines);
    result.setCtr(DxvkStatCounter::PipeCompilerBusy,  m_objects.pipelineManager().isCompilingShaders());
    result.setCtr(DxvkStatCounter::GpuIdleTicks,      m_submissionQueue.gpuIdleTicks());

    std::lock_guard<sync::Spinlock> lock(m_statLock);
    result.merge(m_statCounters);
    return result;
  }
  
  
  DxvkMemoryStats DxvkDevice::getMemoryStats(uint32_t heap) {
    return m_objects.memoryManager().getMemoryStats(heap);
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


  void DxvkDevice::presentImage(
    const Rc<vk::Presenter>&        presenter,
          DxvkSubmitStatus*         status) {
    status->result = VK_NOT_READY;

    DxvkPresentInfo presentInfo;
    presentInfo.presenter = presenter;
    m_submissionQueue.present(presentInfo, status);
    
    std::lock_guard<sync::Spinlock> statLock(m_statLock);
    m_statCounters.addCtr(DxvkStatCounter::QueuePresentCount, 1);
  }


  void DxvkDevice::submitCommandList(
    const Rc<DxvkCommandList>&      commandList) {
    DxvkSubmitInfo submitInfo = { };
    submitInfo.cmdList = commandList;
    m_submissionQueue.submit(submitInfo);

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


  void DxvkDevice::waitForResource(const Rc<DxvkResource>& resource, DxvkAccess access) {
    if (resource->isInUse(access)) {
      auto t0 = dxvk::high_resolution_clock::now();

      m_submissionQueue.synchronizeUntil([resource, access] {
        return !resource->isInUse(access);
      });

      auto t1 = dxvk::high_resolution_clock::now();
      auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

      std::lock_guard<sync::Spinlock> lock(m_statLock);
      m_statCounters.addCtr(DxvkStatCounter::GpuSyncCount, 1);
      m_statCounters.addCtr(DxvkStatCounter::GpuSyncTicks, us.count());
    }
  }
  
  
  void DxvkDevice::waitForIdle() {
    this->lockSubmission();
    if (m_vkd->vkDeviceWaitIdle(m_vkd->device()) != VK_SUCCESS)
      Logger::err("DxvkDevice: waitForIdle: Operation failed");
    this->unlockSubmission();
  }
  
  
  DxvkDevicePerfHints DxvkDevice::getPerfHints() {
    DxvkDevicePerfHints hints;
    hints.preferFbDepthStencilCopy = m_extensions.extShaderStencilExport
      && (m_adapter->matchesDriver(VK_DRIVER_ID_MESA_RADV_KHR, 0, 0)
       || m_adapter->matchesDriver(VK_DRIVER_ID_AMD_OPEN_SOURCE_KHR, 0, 0)
       || m_adapter->matchesDriver(VK_DRIVER_ID_AMD_PROPRIETARY_KHR, 0, 0));
    hints.preferFbResolve = m_extensions.amdShaderFragmentMask
      && (m_adapter->matchesDriver(VK_DRIVER_ID_AMD_OPEN_SOURCE_KHR, 0, 0)
       || m_adapter->matchesDriver(VK_DRIVER_ID_AMD_PROPRIETARY_KHR, 0, 0));
    return hints;
  }


  void DxvkDevice::recycleCommandList(const Rc<DxvkCommandList>& cmdList) {
    m_recycledCommandLists.returnObject(cmdList);
  }
  

  DxvkDeviceQueue DxvkDevice::getQueue(
          uint32_t                family,
          uint32_t                index) const {
    VkQueue queue = VK_NULL_HANDLE;

    if (family != VK_QUEUE_FAMILY_IGNORED)
      m_vkd->vkGetDeviceQueue(m_vkd->device(), family, index, &queue);

    return DxvkDeviceQueue { queue, family, index };
  }
  
}
