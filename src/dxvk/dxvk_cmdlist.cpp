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


  void DxvkCommandSubmission::signalFence(
          VkFence               fence) {
    m_fence = fence;
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

    if (!this->isEmpty())
      vr = vk->vkQueueSubmit2(queue, 1, &submitInfo, m_fence);

    this->reset();
    return vr;
  }


  void DxvkCommandSubmission::reset() {
    m_fence = VK_NULL_HANDLE;
    m_semaphoreWaits.clear();
    m_semaphoreSignals.clear();
    m_commandBuffers.clear();
  }


  bool DxvkCommandSubmission::isEmpty() const {
    return m_fence == VK_NULL_HANDLE
        && m_semaphoreWaits.empty()
        && m_semaphoreSignals.empty()
        && m_commandBuffers.empty();
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

    if (m_vkd->vkCreateSemaphore(m_vkd->device(), &semaphoreInfo, nullptr, &m_bindSemaphore)
     || m_vkd->vkCreateSemaphore(m_vkd->device(), &semaphoreInfo, nullptr, &m_postSemaphore)
     || m_vkd->vkCreateSemaphore(m_vkd->device(), &semaphoreInfo, nullptr, &m_sdmaSemaphore))
      throw DxvkError("DxvkCommandList: Failed to create semaphore");

    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

    if (m_vkd->vkCreateFence(m_vkd->device(), &fenceInfo, nullptr, &m_fence))
      throw DxvkError("DxvkCommandList: Failed to create fence");

    m_graphicsPool = new DxvkCommandPool(device, graphicsQueue.queueFamily);

    if (transferQueue.queueFamily != graphicsQueue.queueFamily)
      m_transferPool = new DxvkCommandPool(device, transferQueue.queueFamily);
    else
      m_transferPool = m_graphicsPool;
  }
  
  
  DxvkCommandList::~DxvkCommandList() {
    this->reset();

    m_vkd->vkDestroySemaphore(m_vkd->device(), m_bindSemaphore, nullptr);
    m_vkd->vkDestroySemaphore(m_vkd->device(), m_postSemaphore, nullptr);
    m_vkd->vkDestroySemaphore(m_vkd->device(), m_sdmaSemaphore, nullptr);

    m_vkd->vkDestroyFence(m_vkd->device(), m_fence, nullptr);
  }
  
  
  VkResult DxvkCommandList::submit() {
    VkResult status = VK_SUCCESS;

    const auto& graphics = m_device->queues().graphics;
    const auto& transfer = m_device->queues().transfer;
    const auto& sparse = m_device->queues().sparse;

    m_commandSubmission.reset();

    for (size_t i = 0; i < m_cmdSubmissions.size(); i++) {
      bool isFirst = i == 0;
      bool isLast  = i == m_cmdSubmissions.size() - 1;

      const auto& cmd = m_cmdSubmissions[i];

      auto sparseBind = cmd.sparseBind
        ? &m_cmdSparseBinds[cmd.sparseCmd]
        : nullptr;

      if (isFirst) {
        // Wait for per-command list semaphores on first submission
        for (const auto& entry : m_waitSemaphores) {
          m_commandSubmission.waitSemaphore(entry.fence->handle(),
            entry.value, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
        }
      }

      if (sparseBind) {
        // Sparse binding needs to serialize command execution, so wait
        // for any prior submissions, then block any subsequent ones
        m_commandSubmission.signalSemaphore(m_bindSemaphore, 0, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

        if ((status = m_commandSubmission.submit(m_device, graphics.queueHandle)))
          return status;

        sparseBind->waitSemaphore(m_bindSemaphore, 0);
        sparseBind->signalSemaphore(m_postSemaphore, 0);

        if ((status = sparseBind->submit(m_device, sparse.queueHandle)))
          return status;

        m_commandSubmission.waitSemaphore(m_postSemaphore, 0, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
      }

      // Submit transfer commands as necessary
      if (cmd.usedFlags.test(DxvkCmdBuffer::SdmaBuffer))
        m_commandSubmission.executeCommandBuffer(cmd.sdmaBuffer);

      // If we had either a transfer command or a semaphore wait, submit to the
      // transfer queue so that all subsequent commands get stalled as necessary.
      if (m_device->hasDedicatedTransferQueue() && !m_commandSubmission.isEmpty()) {
        m_commandSubmission.signalSemaphore(m_sdmaSemaphore, 0, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

        if ((status = m_commandSubmission.submit(m_device, transfer.queueHandle)))
          return status;

        m_commandSubmission.waitSemaphore(m_sdmaSemaphore, 0, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
      }

      // We promise to never do weird stuff to WSI images on
      // the transfer queue, so blocking graphics is sufficient
      if (isFirst && m_wsiSemaphores.acquire) {
        m_commandSubmission.waitSemaphore(m_wsiSemaphores.acquire,
          0, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
      }

      // Submit graphics commands
      if (cmd.usedFlags.test(DxvkCmdBuffer::InitBuffer))
        m_commandSubmission.executeCommandBuffer(cmd.initBuffer);

      if (cmd.usedFlags.test(DxvkCmdBuffer::ExecBuffer))
        m_commandSubmission.executeCommandBuffer(cmd.execBuffer);

      if (isLast) {
        // Signal per-command list semaphores on the final submission
        for (const auto& entry : m_signalSemaphores) {
          m_commandSubmission.signalSemaphore(entry.fence->handle(),
            entry.value, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
        }

        // Signal WSI semaphore on the final submission
        if (m_wsiSemaphores.present) {
          m_commandSubmission.signalSemaphore(m_wsiSemaphores.present,
            0, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
        }

        // Signal synchronization fence on final submission
        m_commandSubmission.signalFence(m_fence);
      }

      // Finally, submit all graphics commands of the current submission
      if ((status = m_commandSubmission.submit(m_device, graphics.queueHandle)))
        return status;
    }

    return VK_SUCCESS;
  }
  
  
  void DxvkCommandList::init() {
    m_cmd = DxvkCommandSubmissionInfo();

    // Grab a fresh set of command buffers from the pools
    m_cmd.execBuffer = m_graphicsPool->getCommandBuffer();
    m_cmd.initBuffer = m_graphicsPool->getCommandBuffer();
    m_cmd.sdmaBuffer = m_transferPool->getCommandBuffer();
  }
  
  
  void DxvkCommandList::finalize() {
    if (m_cmdSubmissions.empty() || m_cmd.usedFlags != 0)
      m_cmdSubmissions.push_back(m_cmd);

    // For consistency, end all command buffers here,
    // regardless of whether they have been used.
    this->endCommandBuffer(m_cmd.execBuffer);
    this->endCommandBuffer(m_cmd.initBuffer);
    this->endCommandBuffer(m_cmd.sdmaBuffer);

    // Reset all command buffer handles
    m_cmd = DxvkCommandSubmissionInfo();

    // Increment queue submission count
    uint64_t submissionCount = m_cmdSubmissions.size();
    m_statCounters.addCtr(DxvkStatCounter::QueueSubmitCount, submissionCount);
  }


  void DxvkCommandList::next() {
    if (m_cmd.usedFlags != 0 || m_cmd.sparseBind)
      m_cmdSubmissions.push_back(m_cmd);

    // Only replace used command buffer to save resources
    if (m_cmd.usedFlags.test(DxvkCmdBuffer::ExecBuffer)) {
      this->endCommandBuffer(m_cmd.execBuffer);
      m_cmd.execBuffer = m_graphicsPool->getCommandBuffer();
    }

    if (m_cmd.usedFlags.test(DxvkCmdBuffer::InitBuffer)) {
      this->endCommandBuffer(m_cmd.initBuffer);
      m_cmd.initBuffer = m_graphicsPool->getCommandBuffer();
    }

    if (m_cmd.usedFlags.test(DxvkCmdBuffer::SdmaBuffer)) {
      this->endCommandBuffer(m_cmd.sdmaBuffer);
      m_cmd.sdmaBuffer = m_transferPool->getCommandBuffer();
    }

    m_cmd.usedFlags = 0;
  }

  
  VkResult DxvkCommandList::synchronizeFence() {
    return m_vkd->vkWaitForFences(m_vkd->device(), 1, &m_fence, VK_TRUE, ~0ull);
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

    m_cmdSubmissions.clear();
    m_cmdSparseBinds.clear();

    m_wsiSemaphores = PresenterSync();

    // Reset actual command buffers and pools
    m_graphicsPool->reset();
    m_transferPool->reset();

    // Reset fence
    if (m_vkd->vkResetFences(m_vkd->device(), 1, &m_fence))
      Logger::err("DxvkCommandList: Failed to reset fence");
  }


  void DxvkCommandList::endCommandBuffer(VkCommandBuffer cmdBuffer) {
    auto vk = m_device->vkd();

    if (vk->vkEndCommandBuffer(cmdBuffer))
      throw DxvkError("DxvkCommandList: Failed to end command buffer");
  }

}
