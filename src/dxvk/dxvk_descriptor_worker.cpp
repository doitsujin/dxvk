#include "dxvk_descriptor_worker.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkDescriptorCopyWorker::DxvkDescriptorCopyWorker(const Rc<DxvkDevice>& device)
  : m_device        (device),
    m_vkd           (device->vkd()),
    m_appendFence   (new sync::Fence()),
    m_consumeFence  (new sync::Fence()),
    m_writeBufferDescriptorsFn(getWriteBufferDescriptorFn()) {
    if (m_device->canUseDescriptorHeap() || m_device->canUseDescriptorBuffer())
      m_thread = std::thread([this] { runWorker(); });
  }


  DxvkDescriptorCopyWorker::~DxvkDescriptorCopyWorker() {
    if (m_thread.joinable()) {
      m_consumeFence->wait(m_appendFence->value());
      m_appendFence->signal(-1);
      m_thread.join();
    }
  }


  DxvkDescriptorCopyWorker::Block* DxvkDescriptorCopyWorker::flushBlock() {
    // No need to do anything if the block is empty
    if (!m_blocks[m_blockIndex].rangeCount)
      return &m_blocks[m_blockIndex];

    // Ensure the next block is actually usable
    uint64_t append = m_appendFence->value() + 1u;
    m_appendFence->signal(append);

    if (append >= BlockCount)
      m_consumeFence->wait(append - BlockCount + 1u);

    m_blockIndex = append % BlockCount;
    return &m_blocks[m_blockIndex];
  }


  WriteBufferDescriptorsFn* DxvkDescriptorCopyWorker::getWriteBufferDescriptorFn() const {
    // HACK: Hard-code the RDNA2 raw buffer descriptor format for Steam Deck
    // in order to dodge significant API call overhead on 32-bit winevulkan.
    if (env::is32BitHostPlatform() && m_device->debugFlags().isClear()) {
      const auto& props = m_device->properties();

      bool isDeck = props.vk12.driverID == VK_DRIVER_ID_MESA_RADV
                 && props.core.properties.vendorID == uint16_t(DxvkGpuVendor::Amd)
                 && (props.core.properties.deviceID == 0x163fu
                  || props.core.properties.deviceID == 0x1435u);

      // Validate descriptor sizes to make sure we're *actually* a Deck and
      // not running with any layers that screw around with descriptor data.
      // This still isn't guaranteed to be reliable.
      auto uboSize = m_device->getDescriptorProperties().getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER).size;
      auto ssboSize = m_device->getDescriptorProperties().getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER).size;

      if (isDeck && uboSize == 16u && ssboSize == 16u) {
        Logger::info("Steam Deck detected, using custom buffer descriptors!");
        return &writeBufferDescriptorsSteamDeck;
      }
    }

    if (m_device->canUseDescriptorHeap())
      return &writeBufferDescriptorsGeneric;

    if (m_device->canUseDescriptorBuffer())
      return &writeBufferDescriptorsGetDescriptorExt;

    return nullptr;
  }


  void DxvkDescriptorCopyWorker::processBlock(Block& block) {
    // Local memory for uniform buffers descriptors in each set
    std::array<DxvkDescriptor, MaxNumUniformBufferSlots> scratchDescriptors;

    DxvkDescriptorCopy e = { };
    e.descriptors = block.descriptors.data();
    e.buffers = block.buffers.data();

    for (uint32_t i = 0u; i < block.rangeCount; i++) {
      const auto& range = block.ranges[i];

      m_writeBufferDescriptorsFn(this,
        scratchDescriptors.data(), range.bufferCount, e.buffers);

      for (uint32_t j = 0u; j < range.bufferCount; j++)
        e.descriptors[e.buffers[j].indexInSet] = &scratchDescriptors[j];

      range.layout->update(range.descriptorMemory, e.descriptors);

      e.descriptors += range.descriptorCount;
      e.buffers += range.bufferCount;
    }

    // Reset entire block to avoid stale descriptors if
    // anything goes wrong; may improve debuggability.
    block = Block();
  }


  void DxvkDescriptorCopyWorker::runWorker() {
    env::setThreadName("dxvk-descriptor");

    uint64_t consume = 0u;

    while (true) {
      m_appendFence->wait(consume + 1u);

      // Explicitly check current append counter value
      // since that's how we stop the worker thread
      uint64_t append = m_appendFence->value();

      if (append == uint64_t(-1))
        return;

      // Process all blocks that have been queued up
      auto t0 = dxvk::high_resolution_clock::now();

      while (consume < append) {
        processBlock(m_blocks[consume % BlockCount]);
        m_consumeFence->signal(++consume);
      }

      // Update stat counters
      auto t1 = dxvk::high_resolution_clock::now();
      auto td = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);

      m_device->addStatCtr(DxvkStatCounter::DescriptorCopyBusyTicks, td.count());
    }
  }


  void DxvkDescriptorCopyWorker::writeBufferDescriptorsGeneric(
    const DxvkDescriptorCopyWorker* worker,
          DxvkDescriptor*           descriptors,
          uint32_t                  bufferCount,
    const DxvkDescriptorCopyBuffer* bufferInfos) {
    // Batch API calls to avoid overhead, especially on 32-bit
    constexpr size_t MaxWrites = 32u;

    small_vector<VkHostAddressRangeEXT, MaxWrites> hostRanges;
    small_vector<VkDeviceAddressRangeEXT, MaxWrites> bufferRanges;
    small_vector<VkResourceDescriptorInfoEXT, MaxWrites> writes;

    for (uint32_t i = 0u; i < bufferCount; i++) {
      auto& buffer = bufferInfos[i];

      hostRanges.push_back(descriptors[i].getHostAddressRange());

      auto& bufferRange = bufferRanges.emplace_back();
      bufferRange.address = buffer.gpuAddress;
      bufferRange.size = buffer.size;

      auto& write = writes.emplace_back();
      write.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
      write.type = VkDescriptorType(buffer.descriptorType);

      if (buffer.size)
        write.data.pAddressRange = &bufferRange;

      if (writes.size() == MaxWrites || i + 1u == bufferCount) {
        worker->m_vkd->vkWriteResourceDescriptorsEXT(worker->m_vkd->device(),
          writes.size(), writes.data(), hostRanges.data());

        hostRanges.clear();
        bufferRanges.clear();
        writes.clear();
      }
    }
  }


  void DxvkDescriptorCopyWorker::writeBufferDescriptorsGetDescriptorExt(
    const DxvkDescriptorCopyWorker* worker,
          DxvkDescriptor*           descriptors,
          uint32_t                  bufferCount,
    const DxvkDescriptorCopyBuffer* bufferInfos) {
    for (uint32_t i = 0u; i < bufferCount; i++) {
      auto& descriptor = descriptors[i];
      auto& buffer = bufferInfos[i];

      VkDescriptorAddressInfoEXT bufferInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
      bufferInfo.address = buffer.gpuAddress;
      bufferInfo.range = buffer.size;

      VkDescriptorGetInfoEXT descriptorInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
      descriptorInfo.type = VkDescriptorType(buffer.descriptorType);

      if (bufferInfo.range)
        descriptorInfo.data.pUniformBuffer = &bufferInfo;

      VkDeviceSize descriptorSize = worker->m_device->getDescriptorProperties().getDescriptorTypeInfo(descriptorInfo.type).size;

      worker->m_vkd->vkGetDescriptorEXT(worker->m_vkd->device(),
        &descriptorInfo, descriptorSize, descriptor.descriptor.data());
    }
  }


  void DxvkDescriptorCopyWorker::writeBufferDescriptorsSteamDeck(
    const DxvkDescriptorCopyWorker* worker,
          DxvkDescriptor*           descriptors,
          uint32_t                  bufferCount,
    const DxvkDescriptorCopyBuffer* bufferInfos) {
    for (uint32_t i = 0u; i < bufferCount; i++) {
      auto& descriptor = descriptors[i];
      auto& buffer = bufferInfos[i];

      std::array<uint32_t, 4u> rawDescriptor = { };

      if (buffer.size) {
        rawDescriptor[0u] = uint32_t(buffer.gpuAddress);
        rawDescriptor[1u] = uint32_t(buffer.gpuAddress >> 32u) & 0xffffu;
        rawDescriptor[2u] = buffer.size;
        rawDescriptor[3u] = 0x31016facu; /* don't ask */
      }

      std::memcpy(descriptor.descriptor.data(), rawDescriptor.data(), sizeof(rawDescriptor));
    }
  }

}
