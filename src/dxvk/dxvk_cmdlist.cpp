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
          VkQueue               queue,
          uint64_t              frameId) {
    auto vk = device->vkd();

    VkLatencySubmissionPresentIdNV latencyInfo = { VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV };
    latencyInfo.presentID = frameId;

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

    if (frameId && device->features().nvLowLatency2)
      latencyInfo.pNext = std::exchange(submitInfo.pNext, &latencyInfo);

    VkResult vr = VK_SUCCESS;

    if (!this->isEmpty())
      vr = vk->vkQueueSubmit2(queue, 1, &submitInfo, VK_NULL_HANDLE);

    this->reset();
    return vr;
  }


  void DxvkCommandSubmission::reset() {
    m_semaphoreWaits.clear();
    m_semaphoreSignals.clear();
    m_commandBuffers.clear();
  }


  bool DxvkCommandSubmission::isEmpty() const {
    return m_semaphoreWaits.empty()
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


  VkCommandBuffer DxvkCommandPool::getCommandBuffer(DxvkCmdBuffer type) {
    auto vk = m_device->vkd();

    if (m_nextPrimary == m_primaryBuffers.size()) {
      // Allocate a new command buffer and add it to the list
      VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
      allocInfo.commandPool = m_commandPool;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandBufferCount = 1;

      VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

      if (vk->vkAllocateCommandBuffers(vk->device(), &allocInfo, &commandBuffer))
        throw DxvkError("DxvkCommandPool: Failed to allocate command buffer");

      m_primaryBuffers.push_back(commandBuffer);
    }

    // Take existing command buffer. All command buffers
    // will be in reset state, so we can begin it safely.
    VkCommandBuffer commandBuffer = m_primaryBuffers[m_nextPrimary++];

    VkCommandBufferBeginInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vk->vkBeginCommandBuffer(commandBuffer, &info))
      throw DxvkError("DxvkCommandPool: Failed to begin command buffer");

    if (m_device->isDebugEnabled()) {
      auto vki = m_device->vki();

      VkDebugUtilsLabelEXT label = { };

      switch (type) {
        case DxvkCmdBuffer::ExecBuffer: label = vk::makeLabel(0xdcc0a2, "Graphics commands"); break;
        case DxvkCmdBuffer::InitBuffer: label = vk::makeLabel(0xc0dca2, "Init commands"); break;
        case DxvkCmdBuffer::InitBarriers: label = vk::makeLabel(0xd0e6b8, "Init barriers"); break;
        case DxvkCmdBuffer::SdmaBuffer: label = vk::makeLabel(0xc0a2dc, "Upload commands"); break;
        case DxvkCmdBuffer::SdmaBarriers: label = vk::makeLabel(0xd0b8e6, "Upload barriers"); break;
        default: ;
      }

      if (label.pLabelName)
        vki->vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);
    }

    return commandBuffer;
  }


  VkCommandBuffer DxvkCommandPool::getSecondaryCommandBuffer(
    const VkCommandBufferInheritanceInfo& inheritanceInfo) {
    auto vk = m_device->vkd();

    if (m_nextSecondary == m_secondaryBuffers.size()) {
      // Allocate a new command buffer and add it to the list
      VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
      allocInfo.commandPool = m_commandPool;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
      allocInfo.commandBufferCount = 1;

      VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

      if (vk->vkAllocateCommandBuffers(vk->device(), &allocInfo, &commandBuffer))
        throw DxvkError("DxvkCommandPool: Failed to allocate secondary command buffer");

      m_secondaryBuffers.push_back(commandBuffer);
    }

    // Assume that the secondary command buffer contains only rendering commands
    VkCommandBuffer commandBuffer = m_secondaryBuffers[m_nextSecondary++];

    VkCommandBufferBeginInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
               | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    info.pInheritanceInfo = &inheritanceInfo;

    if (vk->vkBeginCommandBuffer(commandBuffer, &info))
      throw DxvkError("DxvkCommandPool: Failed to begin secondary command buffer");

    return commandBuffer;
  }


  void DxvkCommandPool::reset() {
    auto vk = m_device->vkd();

    if (m_nextPrimary || m_nextSecondary) {
      if (vk->vkResetCommandPool(vk->device(), m_commandPool, 0))
        throw DxvkError("DxvkCommandPool: Failed to reset command pool");

      m_nextPrimary = 0;
      m_nextSecondary = 0;
    }
  }


  DxvkCommandList::DxvkCommandList(DxvkDevice* device)
  : m_device        (device),
    m_vkd           (device->vkd()),
    m_vki           (device->vki()) {
    const auto& graphicsQueue = m_device->queues().graphics;
    const auto& transferQueue = m_device->queues().transfer;

    m_graphicsPool = new DxvkCommandPool(device, graphicsQueue.queueFamily);

    if (transferQueue.queueFamily != graphicsQueue.queueFamily)
      m_transferPool = new DxvkCommandPool(device, transferQueue.queueFamily);
    else
      m_transferPool = m_graphicsPool;
  }
  
  
  DxvkCommandList::~DxvkCommandList() {
    this->reset();
  }
  
  
  VkResult DxvkCommandList::submit(
    const DxvkTimelineSemaphores&       semaphores,
          DxvkTimelineSemaphoreValues&  timelines,
          uint64_t                      trackedId) {
    VkResult status = VK_SUCCESS;

    static const std::array<DxvkCmdBuffer, 2> SdmaCmdBuffers =
      { DxvkCmdBuffer::SdmaBarriers, DxvkCmdBuffer::SdmaBuffer };
    static const std::array<DxvkCmdBuffer, 2> InitCmdBuffers =
      { DxvkCmdBuffer::InitBarriers, DxvkCmdBuffer::InitBuffer };

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
        for (size_t i = 0; i < m_waitSemaphores.size(); i++) {
          m_commandSubmission.waitSemaphore(m_waitSemaphores[i].fence->handle(),
            m_waitSemaphores[i].value, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
        }
      }

      if (sparseBind) {
        // Sparse binding needs to serialize command execution, so wait
        // for any prior submissions, then block any subsequent ones
        sparseBind->waitSemaphore(semaphores.graphics, timelines.graphics);
        sparseBind->waitSemaphore(semaphores.transfer, timelines.transfer);

        sparseBind->signalSemaphore(semaphores.graphics, ++timelines.graphics);

        if ((status = sparseBind->submit(m_device, sparse.queueHandle)))
          return status;

        m_commandSubmission.waitSemaphore(semaphores.graphics,
          timelines.graphics, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
      }

      // Execute transfer command buffer, if any
      for (auto cmdBuffer : SdmaCmdBuffers) {
        if (cmd.cmdBuffers[uint32_t(cmdBuffer)])
          m_commandSubmission.executeCommandBuffer(cmd.cmdBuffers[uint32_t(cmdBuffer)]);
      }

      // If we had either a transfer command or a semaphore wait, submit to the
      // transfer queue so that all subsequent commands get stalled as necessary.
      if (m_device->hasDedicatedTransferQueue() && !m_commandSubmission.isEmpty()) {
        m_commandSubmission.signalSemaphore(semaphores.transfer,
          ++timelines.transfer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

        if ((status = m_commandSubmission.submit(m_device, transfer.queueHandle, trackedId)))
          return status;

        m_commandSubmission.waitSemaphore(semaphores.transfer,
          timelines.transfer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
      }

      // We promise to never do weird stuff to WSI images on
      // the transfer queue, so blocking graphics is sufficient
      if (isFirst && m_wsiSemaphores.acquire) {
        m_commandSubmission.waitSemaphore(m_wsiSemaphores.acquire,
          0, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
      }

      // Submit initialization commands, if any
      for (auto cmdBuffer : InitCmdBuffers) {
        if (cmd.cmdBuffers[uint32_t(cmdBuffer)])
          m_commandSubmission.executeCommandBuffer(cmd.cmdBuffers[uint32_t(cmdBuffer)]);
      }

      // Only submit the main command buffer if it has actually been used
      if (cmd.execCommands)
        m_commandSubmission.executeCommandBuffer(cmd.cmdBuffers[uint32_t(DxvkCmdBuffer::ExecBuffer)]);

      if (isLast) {
        // Signal per-command list semaphores on the final submission
        for (size_t i = 0; i < m_signalSemaphores.size(); i++) {
          m_commandSubmission.signalSemaphore(m_signalSemaphores[i].fence->handle(),
            m_signalSemaphores[i].value, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
        }

        // Signal WSI semaphore on the final submission
        if (m_wsiSemaphores.present) {
          m_commandSubmission.signalSemaphore(m_wsiSemaphores.present,
            0, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
        }
      }

      m_commandSubmission.signalSemaphore(semaphores.graphics,
        ++timelines.graphics, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

      // Finally, submit all graphics commands of the current submission
      if ((status = m_commandSubmission.submit(m_device, graphics.queueHandle, trackedId)))
        return status;

      // If there are WSI semaphores involved, do another submit only
      // containing a timeline semaphore signal so that we can be sure
      // that they are safe to use afterwards.
      if ((m_wsiSemaphores.present || m_wsiSemaphores.acquire) && isLast) {
        m_commandSubmission.signalSemaphore(semaphores.graphics,
          ++timelines.graphics, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

        if ((status = m_commandSubmission.submit(m_device, graphics.queueHandle, trackedId)))
          return status;
      }

      // Finally, submit semaphore wait on the transfer queue. If this
      // is not the final iteration, fold the wait into the next one.
      if (cmd.syncSdma) {
        m_commandSubmission.waitSemaphore(semaphores.graphics,
          timelines.graphics, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);

        if (isLast && (status = m_commandSubmission.submit(m_device, transfer.queueHandle, trackedId)))
          return status;
      }
    }

    return VK_SUCCESS;
  }
  
  
  void DxvkCommandList::init() {
    // Make sure the main command buffer is initialized since we can
    // reasonably expect that to always get used. Saves some checks
    // during command recording.
    m_cmd = DxvkCommandSubmissionInfo();
    m_cmd.cmdBuffers[uint32_t(DxvkCmdBuffer::ExecBuffer)] = allocateCommandBuffer(DxvkCmdBuffer::ExecBuffer);
  }
  
  
  void DxvkCommandList::finalize() {
    m_cmdSubmissions.push_back(m_cmd);

    // For consistency, end all command buffers here,
    // regardless of whether they have been used.
    for (uint32_t i = 0; i < m_cmd.cmdBuffers.size(); i++) {
      if (m_cmd.cmdBuffers[i])
        endCommandBuffer(m_cmd.cmdBuffers[i]);
    }

    // Reset all command buffer handles
    m_cmd = DxvkCommandSubmissionInfo();

    // Increment queue submission count
    uint64_t submissionCount = m_cmdSubmissions.size();
    m_statCounters.addCtr(DxvkStatCounter::QueueSubmitCount, submissionCount);
  }


  void DxvkCommandList::next() {
    bool push = m_cmd.sparseBind || m_cmd.execCommands;

    for (uint32_t i = 0; i < m_cmd.cmdBuffers.size(); i++) {
      DxvkCmdBuffer cmdBuffer = DxvkCmdBuffer(i);

      if (cmdBuffer == DxvkCmdBuffer::ExecBuffer && !m_cmd.execCommands)
        continue;

      if (m_cmd.cmdBuffers[i]) {
        endCommandBuffer(m_cmd.cmdBuffers[i]);

        m_cmd.cmdBuffers[i] = cmdBuffer == DxvkCmdBuffer::ExecBuffer
          ? allocateCommandBuffer(cmdBuffer)
          : VK_NULL_HANDLE;

        push = true;
      }
    }

    if (!push)
      return;

    m_cmdSubmissions.push_back(m_cmd);

    m_cmd.execCommands = VK_FALSE;
    m_cmd.syncSdma = VK_FALSE;
    m_cmd.sparseBind = VK_FALSE;
  }

  
  void DxvkCommandList::reset() {
    // Free resources and other objects
    // that are no longer in use
    m_objectTracker.clear();

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
  }


  void DxvkCommandList::endCommandBuffer(VkCommandBuffer cmdBuffer) {
    auto vk = m_device->vkd();

    if (m_device->isDebugEnabled())
      m_vki->vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

    if (vk->vkEndCommandBuffer(cmdBuffer))
      throw DxvkError("DxvkCommandList: Failed to end command buffer");
  }


  VkCommandBuffer DxvkCommandList::allocateCommandBuffer(DxvkCmdBuffer type) {
    return type == DxvkCmdBuffer::SdmaBuffer || type == DxvkCmdBuffer::SdmaBarriers
      ? m_transferPool->getCommandBuffer(type)
      : m_graphicsPool->getCommandBuffer(type);
  }

}
