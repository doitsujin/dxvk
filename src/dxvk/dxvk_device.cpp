#include "dxvk_device.h"
#include "dxvk_instance.h"

namespace dxvk {
  
  DxvkDevice::DxvkDevice(
          std::string               clientApi,
    const Rc<DxvkAdapter>&          adapter,
    const Rc<vk::DeviceFn>&         vkd,
    const DxvkDeviceExtensions&     extensions,
    const DxvkDeviceFeatures&       features)
  : m_clientApi         (clientApi),
    m_options           (adapter->instance()->options()),
    m_adapter           (adapter),
    m_vkd               (vkd),
    m_extensions        (extensions),
    m_features          (features),
    m_properties        (adapter->deviceProperties()),
    m_memory            (new DxvkMemoryAllocator    (this)),
    m_renderPassPool    (new DxvkRenderPassPool     (vkd)),
    m_pipelineManager   (new DxvkPipelineManager    (this, m_renderPassPool.ptr())),
    m_gpuEventPool      (new DxvkGpuEventPool       (vkd)),
    m_gpuQueryPool      (new DxvkGpuQueryPool       (this)),
    m_metaClearObjects  (new DxvkMetaClearObjects   (vkd)),
    m_metaCopyObjects   (new DxvkMetaCopyObjects    (vkd)),
    m_metaResolveObjects(new DxvkMetaResolveObjects (this)),
    m_metaMipGenObjects (new DxvkMetaMipGenObjects  (vkd)),
    m_metaPackObjects   (new DxvkMetaPackObjects    (vkd)),
    m_unboundResources  (this),
    m_submissionQueue   (this) {
    auto queueFamilies = m_adapter->findQueueFamilies();
    m_queues.graphics = getQueue(queueFamilies.graphics, 0);
    m_queues.transfer = getQueue(queueFamilies.transfer, 0);
  }
  
  
  DxvkDevice::~DxvkDevice() {
    // Wait for all pending Vulkan commands to be
    // executed before we destroy any resources.
    m_vkd->vkDeviceWaitIdle(m_vkd->device());
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
    options.maxNumDynamicUniformBuffers = m_properties.limits.maxDescriptorSetUniformBuffersDynamic;
    options.maxNumDynamicStorageBuffers = m_properties.limits.maxDescriptorSetStorageBuffersDynamic;
    return options;
  }
  
  
  Rc<DxvkCommandList> DxvkDevice::createCommandList() {
    Rc<DxvkCommandList> cmdList = m_recycledCommandLists.retrieveObject();
    
    if (cmdList == nullptr)
      cmdList = new DxvkCommandList(this, m_queues.graphics.queueFamily);
    
    return cmdList;
  }


  Rc<DxvkDescriptorPool> DxvkDevice::createDescriptorPool() {
    Rc<DxvkDescriptorPool> pool = m_recycledDescriptorPools.retrieveObject();

    if (pool == nullptr)
      pool = new DxvkDescriptorPool(m_vkd);
    
    return pool;
  }
  
  
  Rc<DxvkContext> DxvkDevice::createContext() {
    return new DxvkContext(this,
      m_pipelineManager,
      m_gpuEventPool,
      m_gpuQueryPool,
      m_metaClearObjects,
      m_metaCopyObjects,
      m_metaResolveObjects,
      m_metaMipGenObjects,
      m_metaPackObjects);
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
    return new DxvkBuffer(this, createInfo, *m_memory, memoryType);
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
  
  
  Rc<DxvkShader> DxvkDevice::createShader(
          VkShaderStageFlagBits     stage,
          uint32_t                  slotCount,
    const DxvkResourceSlot*         slotInfos,
    const DxvkInterfaceSlots&       iface,
    const SpirvCodeBuffer&          code) {
    return new DxvkShader(stage,
      slotCount, slotInfos, iface, code,
      DxvkShaderOptions(),
      DxvkShaderConstData());
  }
  
  
  DxvkStatCounters DxvkDevice::getStatCounters() {
    DxvkMemoryStats mem = m_memory->getMemoryStats();
    DxvkPipelineCount pipe = m_pipelineManager->getPipelineCount();
    
    DxvkStatCounters result;
    result.setCtr(DxvkStatCounter::MemoryAllocated,   mem.memoryAllocated);
    result.setCtr(DxvkStatCounter::MemoryUsed,        mem.memoryUsed);
    result.setCtr(DxvkStatCounter::PipeCountGraphics, pipe.numGraphicsPipelines);
    result.setCtr(DxvkStatCounter::PipeCountCompute,  pipe.numComputePipelines);
    result.setCtr(DxvkStatCounter::PipeCompilerBusy,  m_pipelineManager->isCompilingShaders());

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


  void DxvkDevice::registerShader(const Rc<DxvkShader>& shader) {
    m_pipelineManager->registerShader(shader);
  }
  
  
  VkResult DxvkDevice::presentImage(
    const Rc<vk::Presenter>&        presenter,
          VkSemaphore               semaphore) {
    DxvkPresentInfo presentInfo;
    presentInfo.presenter = presenter;
    presentInfo.waitSync  = semaphore;
    VkResult status = m_submissionQueue.present(presentInfo);

    if (status != VK_SUCCESS)
      return status;
    
    std::lock_guard<sync::Spinlock> statLock(m_statLock);
    m_statCounters.addCtr(DxvkStatCounter::QueuePresentCount, 1);
    return status;
  }


  void DxvkDevice::submitCommandList(
    const Rc<DxvkCommandList>&      commandList,
          VkSemaphore               waitSync,
          VkSemaphore               wakeSync) {
    DxvkSubmitInfo submitInfo;
    submitInfo.cmdList  = commandList;
    submitInfo.queue    = m_queues.graphics.queueHandle;
    submitInfo.waitSync = waitSync;
    submitInfo.wakeSync = wakeSync;
    m_submissionQueue.submit(submitInfo);

    std::lock_guard<sync::Spinlock> statLock(m_statLock);
    m_statCounters.merge(commandList->statCounters());
    m_statCounters.addCtr(DxvkStatCounter::QueueSubmitCount, 1);
  }
  
  
  void DxvkDevice::waitForIdle() {
    m_submissionQueue.synchronize();

    if (m_vkd->vkDeviceWaitIdle(m_vkd->device()) != VK_SUCCESS)
      Logger::err("DxvkDevice: waitForIdle: Operation failed");
  }
  
  
  void DxvkDevice::recycleCommandList(const Rc<DxvkCommandList>& cmdList) {
    m_recycledCommandLists.returnObject(cmdList);
  }
  

  void DxvkDevice::recycleDescriptorPool(const Rc<DxvkDescriptorPool>& pool) {
    m_recycledDescriptorPools.returnObject(pool);
  }


  DxvkDeviceQueue DxvkDevice::getQueue(
          uint32_t                family,
          uint32_t                index) const {
    VkQueue queue = VK_NULL_HANDLE;
    m_vkd->vkGetDeviceQueue(m_vkd->device(), family, index, &queue);
    return DxvkDeviceQueue { queue, family, index };
  }
  
}
