#include "dxvk_descriptor_worker.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkDescriptorCopyWorker::DxvkDescriptorCopyWorker(const Rc<DxvkDevice>& device)
  : m_device        (device),
    m_appendFence   (new sync::Fence()),
    m_consumeFence  (new sync::Fence()),
    m_writeBufferDescriptorsFn(getWriteBufferDescriptorFn()) {
    if (m_device->canUseDescriptorBuffer())
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
    if (!m_device->canUseDescriptorBuffer())
      return nullptr;

    return &writeBufferDescriptorsGetDescriptorExt;
  }


  void DxvkDescriptorCopyWorker::processBlock(Block& block) {
    auto vk = m_device->vkd();

    // Local memory for uniform buffers descriptors in each set
    std::array<DxvkDescriptor, MaxNumUniformBufferSlots> scratchDescriptors;

    DxvkDescriptorCopy e = { };
    e.descriptors = block.descriptors.data();
    e.buffers = block.buffers.data();

    for (uint32_t i = 0u; i < block.rangeCount; i++) {
      const auto& range = block.ranges[i];

      m_writeBufferDescriptorsFn(m_device.ptr(),
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

      while (consume < append) {
        processBlock(m_blocks[consume % BlockCount]);
        m_consumeFence->signal(++consume);
      }
    }
  }


  void DxvkDescriptorCopyWorker::writeBufferDescriptorsGetDescriptorExt(
          DxvkDevice*               device,
          DxvkDescriptor*           descriptors,
          uint32_t                  bufferCount,
    const DxvkDescriptorCopyBuffer* bufferInfos) {
    auto vk = device->vkd();

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

      VkDeviceSize descriptorSize = device->getDescriptorProperties().getDescriptorTypeInfo(descriptorInfo.type).size;

      vk->vkGetDescriptorEXT(vk->device(), &descriptorInfo,
        descriptorSize, descriptor.descriptor.data());
    }
  }

}
