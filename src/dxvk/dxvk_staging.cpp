#include "dxvk_device.h"
#include "dxvk_staging.h"

namespace dxvk {
  
  DxvkStagingBuffer::DxvkStagingBuffer(
    const Rc<DxvkDevice>&     device,
          VkDeviceSize        size)
  : m_device(device), m_offset(0), m_size(size) {

  }


  DxvkStagingBuffer::~DxvkStagingBuffer() {

  }


  DxvkBufferSlice DxvkStagingBuffer::alloc(VkDeviceSize align, VkDeviceSize size) {
    DxvkBufferCreateInfo info;
    info.size   = size;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
                | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_SHADER_READ_BIT;

    VkDeviceSize alignedSize = dxvk::align(size, align);
    VkDeviceSize alignedOffset = dxvk::align(m_offset, align);

    if (2 * alignedSize > m_size) {
      return DxvkBufferSlice(m_device->createBuffer(info,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    }

    if (alignedOffset + alignedSize > m_size || m_buffer == nullptr) {
      info.size = m_size;

      // Free resources first if possible, in some rare
      // situations this may help avoid a memory allocation.
      m_buffer = nullptr;
      m_buffer = m_device->createBuffer(info,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      alignedOffset = 0;
    }

    DxvkBufferSlice slice(m_buffer, alignedOffset, size);
    m_offset = alignedOffset + alignedSize;
    return slice;
  }


  void DxvkStagingBuffer::reset() {
    m_buffer = nullptr;
    m_offset = 0;
  }
  
}
