#include "dxvk_descriptor_worker.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkDescriptorCopyWorker::DxvkDescriptorCopyWorker(const Rc<DxvkDevice>& device)
  : m_device        (device),
    m_appendFence   (new sync::Fence()),
    m_consumeFence  (new sync::Fence()),
    m_thread        ([this] { runWorker(); }) {

  }


  DxvkDescriptorCopyWorker::~DxvkDescriptorCopyWorker() {
    m_consumeFence->wait(m_appendFence->value());
    m_appendFence->signal(-1);
    m_thread.join();
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


  void DxvkDescriptorCopyWorker::processBlock(Block& block) {
    auto vk = m_device->vkd();

    // Local memory for uniform buffers descriptors in each set
    std::array<DxvkDescriptor, MaxNumUniformBufferSlots> scratchDescriptors;

    DxvkDescriptorCopy e = { };
    e.descriptors = block.descriptors.data();
    e.buffers = block.buffers.data();

    for (uint32_t i = 0u; i < block.rangeCount; i++) {
      const auto& range = block.ranges[i];

      for (uint32_t j = 0u; j < range.bufferCount; j++) {
        auto& descriptor = scratchDescriptors[j];

        VkDescriptorAddressInfoEXT bufferInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
        bufferInfo.address = e.buffers[j].gpuAddress;
        bufferInfo.range = e.buffers[j].size;

        VkDescriptorGetInfoEXT descriptorInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
        descriptorInfo.type = VkDescriptorType(e.buffers[j].descriptorType);

        if (bufferInfo.range)
          descriptorInfo.data.pUniformBuffer = &bufferInfo;

        VkDeviceSize descriptorSize = m_device->getDescriptorProperties().getDescriptorTypeInfo(descriptorInfo.type).size;

        vk->vkGetDescriptorEXT(vk->device(), &descriptorInfo,
          descriptorSize, descriptor.descriptor.data());

        e.descriptors[e.buffers[j].indexInSet] = &descriptor;
      }

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



}
