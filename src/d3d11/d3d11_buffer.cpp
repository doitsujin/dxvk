#include "d3d11_buffer.h"
#include "d3d11_context.h"
#include "d3d11_device.h"

#include "../dxvk/dxvk_data.h"

namespace dxvk {
  
  D3D11Buffer::D3D11Buffer(
          D3D11Device*                pDevice,
    const D3D11_BUFFER_DESC*          pDesc)
  : m_device  (pDevice),
    m_desc    (*pDesc) {
    
  }
  
  
  D3D11Buffer::~D3D11Buffer() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Buffer::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Resource);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Buffer);
    
    Logger::warn("D3D11Buffer::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11Buffer::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Buffer::GetEvictionPriority() {
    Logger::warn("D3D11Buffer::GetEvictionPriority: Stub");
    return DXGI_RESOURCE_PRIORITY_NORMAL;
  }
  
  
  void STDMETHODCALLTYPE D3D11Buffer::SetEvictionPriority(UINT EvictionPriority) {
    Logger::warn("D3D11Buffer::SetEvictionPriority: Stub");
  }
  
  
  void STDMETHODCALLTYPE D3D11Buffer::GetType(D3D11_RESOURCE_DIMENSION* pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_BUFFER;
  }
  
  
  void STDMETHODCALLTYPE D3D11Buffer::GetDesc(D3D11_BUFFER_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
  
  HRESULT D3D11Buffer::Map(
          D3D11DeviceContext*       pContext,
          D3D11_MAP                 MapType,
          UINT                      MapFlags,
          D3D11_MAPPED_SUBRESOURCE* pMappedSubresource) {
    const Rc<DxvkBuffer> buffer = GetDXVKBuffer();
    
    if (buffer->mapPtr(0) == nullptr) {
      Logger::err("D3D11: Cannot map a device-local buffer");
      return E_FAIL;
    }
    
    if (pMappedSubresource == nullptr)
      return S_OK;
    
    if (!buffer->isInUse()) {
      // Simple case: The buffer is currently not being
      // used by the device, we can return the pointer.
      pMappedSubresource->pData      = buffer->mapPtr(0);
      pMappedSubresource->RowPitch   = buffer->info().size;
      pMappedSubresource->DepthPitch = buffer->info().size;
      return S_OK;
    } else {
      // Don't wait if the application tells us not to
      if (MapFlags & D3D11_MAP_FLAG_DO_NOT_WAIT)
        return DXGI_ERROR_WAS_STILL_DRAWING;
      
      // TODO optimize this. In order to properly cover common use cases
      // like frequent constant buffer updates, we must implement buffer
      // renaming techniques. The current approach is inefficient as it
      // leads to a lot of Flush() and Synchronize() calls.
      // 
      // Possible solution:
      //  (1) Create buffers with a significantly larger size if they
      //      can be mapped by the host for writing. If mapping the
      //      buffer would stall on D3D11_MAP_WRITE_DISCARD, map the
      //      next slice. on D3D11_MAP_WRITE_NO_OVERWRITE, return the
      //      current slice. If the buffer is bound, update bindings.
      //  (2) If no more slices are available, create a new buffer.
      //      Limit the number of buffers to a small, fixed number.
      //  (3) If no more buffers are available, flush and synchronize.
      //  (4) When renaming the buffer internally, all active bindings
      //      need to be updated internally as well.
      // 
      // In order to support deferred contexts, the immediate context
      // must commit all changes to the initial buffer slice prior to
      // executing a command list. When mapping on deferred contexts,
      // the deferred context shall create local buffer objects.
      pContext->Flush();
      pContext->Synchronize();
      
      pMappedSubresource->pData      = buffer->mapPtr(0);
      pMappedSubresource->RowPitch   = buffer->info().size;
      pMappedSubresource->DepthPitch = buffer->info().size;
      return S_OK;
    }
  }
  
  
  void D3D11Buffer::Unmap(
          D3D11DeviceContext*       pContext) {
    // Nothing to see here, folks
  }
  
  
  Rc<DxvkBuffer> D3D11Buffer::GetDXVKBuffer() {
    return m_buffer;
  }
  
  
  Rc<DxvkBuffer> D3D11Buffer::CreateBuffer(
    const D3D11_BUFFER_DESC* pDesc) const {
    // Gather usage information
    DxvkBufferCreateInfo  info;
    info.size   = pDesc->ByteWidth;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT;
    
    if (pDesc->BindFlags & D3D11_BIND_VERTEX_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_INDEX_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_INDEX_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_CONSTANT_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      info.stages |= m_device->GetEnabledShaderStages();
      info.access |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_SHADER_RESOURCE) {
      info.usage  |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                  |  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
      info.stages |= m_device->GetEnabledShaderStages();
      info.access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_STREAM_OUTPUT)
      throw DxvkError("D3D11Device::CreateBuffer: D3D11_BIND_STREAM_OUTPUT not supported");
    
    if (pDesc->BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
      info.usage  |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                  |  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                  |  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      info.access |= VK_ACCESS_SHADER_READ_BIT
                  |  VK_ACCESS_SHADER_WRITE_BIT;
    }
    
    if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_WRITE_BIT;
    }
    
    if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_READ) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_READ_BIT;
    }
    
    if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) {
      info.usage  |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
      info.access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    
    return m_device->GetDXVKDevice()->createBuffer(
      info, GetMemoryFlagsForUsage(pDesc->Usage));
  }
  
}
