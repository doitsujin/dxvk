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
  
  
  Rc<DxvkCommandList> DxvkDevice::createCommandList() {
    return new DxvkCommandList(m_vkd,
      m_adapter->graphicsQueueFamily());
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
    return new DxvkShader(m_vkd, stage,
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
    return fence;
  }
  
  
  void DxvkDevice::waitForIdle() const {
    if (m_vkd->vkDeviceWaitIdle(m_vkd->device()) != VK_SUCCESS)
      throw DxvkError("DxvkDevice::waitForIdle: Operation failed");
  }
  
}