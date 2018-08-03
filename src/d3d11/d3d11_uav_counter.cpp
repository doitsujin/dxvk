#include "d3d11_device.h"
#include "d3d11_uav_counter.h"

namespace dxvk {

  constexpr VkDeviceSize D3D11UavCounterAllocator::SlicesPerBuffer;


  D3D11UavCounterAllocator::D3D11UavCounterAllocator(D3D11Device* pDevice)
  : m_device    (pDevice),
    m_alignment (GetOffsetAlignment()) {
    
  }
  

  D3D11UavCounterAllocator::~D3D11UavCounterAllocator() {

  }


  DxvkBufferSlice D3D11UavCounterAllocator::AllocSlice() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_freeSlices.size() == 0)
      CreateBuffer(SlicesPerBuffer);
    
    DxvkBufferSlice slice = m_freeSlices.back();
    m_freeSlices.pop_back();
    return slice;
  }


  void D3D11UavCounterAllocator::FreeSlice(const DxvkBufferSlice& Slice) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_freeSlices.push_back(Slice);
  }


  void D3D11UavCounterAllocator::CreateBuffer(VkDeviceSize SliceCount) {
    DxvkBufferCreateInfo info;
    info.size         = SliceCount * m_alignment;
    info.usage        = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                      | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                      | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.stages       = VK_PIPELINE_STAGE_TRANSFER_BIT
                      | m_device->GetEnabledShaderStages();
    info.access       = VK_ACCESS_TRANSFER_READ_BIT
                      | VK_ACCESS_TRANSFER_WRITE_BIT
                      | VK_ACCESS_SHADER_READ_BIT
                      | VK_ACCESS_SHADER_WRITE_BIT;
    
    Rc<DxvkBuffer> buffer = m_device->GetDXVKDevice()->createBuffer(
      info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    for (uint32_t i = 0; i < SliceCount; i++) {
      m_freeSlices.push_back(DxvkBufferSlice(
        buffer, m_alignment * i, m_alignment));
    }
  }


  VkDeviceSize D3D11UavCounterAllocator::GetOffsetAlignment() const {
    const auto& devInfo = m_device->GetDXVKDevice()->adapter()->deviceProperties();
    return align(sizeof(D3D11UavCounter), devInfo.limits.minStorageBufferOffsetAlignment);
  }

}