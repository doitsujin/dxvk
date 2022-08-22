#include "dxvk_cmdlist.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkCommandSubmission::DxvkCommandSubmission() {

  }


  DxvkCommandSubmission::~DxvkCommandSubmission() {

  }


  void DxvkCommandSubmission::waitSemaphore(
          VkSemaphore           semaphore,
          uint64_t              value,
          VkPipelineStageFlags2 stageMask) {
    VkSemaphoreSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    submitInfo.semaphore = semaphore;
    submitInfo.value     = value;
    submitInfo.stageMask = stageMask;

    m_semaphoreWaits.push_back(submitInfo);
  }


  void DxvkCommandSubmission::signalSemaphore(
          VkSemaphore           semaphore,
          uint64_t              value,
          VkPipelineStageFlags2 stageMask) {
    VkSemaphoreSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
    submitInfo.semaphore = semaphore;
    submitInfo.value     = value;
    submitInfo.stageMask = stageMask;

    m_semaphoreSignals.push_back(submitInfo);
  }


  void DxvkCommandSubmission::executeCommandBuffer(
          VkCommandBuffer       commandBuffer) {
    VkCommandBufferSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
    submitInfo.commandBuffer = commandBuffer;

    m_commandBuffers.push_back(submitInfo);
  }


  VkResult DxvkCommandSubmission::submit(
          DxvkDevice*           device,
          VkQueue               queue) {
    auto vk = device->vkd();

    VkSubmitInfo2 submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };

    if (!m_semaphoreWaits.empty()) {
      submitInfo.waitSemaphoreInfoCount = m_semaphoreWaits.size();
      submitInfo.pWaitSemaphoreInfos = m_semaphoreWaits.data();
    }

    if (!m_commandBuffers.empty()) {
      submitInfo.commandBufferInfoCount = m_commandBuffers.size();
      submitInfo.pCommandBufferInfos = m_commandBuffers.data();
    }

    if (!m_semaphoreSignals.empty()) {
      submitInfo.signalSemaphoreInfoCount = m_semaphoreSignals.size();
      submitInfo.pSignalSemaphoreInfos = m_semaphoreSignals.data();
    }

    VkResult vr = VK_SUCCESS;

    if (submitInfo.waitSemaphoreInfoCount || submitInfo.commandBufferInfoCount || submitInfo.signalSemaphoreInfoCount)
      vr = vk->vkQueueSubmit2(queue, 1, &submitInfo, VK_NULL_HANDLE);

    this->reset();
    return vr;
  }


  void DxvkCommandSubmission::reset() {
    m_semaphoreWaits.clear();
    m_semaphoreSignals.clear();
    m_commandBuffers.clear();
  }


  DxvkCommandList::DxvkCommandList(DxvkDevice* device)
  : m_device        (device),
    m_vkd           (device->vkd()),
    m_vki           (device->instance()->vki()),
    m_cmdBuffersUsed(0) {
    const auto& graphicsQueue = m_device->queues().graphics;
    const auto& transferQueue = m_device->queues().transfer;

    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    if (m_vkd->vkCreateSemaphore(m_vkd->device(), &semaphoreInfo, nullptr, &m_sdmaSemaphore))
      throw DxvkError("DxvkCommandList: Failed to create semaphore");

    VkCommandPoolCreateInfo poolInfo;
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext            = nullptr;
    poolInfo.flags            = 0;
    poolInfo.queueFamilyIndex = graphicsQueue.queueFamily;
    
    if (m_vkd->vkCreateCommandPool(m_vkd->device(), &poolInfo, nullptr, &m_graphicsPool) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList: Failed to create graphics command pool");
    
    if (m_device->hasDedicatedTransferQueue()) {
      poolInfo.queueFamilyIndex = transferQueue.queueFamily;

      if (m_vkd->vkCreateCommandPool(m_vkd->device(), &poolInfo, nullptr, &m_transferPool) != VK_SUCCESS)
        throw DxvkError("DxvkCommandList: Failed to create transfer command pool");
    }
    
    VkCommandBufferAllocateInfo cmdInfoGfx;
    cmdInfoGfx.sType             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfoGfx.pNext             = nullptr;
    cmdInfoGfx.commandPool       = m_graphicsPool;
    cmdInfoGfx.level             = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfoGfx.commandBufferCount = 1;
    
    VkCommandBufferAllocateInfo cmdInfoDma;
    cmdInfoDma.sType             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfoDma.pNext             = nullptr;
    cmdInfoDma.commandPool       = m_transferPool ? m_transferPool : m_graphicsPool;
    cmdInfoDma.level             = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfoDma.commandBufferCount = 1;
    
    if (m_vkd->vkAllocateCommandBuffers(m_vkd->device(), &cmdInfoGfx, &m_execBuffer) != VK_SUCCESS
     || m_vkd->vkAllocateCommandBuffers(m_vkd->device(), &cmdInfoGfx, &m_initBuffer) != VK_SUCCESS
     || m_vkd->vkAllocateCommandBuffers(m_vkd->device(), &cmdInfoDma, &m_sdmaBuffer) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList: Failed to allocate command buffer");
  }
  
  
  DxvkCommandList::~DxvkCommandList() {
    this->reset();

    m_vkd->vkDestroyCommandPool(m_vkd->device(), m_graphicsPool, nullptr);
    m_vkd->vkDestroyCommandPool(m_vkd->device(), m_transferPool, nullptr);

    m_vkd->vkDestroySemaphore(m_vkd->device(), m_sdmaSemaphore, nullptr);
  }
  
  
  VkResult DxvkCommandList::submit(
          VkSemaphore       semaphore,
          uint64_t&         semaphoreValue) {
    VkResult status = VK_SUCCESS;

    const auto& graphics = m_device->queues().graphics;
    const auto& transfer = m_device->queues().transfer;

    m_commandSubmission.reset();

    if (m_cmdBuffersUsed.test(DxvkCmdBuffer::SdmaBuffer)) {
      m_commandSubmission.executeCommandBuffer(m_sdmaBuffer);

      if (m_device->hasDedicatedTransferQueue()) {
        m_commandSubmission.signalSemaphore(m_sdmaSemaphore, 0, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

        if ((status = m_commandSubmission.submit(m_device, transfer.queueHandle)))
          return status;

        m_commandSubmission.waitSemaphore(m_sdmaSemaphore, 0, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
      }
    }

    if (m_cmdBuffersUsed.test(DxvkCmdBuffer::InitBuffer))
      m_commandSubmission.executeCommandBuffer(m_initBuffer);

    if (m_cmdBuffersUsed.test(DxvkCmdBuffer::ExecBuffer))
      m_commandSubmission.executeCommandBuffer(m_execBuffer);

    if (m_wsiSemaphores.acquire) {
      m_commandSubmission.waitSemaphore(m_wsiSemaphores.acquire,
        0, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
    }

    if (m_wsiSemaphores.present) {
      m_commandSubmission.signalSemaphore(m_wsiSemaphores.present,
        0, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
    }

    for (const auto& entry : m_waitSemaphores) {
      m_commandSubmission.waitSemaphore(
        entry.fence->handle(),
        entry.value, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
    }

    for (const auto& entry : m_signalSemaphores)
      m_commandSubmission.signalSemaphore(entry.fence->handle(), entry.value, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    m_commandSubmission.signalSemaphore(semaphore,
      ++semaphoreValue, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    if ((status = m_commandSubmission.submit(m_device, graphics.queueHandle)))
      return status;

    return VK_SUCCESS;
  }
  
  
  void DxvkCommandList::beginRecording() {
    VkCommandBufferBeginInfo info;
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext            = nullptr;
    info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = nullptr;
    
    if ((m_graphicsPool && m_vkd->vkResetCommandPool(m_vkd->device(), m_graphicsPool, 0) != VK_SUCCESS)
     || (m_transferPool && m_vkd->vkResetCommandPool(m_vkd->device(), m_transferPool, 0) != VK_SUCCESS))
      Logger::err("DxvkCommandList: Failed to reset command buffer");
    
    if (m_vkd->vkBeginCommandBuffer(m_execBuffer, &info) != VK_SUCCESS
     || m_vkd->vkBeginCommandBuffer(m_initBuffer, &info) != VK_SUCCESS
     || m_vkd->vkBeginCommandBuffer(m_sdmaBuffer, &info) != VK_SUCCESS)
      Logger::err("DxvkCommandList: Failed to begin command buffer");
    
    // Unconditionally mark the exec buffer as used. There
    // is virtually no use case where this isn't correct.
    m_cmdBuffersUsed = DxvkCmdBuffer::ExecBuffer;
  }
  
  
  void DxvkCommandList::endRecording() {
    if (m_vkd->vkEndCommandBuffer(m_execBuffer) != VK_SUCCESS
     || m_vkd->vkEndCommandBuffer(m_initBuffer) != VK_SUCCESS
     || m_vkd->vkEndCommandBuffer(m_sdmaBuffer) != VK_SUCCESS)
      Logger::err("DxvkCommandList::endRecording: Failed to record command buffer");
  }
  
  
  void DxvkCommandList::reset() {
    // Free resources and other objects
    // that are no longer in use
    m_resources.reset();

    // Return buffer memory slices
    m_bufferTracker.reset();

    // Return query and event handles
    m_gpuQueryTracker.reset();
    m_gpuEventTracker.reset();

    // Less important stuff
    m_signalTracker.reset();
    m_statCounters.reset();

    // Recycle descriptor pools
    for (const auto& descriptorPools : m_descriptorPools)
      descriptorPools.second->recycleDescriptorPool(descriptorPools.first);

    m_descriptorPools.clear();

    // Release pipelines
    for (auto pipeline : m_pipelines)
      pipeline->releasePipeline();

    m_pipelines.clear();

    m_waitSemaphores.clear();
    m_signalSemaphores.clear();

    m_wsiSemaphores = vk::PresenterSync();
  }

}
