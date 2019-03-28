#include "d3d11_counter_buffer.h"
#include "d3d11_device.h"

namespace dxvk {

  D3D11CounterBuffer::D3D11CounterBuffer(
    const Rc<DxvkDevice>&       Device,
    const DxvkBufferCreateInfo& BufferInfo,
          VkDeviceSize          SliceLength)
  : m_device      (Device),
    m_bufferInfo  (BufferInfo),
    m_sliceLength (SliceLength) {
    
  }
  

  D3D11CounterBuffer::~D3D11CounterBuffer() {

  }


  DxvkBufferSlice D3D11CounterBuffer::AllocSlice() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_freeSlices.size() == 0)
      CreateBuffer();
    
    DxvkBufferSlice slice = m_freeSlices.back();
    m_freeSlices.pop_back();
    return slice;
  }


  void D3D11CounterBuffer::FreeSlice(const DxvkBufferSlice& Slice) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_freeSlices.push_back(Slice);
  }


  void D3D11CounterBuffer::CreateBuffer() {
    VkMemoryPropertyFlags memoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // Workaround for predicate buffer sync issues on RADV
    if ((m_bufferInfo.usage & VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT)
     && (m_device->adapter()->matchesDriver(DxvkGpuVendor::Amd, VK_DRIVER_ID_MESA_RADV_KHR, 0, 0))) {
      memoryType |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                 |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    Rc<DxvkBuffer> buffer = m_device->createBuffer(m_bufferInfo, memoryType);
    
    VkDeviceSize sliceCount = m_bufferInfo.size / m_sliceLength;
    
    for (uint32_t i = 0; i < sliceCount; i++) {
      m_freeSlices.push_back(DxvkBufferSlice(
        buffer, m_sliceLength * i, m_sliceLength));
    }
  }

}