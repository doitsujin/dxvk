#include "dxvk_device.h"
#include "dxvk_instance.h"

namespace dxvk {
  
  DxvkDevice::DxvkDevice(
    const Rc<DxvkAdapter>&          adapter,
    const Rc<vk::DeviceFn>&         vkd,
    const VkPhysicalDeviceFeatures& features)
  : m_adapter         (adapter),
    m_vkd             (vkd),
    m_features        (features),
    m_memory          (new DxvkMemoryAllocator(adapter, vkd)),
    m_renderPassPool  (new DxvkRenderPassPool (vkd)),
    m_pipelineManager (new DxvkPipelineManager(vkd)) {
    m_vkd->vkGetDeviceQueue(m_vkd->device(),
      m_adapter->graphicsQueueFamily(), 0,
      &m_graphicsQueue);
    m_vkd->vkGetDeviceQueue(m_vkd->device(),
      m_adapter->presentQueueFamily(), 0,
      &m_presentQueue);
  }
  
  
  DxvkDevice::~DxvkDevice() {
    m_renderPassPool  = nullptr;
    m_pipelineManager = nullptr;
    m_memory          = nullptr;
    
    m_vkd->vkDeviceWaitIdle(m_vkd->device());
    m_vkd->vkDestroyDevice(m_vkd->device(), nullptr);
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
    if (buffer->size() == DefaultStagingBufferSize)
      m_recycledStagingBuffers.returnObject(buffer);
  }
  
  
  Rc<DxvkCommandList> DxvkDevice::createCommandList() {
    Rc<DxvkCommandList> cmdList = m_recycledCommandLists.retrieveObject();
    
    if (cmdList == nullptr) {
      cmdList = new DxvkCommandList(m_vkd,
        this, m_adapter->graphicsQueueFamily());
    }
    
    return cmdList;
  }
  
  
  Rc<DxvkContext> DxvkDevice::createContext() {
    return new DxvkContext(this);
  }
  
  
  Rc<DxvkFramebuffer> DxvkDevice::createFramebuffer(
    const DxvkRenderTargets& renderTargets) {
    auto format = renderTargets.renderPassFormat();
    auto renderPass = m_renderPassPool->getRenderPass(format);
    return new DxvkFramebuffer(m_vkd, renderPass, renderTargets);
  }
  
  
  Rc<DxvkBuffer> DxvkDevice::createBuffer(
    const DxvkBufferCreateInfo& createInfo,
          VkMemoryPropertyFlags memoryType) {
    return new DxvkBuffer(m_vkd,
      createInfo, *m_memory, memoryType);
  }
  
  
  Rc<DxvkBufferView> DxvkDevice::createBufferView(
    const Rc<DxvkBuffer>&           buffer,
    const DxvkBufferViewCreateInfo& createInfo) {
    return new DxvkBufferView(m_vkd, buffer, createInfo);
  }
  
  
  Rc<DxvkImage> DxvkDevice::createImage(
    const DxvkImageCreateInfo&  createInfo,
          VkMemoryPropertyFlags memoryType) {
    return new DxvkImage(m_vkd,
      createInfo, *m_memory, memoryType);
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
    const SpirvCodeBuffer&          code) {
    return new DxvkShader(stage,
      slotCount, slotInfos, code);
  }
  
  
  Rc<DxvkComputePipeline> DxvkDevice::createComputePipeline(
    const Rc<DxvkShader>&           cs) {
    return m_pipelineManager->createComputePipeline(cs);
  }
  
  
  Rc<DxvkGraphicsPipeline> DxvkDevice::createGraphicsPipeline(
    const Rc<DxvkShader>&           vs,
    const Rc<DxvkShader>&           tcs,
    const Rc<DxvkShader>&           tes,
    const Rc<DxvkShader>&           gs,
    const Rc<DxvkShader>&           fs) {
    return m_pipelineManager->createGraphicsPipeline(vs, tcs, tes, gs, fs);
  }
  
  
  Rc<DxvkSwapchain> DxvkDevice::createSwapchain(
    const Rc<DxvkSurface>&          surface,
    const DxvkSwapchainProperties&  properties) {
    return new DxvkSwapchain(this, surface, properties, m_presentQueue);
  }
  
  
  Rc<DxvkFence> DxvkDevice::submitCommandList(
    const Rc<DxvkCommandList>&      commandList,
    const Rc<DxvkSemaphore>&        waitSync,
    const Rc<DxvkSemaphore>&        wakeSync) {
    Rc<DxvkFence> fence = new DxvkFence(m_vkd);
    
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
    
    commandList->submit(m_graphicsQueue,
      waitSemaphore, wakeSemaphore, fence->handle());
    
    // TODO Delay synchronization by putting these into a ring buffer
    fence->wait(std::numeric_limits<uint64_t>::max());
    commandList->reset();
    
    // FIXME this must go away once the ring buffer is implemented
    m_recycledCommandLists.returnObject(commandList);
    return fence;
  }
  
  
  void DxvkDevice::waitForIdle() const {
    if (m_vkd->vkDeviceWaitIdle(m_vkd->device()) != VK_SUCCESS)
      throw DxvkError("DxvkDevice::waitForIdle: Operation failed");
  }
  
}