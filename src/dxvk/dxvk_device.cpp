#include "dxvk_device.h"
#include "dxvk_instance.h"

namespace dxvk {
  
  DxvkDevice::DxvkDevice(
    const Rc<DxvkAdapter>&          adapter,
    const Rc<vk::DeviceFn>&         vkd,
    const DxvkDeviceExtensions&     extensions,
    const DxvkDeviceFeatures&       features)
  : m_options           (adapter->instance()->config()),
    m_adapter           (adapter),
    m_vkd               (vkd),
    m_extensions        (extensions),
    m_features          (features),
    m_properties        (adapter->deviceProperties()),
    m_memory            (new DxvkMemoryAllocator    (this)),
    m_renderPassPool    (new DxvkRenderPassPool     (vkd)),
    m_pipelineManager   (new DxvkPipelineManager    (this)),
    m_metaClearObjects  (new DxvkMetaClearObjects   (vkd)),
    m_metaMipGenObjects (new DxvkMetaMipGenObjects  (vkd)),
    m_metaResolveObjects(new DxvkMetaResolveObjects (vkd)),
    m_unboundResources  (this),
    m_submissionQueue   (this) {
    m_graphicsQueue.queueFamily = m_adapter->graphicsQueueFamily();
    m_presentQueue.queueFamily  = m_adapter->presentQueueFamily();
    
    m_vkd->vkGetDeviceQueue(m_vkd->device(),
      m_graphicsQueue.queueFamily, 0,
      &m_graphicsQueue.queueHandle);
    
    m_vkd->vkGetDeviceQueue(m_vkd->device(),
      m_presentQueue.queueFamily, 0,
      &m_presentQueue.queueHandle);
  }
  
  
  DxvkDevice::~DxvkDevice() {
    // Wait for all pending Vulkan commands to be
    // executed before we destroy any resources.
    m_vkd->vkDeviceWaitIdle(m_vkd->device());
  }


  DxvkDeviceOptions DxvkDevice::options() const {
    DxvkDeviceOptions options;
    options.maxNumDynamicUniformBuffers = m_properties.limits.maxDescriptorSetUniformBuffersDynamic;
    options.maxNumDynamicStorageBuffers = m_properties.limits.maxDescriptorSetStorageBuffersDynamic;
    return options;
  }
  
  
  Rc<DxvkPhysicalBuffer> DxvkDevice::allocPhysicalBuffer(
    const DxvkBufferCreateInfo& createInfo,
          VkMemoryPropertyFlags memoryType) {
    return new DxvkPhysicalBuffer(m_vkd,
      createInfo, *m_memory, memoryType);
  }
  
  
  Rc<DxvkStagingBuffer> DxvkDevice::allocStagingBuffer(VkDeviceSize size) {
    // In case we need a standard-size staging buffer, try
    // to recycle an old one that has been returned earlier
    if (size <= DefaultStagingBufferSize) {
      const Rc<DxvkStagingBuffer> buffer
        = m_recycledStagingBuffers.retrieveObject();
      
      if (buffer != nullptr)
        return buffer;
    }
    
    // Staging buffers only need to be able to handle transfer
    // operations, and they need to be in host-visible memory.
    DxvkBufferCreateInfo info;
    info.size   = size;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                | VK_PIPELINE_STAGE_HOST_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_HOST_WRITE_BIT;
    
    VkMemoryPropertyFlags memFlags
      = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    // Don't create buffers that are too small. A staging
    // buffer should be able to serve multiple uploads.
    if (info.size < DefaultStagingBufferSize)
      info.size = DefaultStagingBufferSize;
    
    return new DxvkStagingBuffer(this->createBuffer(info, memFlags));
  }
  
  
  void DxvkDevice::recycleStagingBuffer(const Rc<DxvkStagingBuffer>& buffer) {
    // Drop staging buffers that are bigger than the
    // standard ones to save memory, recycle the rest
    if (buffer->size() == DefaultStagingBufferSize) {
      m_recycledStagingBuffers.returnObject(buffer);
      buffer->reset();
    }
  }
  
  
  Rc<DxvkCommandList> DxvkDevice::createCommandList() {
    Rc<DxvkCommandList> cmdList = m_recycledCommandLists.retrieveObject();
    
    if (cmdList == nullptr) {
      cmdList = new DxvkCommandList(this,
        m_adapter->graphicsQueueFamily());
    }
    
    return cmdList;
  }
  
  
  Rc<DxvkContext> DxvkDevice::createContext() {
    return new DxvkContext(this,
      m_pipelineManager,
      m_metaClearObjects,
      m_metaMipGenObjects,
      m_metaResolveObjects);
  }
  
  
  Rc<DxvkFramebuffer> DxvkDevice::createFramebuffer(
    const DxvkRenderTargets& renderTargets) {
    const DxvkFramebufferSize defaultSize = {
      m_properties.limits.maxFramebufferWidth,
      m_properties.limits.maxFramebufferHeight,
      m_properties.limits.maxFramebufferLayers };
    
    auto renderPassFormat = DxvkFramebuffer::getRenderPassFormat(renderTargets);
    auto renderPassObject = m_renderPassPool->getRenderPass(renderPassFormat);
    
    return new DxvkFramebuffer(m_vkd,
      renderPassObject, renderTargets, defaultSize);
  }
  
  
  Rc<DxvkBuffer> DxvkDevice::createBuffer(
    const DxvkBufferCreateInfo& createInfo,
          VkMemoryPropertyFlags memoryType) {
    return new DxvkBuffer(this, createInfo, memoryType);
  }
  
  
  Rc<DxvkBufferView> DxvkDevice::createBufferView(
    const Rc<DxvkBuffer>&           buffer,
    const DxvkBufferViewCreateInfo& createInfo) {
    return new DxvkBufferView(m_vkd, buffer, createInfo);
  }
  
  
  Rc<DxvkImage> DxvkDevice::createImage(
    const DxvkImageCreateInfo&  createInfo,
          VkMemoryPropertyFlags memoryType) {
    return new DxvkImage(m_vkd, createInfo, *m_memory, memoryType);
  }
  
  
  Rc<DxvkImageView> DxvkDevice::createImageView(
    const Rc<DxvkImage>&            image,
    const DxvkImageViewCreateInfo&  createInfo) {
    return new DxvkImageView(m_vkd, image, createInfo);
  }
  
  
  Rc<DxvkSampler> DxvkDevice::createSampler(
    const DxvkSamplerCreateInfo&  createInfo) {
    return new DxvkSampler(m_vkd, createInfo);
  }
  
  
  Rc<DxvkSemaphore> DxvkDevice::createSemaphore() {
    return new DxvkSemaphore(m_vkd);
  }
  
  
  Rc<DxvkShader> DxvkDevice::createShader(
          VkShaderStageFlagBits     stage,
          uint32_t                  slotCount,
    const DxvkResourceSlot*         slotInfos,
    const DxvkInterfaceSlots&       iface,
    const SpirvCodeBuffer&          code) {
    return new DxvkShader(stage,
      slotCount, slotInfos, iface,
      code, DxvkShaderConstData());
  }
  
  
  Rc<DxvkSwapchain> DxvkDevice::createSwapchain(
    const Rc<DxvkSurface>&          surface,
    const DxvkSwapchainProperties&  properties) {
    return new DxvkSwapchain(this, surface, properties);
  }
  
  
  DxvkStatCounters DxvkDevice::getStatCounters() {
    DxvkMemoryStats mem = m_memory->getMemoryStats();
    DxvkPipelineCount pipe = m_pipelineManager->getPipelineCount();
    
    DxvkStatCounters result;
    result.setCtr(DxvkStatCounter::MemoryAllocated,   mem.memoryAllocated);
    result.setCtr(DxvkStatCounter::MemoryUsed,        mem.memoryUsed);
    result.setCtr(DxvkStatCounter::PipeCountGraphics, pipe.numGraphicsPipelines);
    result.setCtr(DxvkStatCounter::PipeCountCompute,  pipe.numComputePipelines);
    
    std::lock_guard<sync::Spinlock> lock(m_statLock);
    result.merge(m_statCounters);
    return result;
  }


