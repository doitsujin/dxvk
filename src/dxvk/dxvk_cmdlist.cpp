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


  DxvkCommandPool::DxvkCommandPool(
          DxvkDevice*           device,
          uint32_t              queueFamily)
  : m_device(device) {
    auto vk = m_device->vkd();

    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.queueFamilyIndex = queueFamily;

    if (vk->vkCreateCommandPool(vk->device(), &poolInfo, nullptr, &m_commandPool))
      throw DxvkError("DxvkCommandPool: Failed to create command pool");
  }


  DxvkCommandPool::~DxvkCommandPool() {
    auto vk = m_device->vkd();

    vk->vkDestroyCommandPool(vk->device(), m_commandPool, nullptr);
  }


  VkCommandBuffer DxvkCommandPool::getCommandBuffer() {
    auto vk = m_device->vkd();

    if (m_next == m_commandBuffers.size()) {
      // Allocate a new command buffer and add it to the list
      VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
      allocInfo.commandPool = m_commandPool;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandBufferCount = 1;

      VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

      if (vk->vkAllocateCommandBuffers(vk->device(), &allocInfo, &commandBuffer))
        throw DxvkError("DxvkCommandPool: Failed to allocate command buffer");

      m_commandBuffers.push_back(commandBuffer);
    }

    // Take existing command buffer. All command buffers
    // will be in reset state, so we can begin it safely.
    VkCommandBuffer commandBuffer = m_commandBuffers[m_next++];

    VkCommandBufferBeginInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vk->vkBeginCommandBuffer(commandBuffer, &info))
      throw DxvkError("DxvkCommandPool: Failed to begin command buffer");

    return commandBuffer;
  }


  void DxvkCommandPool::reset() {
    auto vk = m_device->vkd();

    if (m_next) {
      if (vk->vkResetCommandPool(vk->device(), m_commandPool, 0))
        throw DxvkError("DxvkCommandPool: Failed to reset command pool");

      m_next = 0;
    }
  }


  DxvkCommandList::DxvkCommandList(DxvkDevice* device)
  : m_device        (device),
    m_vkd           (device->vkd()),
    m_vki           (device->instance()->vki()) {
    const auto& graphicsQueue = m_device->queues().graphics;
    const auto& transferQueue = m_device->queues().transfer;

    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    if (m_vkd->vkCreateSemaphore(m_vkd->device(), &semaphoreInfo, nullptr, &m_sdmaSemaphore))
      throw DxvkError("DxvkCommandList: Failed to create semaphore");

    m_graphicsPool = new DxvkCommandPool(device, graphicsQueue.queueFamily);

    if (transferQueue.queueFamily != graphicsQueue.queueFamily)
      m_transferPool = new DxvkCommandPool(device, transferQueue.queueFamily);
    else
      m_transferPool = m_graphicsPool;
  }
  
  
  DxvkCommandList::~DxvkCommandList() {
    this->reset();

    m_vkd->vkDestroySemaphore(m_vkd->device(), m_sdmaSemaphore, nullptr);
  }
  
  
  VkResult DxvkCommandList::submit(
          VkSemaphore       semaphore,
          uint64_t&         semaphoreValue) {
    VkResult status = VK_SUCCESS;

    const auto& graphics = m_device->queues().graphics;
    const auto& transfer = m_device->queues().transfer;

    m_commandSubmission.reset();

    if (m_cmd.usedFlags.test(DxvkCmdBuffer::SdmaBuffer)) {
      m_commandSubmission.executeCommandBuffer(m_cmd.sdmaBuffer);

      if (m_device->hasDedicatedTransferQueue()) {
        m_commandSubmission.signalSemaphore(m_sdmaSemaphore, 0, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

        if ((status = m_commandSubmission.submit(m_device, transfer.queueHandle)))
          return status;

        m_commandSubmission.waitSemaphore(m_sdmaSemaphore, 0, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
      }
    }

    if (m_cmd.usedFlags.test(DxvkCmdBuffer::InitBuffer))
      m_commandSubmission.executeCommandBuffer(m_cmd.initBuffer);

    if (m_cmd.usedFlags.test(DxvkCmdBuffer::ExecBuffer))
      m_commandSubmission.executeCommandBuffer(m_cmd.execBuffer);

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
    m_cmd = DxvkCommandSubmissionInfo();
    m_cmd.execBuffer = m_graphicsPool->getCommandBuffer();
    m_cmd.initBuffer = m_graphicsPool->getCommandBuffer();
    m_cmd.sdmaBuffer = m_transferPool->getCommandBuffer();
  }
  
  
  void DxvkCommandList::endRecording() {
    if (m_vkd->vkEndCommandBuffer(m_cmd.execBuffer) != VK_SUCCESS
     || m_vkd->vkEndCommandBuffer(m_cmd.initBuffer) != VK_SUCCESS
     || m_vkd->vkEndCommandBuffer(m_cmd.sdmaBuffer) != VK_SUCCESS)
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

    // Reset actual command buffers and pools
    m_graphicsPool->reset();
    m_transferPool->reset();
  }

}
