
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

    if (m_device->debugFlags().test(DxvkDebugFlag::Capture)) {
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
    // Wait for pending descriptor copies to finish
    m_descriptorSync.synchronize();

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
    // Record commands to upload descriptors if necessary, and
    // reset the descriptor range to not keep it alive for too
    // long. Descriptor ranges are tracked when bound.
    if (m_device->canUseDescriptorHeap() || m_device->canUseDescriptorBuffer()) {
      countDescriptorStats(m_descriptorRange, m_descriptorOffset);

      m_descriptorRange = nullptr;
      m_descriptorHeap = nullptr;
    } else {
      m_descriptorPool->updateStats(m_statCounters);
    }

    // Commit current set of command buffers
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
    // We will re-apply heap bindings first thing in a
    // new command list, so reset this flag here
    m_descriptorHeapInvalidated = false;

    // Free resources and other objects
    // that are no longer in use
    m_objectTracker.clear();

    // Less important stuff
    m_signalTracker.reset();
    m_statCounters.reset();

    // Recycle descriptor pools
    if (m_descriptorPool) {
      m_descriptorPool->notifyCompletion(m_trackingId);
      m_descriptorPool = nullptr;
    }

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


  void DxvkCommandList::bindResources(
          DxvkCmdBuffer                 cmdBuffer,
    const DxvkPipelineLayout*           layout,
          uint32_t                      descriptorCount,
    const DxvkDescriptorWrite*          descriptorInfos,
          size_t                        pushDataSize,
    const void*                         pushData) {
    if (m_device->canUseDescriptorHeap()) {
      bindResourcesDescriptorHeap(cmdBuffer, layout,
        descriptorCount, descriptorInfos, pushDataSize, pushData);
    } else if (m_device->canUseDescriptorBuffer()) {
      bindResourcesDescriptorBuffer(cmdBuffer, layout,
        descriptorCount, descriptorInfos, pushDataSize, pushData);
    } else {
      bindResourcesLegacy(cmdBuffer, layout,
        descriptorCount, descriptorInfos, pushDataSize, pushData);
    }
  }


  void DxvkCommandList::bindResourcesLegacy(
          DxvkCmdBuffer                 cmdBuffer,
    const DxvkPipelineLayout*           layout,
          uint32_t                      descriptorCount,
    const DxvkDescriptorWrite*          descriptorInfos,
          size_t                        pushDataSize,
    const void*                         pushData) {
    // Update descriptor set as necessary
    auto setLayout = layout->getDescriptorSetLayout(0u);

    if (descriptorCount && setLayout && !setLayout->isEmpty()) {
      VkDescriptorSet set = m_descriptorPool->alloc(m_trackingId, setLayout);

      small_vector<DxvkLegacyDescriptor, 16u> descriptors;

      for (uint32_t i = 0u; i < descriptorCount; i++) {
        const auto& info = descriptorInfos[i];
        auto& descriptor = descriptors.emplace_back();

        switch (info.descriptorType) {
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
            if (info.descriptor) {
              descriptor.buffer = info.descriptor->legacy.buffer;
            } else {
              descriptor.buffer.buffer = info.buffer.buffer;
              descriptor.buffer.offset = info.buffer.offset;
              descriptor.buffer.range = info.buffer.size;

              if (!descriptor.buffer.buffer)
                descriptor.buffer.range = VK_WHOLE_SIZE;
            }
          } break;

          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
            if (info.descriptor)
              descriptor.bufferView = info.descriptor->legacy.bufferView;
          } break;

          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
            if (info.descriptor)
              descriptor.image = info.descriptor->legacy.image;
          } break;

          default:
            Logger::err(str::format("Unhandled descriptor type ", info.descriptorType));
        }
      }

      this->updateDescriptorSetWithTemplate(set,
        setLayout->getSetUpdateTemplate(),
        descriptors.data());

      // Bind set as well as the global sampler heap, if requested
      small_vector<VkDescriptorSet, 2u> sets;

      if (layout->usesSamplerHeap())
        sets.push_back(m_device->getSamplerDescriptorSet().set);

      sets.push_back(set);

      this->cmdBindDescriptorSets(cmdBuffer,
        layout->getBindPoint(),
        layout->getPipelineLayout(),
        0u, sets.size(), sets.data());
    }

    // Update push constants
    DxvkPushDataBlock pushDataBlock = layout->getPushData();

    if (pushDataSize && !pushDataBlock.isEmpty()) {
      std::array<char, MaxTotalPushDataSize> dataCopy;
      std::memcpy(dataCopy.data(), pushData,
        std::min(dataCopy.size(), pushDataSize));

      this->cmdPushConstants(cmdBuffer,
        layout->getPipelineLayout(),
        pushDataBlock.getStageMask(),
        pushDataBlock.getOffset(),
        pushDataBlock.getSize(),
        dataCopy.data());
    }
  }


  void DxvkCommandList::bindResourcesDescriptorHeap(
          DxvkCmdBuffer                 cmdBuffer,
    const DxvkPipelineLayout*           layout,
          uint32_t                      descriptorCount,
    const DxvkDescriptorWrite*          descriptorInfos,
          size_t                        pushDataSize,
    const void*                         pushData) {
    auto setLayout = layout->getDescriptorSetLayout(0u);

    // Whether heaps are valid is command list state, not context state,
    // to facilitate interactions with external rendering
    this->ensureDescriptorHeapBinding();

    // For built-in pipelines, the push data layout will have shader-defined
    // consants first, then a byte offset to the descriptor set, in contrast
    // to regular pipelines.
    DxvkPushDataBlock pushDataBlock = layout->getPushData();

    if (pushDataSize && !pushDataBlock.isEmpty()) {
      VkPushDataInfoEXT pushInfo = { VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT };
      pushInfo.offset = 0u;
      pushInfo.data.address = pushData;
      pushInfo.data.size = pushDataSize;

      this->cmdPushData(cmdBuffer, &pushInfo);
    }

    if (descriptorCount && setLayout && !setLayout->isEmpty()) {
      auto vk = m_device->vkd();

      // Assume that a descriptor heap is already active and that
      // we're not recording into a secondary command buffer.
      if (!canAllocateDescriptors(layout))
        createDescriptorRange();

      // Need to pre-allocate arrays with a fixed size so pointers remain valid
      small_vector<DxvkDescriptor, 8u> buffers(descriptorCount);
      small_vector<VkHostAddressRangeEXT, 8u> hostRanges(descriptorCount);
      small_vector<VkDeviceAddressRangeEXT, 8u> bufferRanges(descriptorCount);
      small_vector<VkResourceDescriptorInfoEXT, 8u> writes(descriptorCount);

      // Populate descriptor arrays with necessary information
      small_vector<const DxvkDescriptor*, 8u> descriptors;
      descriptors.reserve(descriptorCount);

      uint32_t writeCount = 0u;

      for (uint32_t i = 0u; i < descriptorCount; i++) {
        const auto& info = descriptorInfos[i];

        switch (info.descriptorType) {
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
            auto& descriptor = buffers[writeCount];
            descriptors.push_back(&descriptor);

            auto& hostRange = hostRanges[writeCount];
            hostRange = descriptor.getHostAddressRange();

            auto& bufferRange = bufferRanges[writeCount];
            bufferRange.address = info.buffer.gpuAddress;
            bufferRange.size = info.buffer.size;

            auto& write = writes[writeCount];
            write.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
            write.type = info.descriptorType;
            write.data.pAddressRange = &bufferRange;

            writeCount += 1u;
          } break;

          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
            auto descriptor = info.descriptor;

            if (!descriptor)
              descriptor = m_device->getDescriptorProperties().getNullDescriptor(info.descriptorType);

            descriptors.push_back(descriptor);
          } break;

          default:
            Logger::err(str::format("Unhandled descriptor type ", info.descriptorType));
        }
      }

      // Write out buffer descriptors
      if (writeCount) {
        vk->vkWriteResourceDescriptorsEXT(vk->device(),
          writeCount, writes.data(), hostRanges.data());
      }

      // Allocate descriptor storage and update the set
      auto setLayout = layout->getDescriptorSetLayout(0u);
      auto storage = allocateDescriptors(setLayout);

      setLayout->update(storage.mapPtr, descriptors.data());

      // Bind the set by updating the appropriate push constant
      uint32_t setOffset = storage.offset >> layout->getDescriptorOffsetShift();

      VkPushDataInfoEXT pushInfo = { VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT };
      pushInfo.offset = pushDataBlock.getSize();
      pushInfo.data.address = &setOffset;
      pushInfo.data.size = sizeof(setOffset);

      this->cmdPushData(cmdBuffer, &pushInfo);
    }
  }


  void DxvkCommandList::bindResourcesDescriptorBuffer(
          DxvkCmdBuffer                 cmdBuffer,
    const DxvkPipelineLayout*           layout,
          uint32_t                      descriptorCount,
    const DxvkDescriptorWrite*          descriptorInfos,
          size_t                        pushDataSize,
    const void*                         pushData) {

    auto setLayout = layout->getDescriptorSetLayout(0u);

    if (descriptorCount && setLayout && !setLayout->isEmpty()) {
      auto vk = m_device->vkd();

      // Assume that a descriptor heap is already active and that
      // we're not recording into a secondary command buffer.
      if (!canAllocateDescriptors(layout))
        createDescriptorRange();

      // Populate descriptor arrays with necessary information
      small_vector<const DxvkDescriptor*, 8u> descriptors;
      descriptors.reserve(descriptorCount);

      small_vector<DxvkDescriptor, 8u> buffers;
      buffers.reserve(descriptorCount);

      for (uint32_t i = 0u; i < descriptorCount; i++) {
        const auto& info = descriptorInfos[i];

        switch (info.descriptorType) {
          case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
            auto& descriptor = buffers.emplace_back();

            VkDescriptorAddressInfoEXT bufferInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
            bufferInfo.address = info.buffer.gpuAddress;
            bufferInfo.range = info.buffer.size;

            VkDescriptorGetInfoEXT descriptorInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
            descriptorInfo.type = info.descriptorType;

            if (info.buffer.size) {
              (info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                ? descriptorInfo.data.pStorageBuffer
                : descriptorInfo.data.pUniformBuffer) = &bufferInfo;
            }

            vk->vkGetDescriptorEXT(vk->device(), &descriptorInfo,
              m_device->getDescriptorProperties().getDescriptorTypeInfo(info.descriptorType).size,
              descriptor.descriptor.data());

            descriptors.push_back(&descriptor);
          } break;

          case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
          case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
            auto descriptor = info.descriptor;

            if (!descriptor)
              descriptor = m_device->getDescriptorProperties().getNullDescriptor(info.descriptorType);

            descriptors.push_back(descriptor);
          } break;

          default:
            Logger::err(str::format("Unhandled descriptor type ", info.descriptorType));
        }
      }

      // Allocate descriptor storage and update the set
      auto setLayout = layout->getDescriptorSetLayout(0u);
      auto storage = allocateDescriptors(setLayout);

      setLayout->update(storage.mapPtr, descriptors.data());

      // Bind actual descriptors
      std::array<uint32_t,     2u> bufferIndices = { };
      std::array<VkDeviceSize, 2u> bufferOffsets = { };

      uint32_t setCount = 0u;

      if (layout->usesSamplerHeap()) {
        bufferIndices[setCount] = 0u;
        bufferOffsets[setCount] = 0u;
        setCount++;
      }

      bufferIndices[setCount] = 1u;
      bufferOffsets[setCount] = storage.offset;
      setCount++;

      cmdSetDescriptorBufferOffsetsEXT(cmdBuffer,
        layout->getBindPoint(),
        layout->getPipelineLayout(),
        0u, setCount,
        bufferIndices.data(),
        bufferOffsets.data());
    }

    // Update push constants
    DxvkPushDataBlock pushDataBlock = layout->getPushData();

    if (pushDataSize && !pushDataBlock.isEmpty()) {
      std::array<char, MaxTotalPushDataSize> dataCopy;
      std::memcpy(dataCopy.data(), pushData,
        std::min(dataCopy.size(), pushDataSize));

      this->cmdPushConstants(cmdBuffer,
        layout->getPipelineLayout(),
        pushDataBlock.getStageMask(),
        pushDataBlock.getOffset(),
        pushDataBlock.getSize(),
        dataCopy.data());
    }
  }


  bool DxvkCommandList::createDescriptorRange() {
    countDescriptorStats(m_descriptorRange, m_descriptorOffset);

    auto oldBaseAddress = m_descriptorRange
      ? m_descriptorRange->getHeapInfo().gpuAddress
      : 0u;

    m_descriptorRange = m_descriptorHeap->allocRange();
    auto newBaseAddress = m_descriptorRange->getHeapInfo().gpuAddress;

    if (newBaseAddress != oldBaseAddress) {
      if (m_execBuffer) {
        m_descriptorRange = nullptr;
        return false;
      }

      if (m_device->canUseDescriptorHeap())
        rebindResourceHeap();
      else if (m_device->canUseDescriptorBuffer())
        rebindDescriptorBuffers();
    }

    m_descriptorOffset = m_descriptorRange->getAllocationOffset();

    track(m_descriptorRange);
    return true;
  }


  void DxvkCommandList::beginSecondaryCommandBuffer(
          VkCommandBufferInheritanceInfo inheritanceInfo) {
    VkCommandBufferInheritanceDescriptorHeapInfoEXT heapInheritance = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_DESCRIPTOR_HEAP_INFO_EXT };

    VkBindHeapInfoEXT samplerHeap = { };
    VkBindHeapInfoEXT resourceHeap = { };

    if (m_device->canUseDescriptorHeap()) {
      samplerHeap = getHeapBindInfo(m_device->getSamplerDescriptorHeap());
      resourceHeap = getHeapBindInfo(m_descriptorRange->getHeapInfo());

      heapInheritance.pNext = std::exchange(inheritanceInfo.pNext, &heapInheritance);
      heapInheritance.pSamplerHeapBindInfo = &samplerHeap;
      heapInheritance.pResourceHeapBindInfo = &resourceHeap;
    }

    VkCommandBuffer secondary = m_graphicsPool->getSecondaryCommandBuffer(inheritanceInfo);

    if (m_device->canUseDescriptorBuffer())
      bindDescriptorBuffers(secondary);

    m_execBuffer = std::exchange(m_cmd.cmdBuffers[uint32_t(DxvkCmdBuffer::ExecBuffer)], secondary);
  }


  VkCommandBuffer DxvkCommandList::endSecondaryCommandBuffer() {
    VkCommandBuffer cmd = getCmdBuffer();

    if (m_vkd->vkEndCommandBuffer(cmd))
      throw DxvkError("DxvkCommandList: Failed to end secondary command buffer");

    m_cmd.cmdBuffers[uint32_t(DxvkCmdBuffer::ExecBuffer)] = m_execBuffer;
    m_execBuffer = VK_NULL_HANDLE;
    return cmd;
  }


  void DxvkCommandList::cmdExecuteCommands(
          uint32_t                count,
          VkCommandBuffer*        commandBuffers) {
    m_cmd.execCommands = true;

    VkCommandBuffer primary = getCmdBuffer();
    m_vkd->vkCmdExecuteCommands(primary, count, commandBuffers);

    if (m_device->canUseDescriptorBuffer())
      bindDescriptorBuffers(primary);
  }


  void DxvkCommandList::setDescriptorHeap(
          Rc<DxvkResourceDescriptorHeap> heap) {
    // External rendering reapplies state, but we
    // really want to avoid that for heap binding
    if (m_descriptorHeap == heap)
      return;

    m_descriptorHeap = std::move(heap);
    m_descriptorRange = m_descriptorHeap->getRange();
    m_descriptorOffset = m_descriptorRange->getAllocationOffset();

    if (m_device->canUseDescriptorHeap())
      rebindResourceHeap();
    else if (m_device->canUseDescriptorBuffer())
      rebindDescriptorBuffers();

    track(m_descriptorRange);
  }


  void DxvkCommandList::rebindSamplerHeap() {
    // Secondary command buffer must not be active when this gets called
    for (uint32_t i = uint32_t(DxvkCmdBuffer::ExecBuffer); i <= uint32_t(DxvkCmdBuffer::InitBarriers); i++)
      bindSamplerHeap(m_cmd.cmdBuffers[i]);
  }


  void DxvkCommandList::rebindResourceHeap() {
    // Secondary command buffer must not be active when this gets called
    for (uint32_t i = uint32_t(DxvkCmdBuffer::ExecBuffer); i <= uint32_t(DxvkCmdBuffer::InitBarriers); i++)
      bindResourceHeap(m_cmd.cmdBuffers[i]);
  }


  void DxvkCommandList::rebindDescriptorBuffers() {
    // Secondary command buffer must not be active when this gets called
    for (uint32_t i = uint32_t(DxvkCmdBuffer::ExecBuffer); i <= uint32_t(DxvkCmdBuffer::InitBuffer); i++)
      bindDescriptorBuffers(m_cmd.cmdBuffers[i]);
  }


  void DxvkCommandList::bindSamplerHeap(VkCommandBuffer cmdBuffer) {
    auto vk = m_device->vkd();

    if (!cmdBuffer)
      return;

    VkBindHeapInfoEXT bindInfo = getHeapBindInfo(m_device->getSamplerDescriptorHeap());
    vk->vkCmdBindSamplerHeapEXT(cmdBuffer, &bindInfo);
  }


  void DxvkCommandList::bindResourceHeap(VkCommandBuffer cmdBuffer) {
    auto vk = m_device->vkd();

    if (!cmdBuffer || !m_descriptorRange)
      return;

    VkBindHeapInfoEXT bindInfo = getHeapBindInfo(m_descriptorRange->getHeapInfo());
    vk->vkCmdBindResourceHeapEXT(cmdBuffer, &bindInfo);
  }


  void DxvkCommandList::bindDescriptorBuffers(VkCommandBuffer cmdBuffer) {
    auto vk = m_device->vkd();

    if (!cmdBuffer || !m_descriptorRange)
      return;

    auto samplerInfo = m_device->getSamplerDescriptorHeap();
    auto resourceInfo = m_descriptorRange->getHeapInfo();

    std::array<VkDescriptorBufferBindingInfoEXT, 2u> heaps = { };

    auto& samplerHeap = heaps[0u];
    samplerHeap.sType = { VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT };
    samplerHeap.address = samplerInfo.gpuAddress;
    samplerHeap.usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;

    auto& resourceHeap = heaps[1u];
    resourceHeap.sType = { VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT };
    resourceHeap.address = resourceInfo.gpuAddress;
    resourceHeap.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    vk->vkCmdBindDescriptorBuffersEXT(cmdBuffer, heaps.size(), heaps.data());
  }


  void DxvkCommandList::endCommandBuffer(VkCommandBuffer cmdBuffer) {
    auto vk = m_device->vkd();

    if (m_device->debugFlags().test(DxvkDebugFlag::Capture))
      m_vki->vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

    if (vk->vkEndCommandBuffer(cmdBuffer))
      throw DxvkError("DxvkCommandList: Failed to end command buffer");
  }


  VkCommandBuffer DxvkCommandList::allocateCommandBuffer(DxvkCmdBuffer type) {
    VkCommandBuffer cmdBuffer = (type >= DxvkCmdBuffer::SdmaBuffer)
      ? m_transferPool->getCommandBuffer(type)
      : m_graphicsPool->getCommandBuffer(type);

    if (type <= DxvkCmdBuffer::InitBarriers && m_device->canUseDescriptorHeap()) {
      bindSamplerHeap(cmdBuffer);
      bindResourceHeap(cmdBuffer);
    }

    if (type <= DxvkCmdBuffer::InitBuffer && m_device->canUseDescriptorBuffer())
      bindDescriptorBuffers(cmdBuffer);

    return cmdBuffer;
  }


  void DxvkCommandList::countDescriptorStats(
    const Rc<DxvkResourceDescriptorRange>& range,
          VkDeviceSize                  baseOffset) {
    if (range) {
      VkDeviceSize dataSize = range->getAllocationOffset() - baseOffset;
      addStatCtr(DxvkStatCounter::DescriptorHeapUsed, dataSize);
    }
  }

}
