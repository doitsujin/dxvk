#include "dxvk_cmdlist.h"
#include "dxvk_device.h"

namespace dxvk {
    
  DxvkCommandList::DxvkCommandList(DxvkDevice* device)
  : m_device        (device),
    m_vkd           (device->vkd()),
    m_vki           (device->instance()->vki()),
    m_cmdBuffersUsed(0) {
    const auto& graphicsQueue = m_device->queues().graphics;
    const auto& transferQueue = m_device->queues().transfer;

    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = 0;
    
    if (m_vkd->vkCreateFence(m_vkd->device(), &fenceInfo, nullptr, &m_fence) != VK_SUCCESS)
      throw DxvkError("DxvkCommandList: Failed to create fence");
    
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
    
    if (m_device->hasDedicatedTransferQueue()) {
      VkSemaphoreCreateInfo semInfo;
      semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      semInfo.pNext = nullptr;
      semInfo.flags = 0;

      if (m_vkd->vkCreateSemaphore(m_vkd->device(), &semInfo, nullptr, &m_sdmaSemaphore) != VK_SUCCESS)
        throw DxvkError("DxvkCommandList: Failed to create semaphore");
    }
  }
  
  
  DxvkCommandList::~DxvkCommandList() {
    this->reset();

    m_vkd->vkDestroySemaphore(m_vkd->device(), m_sdmaSemaphore, nullptr);
    
    m_vkd->vkDestroyCommandPool(m_vkd->device(), m_graphicsPool, nullptr);
    m_vkd->vkDestroyCommandPool(m_vkd->device(), m_transferPool, nullptr);
    
    m_vkd->vkDestroyFence(m_vkd->device(), m_fence, nullptr);
  }
  
  
  VkResult DxvkCommandList::submit(
          VkSemaphore     waitSemaphore,
          VkSemaphore     wakeSemaphore) {
    const auto& graphics = m_device->queues().graphics;
    const auto& transfer = m_device->queues().transfer;

    m_submission.reset();

    if (m_cmdBuffersUsed.test(DxvkCmdBuffer::SdmaBuffer)) {
      VkCommandBufferSubmitInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
      cmdInfo.commandBuffer = m_sdmaBuffer;
      m_submission.cmdBuffers.push_back(cmdInfo);

      if (m_device->hasDedicatedTransferQueue()) {
        VkSemaphoreSubmitInfo signalInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
        signalInfo.semaphore = m_sdmaSemaphore;
        signalInfo.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        m_submission.wakeSync.push_back(signalInfo);

        VkResult status = submitToQueue(transfer.queueHandle, VK_NULL_HANDLE, m_submission);

        if (status != VK_SUCCESS)
          return status;

        m_submission.reset();

        VkSemaphoreSubmitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
        waitInfo.semaphore = m_sdmaSemaphore;
        waitInfo.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        m_submission.waitSync.push_back(waitInfo);
      }
    }

    if (m_cmdBuffersUsed.test(DxvkCmdBuffer::InitBuffer)) {
      VkCommandBufferSubmitInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
      cmdInfo.commandBuffer = m_initBuffer;
      m_submission.cmdBuffers.push_back(cmdInfo);
    }

    if (m_cmdBuffersUsed.test(DxvkCmdBuffer::ExecBuffer)) {
      VkCommandBufferSubmitInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
      cmdInfo.commandBuffer = m_execBuffer;
      m_submission.cmdBuffers.push_back(cmdInfo);
    }
    
    if (waitSemaphore) {
      VkSemaphoreSubmitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
      waitInfo.semaphore = waitSemaphore;
      waitInfo.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
      m_submission.waitSync.push_back(waitInfo);
    }

    if (wakeSemaphore) {
      VkSemaphoreSubmitInfo signalInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
      signalInfo.semaphore = wakeSemaphore;
      signalInfo.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
      m_submission.wakeSync.push_back(signalInfo);
    }
    
    for (const auto& entry : m_waitSemaphores) {
      VkSemaphoreSubmitInfo waitInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
      waitInfo.semaphore = entry.fence->handle();
      waitInfo.value = entry.value;
      waitInfo.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
      m_submission.waitSync.push_back(waitInfo);
    }

    for (const auto& entry : m_signalSemaphores) {
      VkSemaphoreSubmitInfo signalInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
      signalInfo.semaphore = entry.fence->handle();
      signalInfo.value = entry.value;
      signalInfo.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
      m_submission.wakeSync.push_back(signalInfo);
    }

    return submitToQueue(graphics.queueHandle, m_fence, m_submission);
  }
  
  
  VkResult DxvkCommandList::synchronize() {
    VkResult status = VK_TIMEOUT;
    
    while (status == VK_TIMEOUT) {
      status = m_vkd->vkWaitForFences(
        m_vkd->device(), 1, &m_fence, VK_FALSE,
        1'000'000'000ull);
    }
    
    return status;
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
    
    if (m_vkd->vkResetFences(m_vkd->device(), 1, &m_fence) != VK_SUCCESS)
      Logger::err("DxvkCommandList: Failed to reset fence");
    
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
  }


  VkResult DxvkCommandList::submitToQueue(
          VkQueue               queue,
          VkFence               fence,
    const DxvkQueueSubmission&  info) {
    VkSubmitInfo2 submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
    submitInfo.waitSemaphoreInfoCount   = info.waitSync.size();
    submitInfo.pWaitSemaphoreInfos      = info.waitSync.data();
    submitInfo.commandBufferInfoCount   = info.cmdBuffers.size();
    submitInfo.pCommandBufferInfos      = info.cmdBuffers.data();
    submitInfo.signalSemaphoreInfoCount = info.wakeSync.size();
    submitInfo.pSignalSemaphoreInfos    = info.wakeSync.data();
    
    return m_vkd->vkQueueSubmit2(queue, 1, &submitInfo, fence);
  }
  
  void DxvkCommandList::cmdBeginDebugUtilsLabel(VkDebugUtilsLabelEXT *pLabelInfo) {
    m_vki->vkCmdBeginDebugUtilsLabelEXT(m_execBuffer, pLabelInfo);
  }

  void DxvkCommandList::cmdEndDebugUtilsLabel() {
    m_vki->vkCmdEndDebugUtilsLabelEXT(m_execBuffer);
  }

  void DxvkCommandList::cmdInsertDebugUtilsLabel(VkDebugUtilsLabelEXT *pLabelInfo) {
    m_vki->vkCmdInsertDebugUtilsLabelEXT(m_execBuffer, pLabelInfo);
  }
}