  uint32_t DxvkDevice::getCurrentFrameId() const {
    return m_statCounters.getCtr(DxvkStatCounter::QueuePresentCount);
  }
  
  
  void DxvkDevice::initResources() {
    m_unboundResources.clearResources(this);
  }
  
  
  VkResult DxvkDevice::presentSwapImage(
    const VkPresentInfoKHR&         presentInfo) {
    { // Queue submissions are not thread safe
      std::lock_guard<std::mutex> queueLock(m_submissionLock);
      std::lock_guard<sync::Spinlock> statLock(m_statLock);
      
      m_statCounters.addCtr(DxvkStatCounter::QueuePresentCount, 1);
      return m_vkd->vkQueuePresentKHR(m_presentQueue.queueHandle, &presentInfo);
    }
  }
  
  
  void DxvkDevice::submitCommandList(
    const Rc<DxvkCommandList>&      commandList,
    const Rc<DxvkSemaphore>&        waitSync,
    const Rc<DxvkSemaphore>&        wakeSync) {
    VkSemaphore waitSemaphore = VK_NULL_HANDLE;
    VkSemaphore wakeSemaphore = VK_NULL_HANDLE;
    
    if (waitSync != nullptr) {
      waitSemaphore = waitSync->handle();
      commandList->trackResource(waitSync);
    }
    
    if (wakeSync != nullptr) {
      wakeSemaphore = wakeSync->handle();
      commandList->trackResource(wakeSync);
    }
    
    VkResult status;
    
    { // Queue submissions are not thread safe
      std::lock_guard<std::mutex> queueLock(m_submissionLock);
      std::lock_guard<sync::Spinlock> statLock(m_statLock);
      
      m_statCounters.merge(commandList->statCounters());
      m_statCounters.addCtr(DxvkStatCounter::QueueSubmitCount, 1);
      
      status = commandList->submit(
        m_graphicsQueue.queueHandle,
        waitSemaphore, wakeSemaphore);
    }
    
    if (status == VK_SUCCESS) {
      // Add this to the set of running submissions
      m_submissionQueue.submit(commandList);
    } else {
      Logger::err(str::format(
        "DxvkDevice: Command buffer submission failed: ",
        status));
    }
  }
  
  
  void DxvkDevice::waitForIdle() {
    if (m_vkd->vkDeviceWaitIdle(m_vkd->device()) != VK_SUCCESS)
      Logger::err("DxvkDevice: waitForIdle: Operation failed");
  }
  
  
  void DxvkDevice::recycleCommandList(const Rc<DxvkCommandList>& cmdList) {
    m_recycledCommandLists.returnObject(cmdList);
  }
  
}
