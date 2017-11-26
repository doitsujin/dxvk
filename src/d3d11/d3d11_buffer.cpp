#include "d3d11_buffer.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11Buffer::D3D11Buffer(
          D3D11Device*        device,
    const D3D11_BUFFER_DESC&  desc)
  : m_device(device), m_desc(desc),
    m_buffer(this->createBuffer()) {
    
  }
  
  
  D3D11Buffer::~D3D11Buffer() {
    
  }
  
  
  HRESULT D3D11Buffer::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Resource);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Buffer);
    
    Logger::warn("D3D11Buffer::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void D3D11Buffer::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void D3D11Buffer::GetType(D3D11_RESOURCE_DIMENSION* pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_BUFFER;
  }
  
  
  void D3D11Buffer::GetDesc(D3D11_BUFFER_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
  
  Rc<DxvkBuffer> D3D11Buffer::createBuffer() {
    VkMemoryPropertyFlags memoryType = 0;
    
    DxvkBufferCreateInfo info;
    info.size           = m_desc.ByteWidth;
    info.usage          = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                        | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.stages         = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access         = VK_ACCESS_TRANSFER_WRITE_BIT
                        | VK_ACCESS_TRANSFER_READ_BIT;
    
    switch (m_desc.Usage) {
      case D3D11_USAGE_DEFAULT:
      case D3D11_USAGE_IMMUTABLE:
        memoryType |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
      
      case D3D11_USAGE_DYNAMIC:
        memoryType |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                   |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
      
      case D3D11_USAGE_STAGING:
        memoryType |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                   |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                   |  VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    }
    
    if (m_desc.BindFlags & D3D11_BIND_VERTEX_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    
    if (m_desc.BindFlags & D3D11_BIND_INDEX_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_INDEX_READ_BIT;
    }
    
    if (m_desc.BindFlags & (D3D11_BIND_CONSTANT_BUFFER & D3D11_BIND_SHADER_RESOURCE)) {
      info.usage  |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                  |  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                  |  VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
                  |  VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
                  |  VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
                  |  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                  |  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      info.access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (m_desc.BindFlags & D3D11_BIND_STREAM_OUTPUT) {
      info.usage  |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                  |  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
      info.access |= VK_ACCESS_SHADER_WRITE_BIT;
    }
    
    if (m_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
      info.usage  |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                  |  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                  |  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      info.access |= VK_ACCESS_SHADER_READ_BIT
                  |  VK_ACCESS_SHADER_WRITE_BIT;
    }
    
    if (m_desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_WRITE_BIT;
    }
    
    if (m_desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_READ_BIT;
    }
    
    if (m_desc.MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) {
      info.usage  |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
      info.access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    
    return m_device->GetDXVKDevice()->createBuffer(info, memoryType);
  }
  
}
