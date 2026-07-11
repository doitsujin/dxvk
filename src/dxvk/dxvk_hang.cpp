#include <iomanip>

#include "dxvk_hang.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkCheckpointBuffer::DxvkCheckpointBuffer(DxvkDevice* device)
  : m_device(device) {
    if (m_device->debugFlags().test(DxvkDebugFlag::Hang))
      m_checkpoints.resize(NumCheckpoints);
  }


  DxvkCheckpointBuffer::~DxvkCheckpointBuffer() {

  }


  int32_t DxvkCheckpointBuffer::addCheckpoint(
    const DxvkDeviceQueue&      queue,
          VkCommandBuffer       cmd,
          int32_t               prev,
    const char*                 text) {
    std::lock_guard lock(m_mutex);

    int32_t index = getNextCheckpoint();

    // Assume that entry is default-initialized
    auto& entry = m_checkpoints.at(index);

    if (prev >= 0) {
      m_checkpoints.at(prev).next = index;
      entry.prev = prev;
    }

    size_t len = std::min(std::strlen(text), entry.info.size() - 1u);
    std::memcpy(entry.info.data(), text, len);

    auto vk = m_device->vkd();

    if (m_device->features().nvDeviceDiagnosticCheckpoints) {
      // Need to record the 'end' marker for the previous command.
      uintptr_t marker = uintptr_t(prev);
      vk->vkCmdSetCheckpointNV(cmd, reinterpret_cast<void*>(marker));
    } else if (m_device->features().amdBufferMarker) {
      // Record both the 'end' marker for the previous command as
      // well as the 'start' marker for the new one on the AMD path.
      auto prevMarkers = getMarkerBuffer(queue, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
      auto nextMarkers = getMarkerBuffer(queue, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

      if (prev >= 0) {
        vk->vkCmdWriteBufferMarkerAMD(cmd,
          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          prevMarkers.buffer, prevMarkers.offset,
          uint32_t(prev));
      }

      vk->vkCmdWriteBufferMarkerAMD(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        nextMarkers.buffer, nextMarkers.offset,
        uint32_t(index));
    }

    return index;
  }


  void DxvkCheckpointBuffer::endCommandBuffer(
    const DxvkDeviceQueue&      queue,
          VkCommandBuffer       cmd,
          int32_t               prev) {
    std::lock_guard lock(m_mutex);

    if (prev < 0)
      return;

    auto vk = m_device->vkd();

    if (m_device->features().nvDeviceDiagnosticCheckpoints) {
      // Need to add unique queues, even if we don't have a
      // reason to use the result here.
      getQueueIndex(queue);

      uintptr_t marker = uintptr_t(prev);
      vk->vkCmdSetCheckpointNV(cmd, reinterpret_cast<void*>(marker));
    } else if (m_device->features().amdBufferMarker) {
      // Record last end marker into command buffer
      auto prevMarkers = getMarkerBuffer(queue, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

      vk->vkCmdWriteBufferMarkerAMD(cmd,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        prevMarkers.buffer, prevMarkers.offset,
        uint32_t(prev));
    }
  }


  void DxvkCheckpointBuffer::printHangInfo() {
    std::lock_guard lock(m_mutex);

    if (std::exchange(m_hung, true))
      return;

    Logger::err("DXVK: Hang detected:");

    if (m_device->features().nvDeviceDiagnosticCheckpoints) {
      auto vk = m_device->vkd();

      for (const auto& q : m_markerQueues) {
        uint32_t checkpointCount = 0u;
        vk->vkGetQueueCheckpointDataNV(q.queueHandle, &checkpointCount, nullptr);

        std::vector<VkCheckpointDataNV> checkpointData(checkpointCount,
          VkCheckpointDataNV { VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV });
        vk->vkGetQueueCheckpointDataNV(q.queueHandle, &checkpointCount, checkpointData.data());

        int32_t hangTop = -1;
        int32_t hangBottom = -1;

        for (const auto& cp : checkpointData) {
          if (cp.stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
            hangTop = int32_t(reinterpret_cast<uintptr_t>(cp.pCheckpointMarker));
          if (cp.stage == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)
            hangBottom = int32_t(reinterpret_cast<uintptr_t>(cp.pCheckpointMarker));
        }

        if (hangTop != hangBottom)
          logHangCommands(q, hangTop, hangBottom);
      }
    } else if (m_device->features().amdBufferMarker) {
      for (const auto& q : m_markerQueues) {
        auto* hangTop = reinterpret_cast<int32_t*>(getMarkerBuffer(q, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT).mapPtr);
        auto* hangBottom = reinterpret_cast<int32_t*>(getMarkerBuffer(q, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT).mapPtr);

        if (*hangTop != *hangBottom)
          logHangCommands(q, *hangTop, *hangBottom);
      }
    } else {
      Logger::err("Cannot determine hang location.");
    }
  }


  int32_t DxvkCheckpointBuffer::getNextCheckpoint() {
    uint32_t index = m_next;

    // Wrap checkpoint indices around. This is a linked list,
    // a higher index does not necessarily imply later execution.
    if (++m_next == int32_t(NumCheckpoints))
      m_next = 0u;

    // Remove any existing links to other commands
    auto& entry = m_checkpoints.at(index);

    if (entry.prev >= 0)
      m_checkpoints.at(entry.prev).next = -1;
    if (entry.next >= 0)
      m_checkpoints.at(entry.next).prev = -1;

    entry = DxvkCheckpoint();
    return index;
  }


  size_t DxvkCheckpointBuffer::getQueueIndex(
    const DxvkDeviceQueue&        queue) {
    size_t index = m_markerQueues.size();

    for (size_t i = 0u; i < m_markerQueues.size(); i++) {
      if (m_markerQueues[i].queueHandle == queue.queueHandle)
        index = i;
    }

    if (index == m_markerQueues.size())
      m_markerQueues.push_back(queue);

    return index;
  }


  DxvkResourceBufferInfo DxvkCheckpointBuffer::getMarkerBuffer(
    const DxvkDeviceQueue&        queue,
          VkPipelineStageFlagBits stage) {
    if (!m_markerBuffer) {
      if (!(m_markerBuffer = createMarkerBuffer()))
        return DxvkResourceBufferInfo();
    }

    VkDeviceSize offset = sizeof(Marker) * getQueueIndex(queue) +
      (stage == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
        ? offsetof(Marker, bottom)
        : offsetof(Marker, top));

    return m_markerBuffer->getSliceInfo(offset, sizeof(uint32_t));
  }


  Rc<DxvkBuffer> DxvkCheckpointBuffer::createMarkerBuffer() {
    DxvkBufferCreateInfo info = {};
    info.debugName = "Hang markers";
    info.size = 256u;
    info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access = VK_ACCESS_TRANSFER_WRITE_BIT;

    Rc<DxvkBuffer> buffer = m_device->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
      VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
      VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD |
      VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD);

    std::memset(buffer->mapPtr(0), -1, info.size);
    return buffer;
  }


  void DxvkCheckpointBuffer::logHangCommands(
    const DxvkDeviceQueue&        queue,
          int32_t                 lastBegun,
          int32_t                 lastComplete) {
    Logger::err(str::format("Queue family ", queue.queueFamily, ", index ", queue.queueIndex));

    auto listComplete = getCommandList(lastComplete);
    auto listBegun = getCommandList(lastBegun);

    Logger::err(str::format("Last command completed: ", lastComplete));
    Logger::err(str::format("Last command started:   ", lastBegun));

    if (listComplete.first == listBegun.first) {
      logHangCommandSequence(listComplete, lastBegun, lastComplete);
    } else {
      logHangCommandSequence(listComplete, -1, lastComplete);
      Logger::err("...");
      logHangCommandSequence(listBegun, lastBegun, -1);
    }
  }


  void DxvkCheckpointBuffer::logHangCommandSequence(
    const std::pair<int32_t, int32_t>& list,
          int32_t                 lastBegun,
          int32_t                 lastComplete) {
    int32_t index = list.first;

    while (index >= 0) {
      const auto& entry = m_checkpoints.at(index);
      Logger::err(str::format(std::setw(6), index, ": ", entry.info.data()));

      if (index == lastComplete)
        Logger::err("!!! Potential hang region BEGIN !!!");

      if (index == lastBegun)
        Logger::err("!!! Potential hang region END !!!");

      if (index == list.second)
        break;

      index = entry.next;
    }
  }


  std::pair<int32_t, int32_t> DxvkCheckpointBuffer::getCommandList(int32_t command) const {
    if (command == -1)
      return std::make_pair(-1, -1);

    int32_t head = command;
    int32_t tail = command;

    while (m_checkpoints.at(head).prev >= 0)
      head = m_checkpoints.at(head).prev;

    while (m_checkpoints.at(tail).next >= 0)
      tail = m_checkpoints.at(tail).next;

    return std::make_pair(head, tail);
  }

}
