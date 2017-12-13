#include "d3d11_buffer.h"
#include "d3d11_context.h"
#include "d3d11_device.h"

#include "../dxvk/dxvk_data.h"

namespace dxvk {
  
  D3D11Buffer::D3D11Buffer(
          D3D11Device*                device,
          IDXGIBufferResourcePrivate* resource,
    const D3D11_BUFFER_DESC&          desc)
  : m_device  (device),
    m_resource(resource),
    m_desc    (desc) {
    
  }
  
  
  D3D11Buffer::~D3D11Buffer() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Buffer::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Resource);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Buffer);
    
    if (riid == __uuidof(IDXGIResource)
     || riid == __uuidof(IDXGIBufferResourcePrivate))
      return m_resource->QueryInterface(riid, ppvObject);
      
    Logger::warn("D3D11Buffer::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11Buffer::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Buffer::GetEvictionPriority() {
    UINT EvictionPriority = DXGI_RESOURCE_PRIORITY_NORMAL;
    m_resource->GetEvictionPriority(&EvictionPriority);
    return EvictionPriority;
  }
  
  
  void STDMETHODCALLTYPE D3D11Buffer::SetEvictionPriority(UINT EvictionPriority) {
    m_resource->SetEvictionPriority(EvictionPriority);
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
      
      if (MapType == D3D11_MAP_WRITE_DISCARD) {
        // Instead of synchronizing with the device, which is
        // highly inefficient, return a host-local buffer to
        // the application and upload its contents on unmap()
        // TODO evaluate whether this improves performance
        m_mapData = new DxvkDataBuffer(buffer->info().size);
        
        pMappedSubresource->pData      = m_mapData->data();
        pMappedSubresource->RowPitch   = buffer->info().size;
        pMappedSubresource->DepthPitch = buffer->info().size;
        return S_OK;
      } else {
        // We have to wait for the device to complete
        pContext->Flush();
        pContext->Synchronize();
        
        pMappedSubresource->pData      = buffer->mapPtr(0);
        pMappedSubresource->RowPitch   = buffer->info().size;
        pMappedSubresource->DepthPitch = buffer->info().size;
        return S_OK;
      }
    }
  }
  
  
  void D3D11Buffer::Unmap(
          D3D11DeviceContext*       pContext) {
    if (m_mapData != nullptr) {
      const Rc<DxvkContext> context
        = pContext->GetDXVKContext();
        
      context->updateBuffer(
        m_resource->GetDXVKBuffer(),
        0, m_mapData->size(),
        m_mapData->data());
      
      m_mapData = nullptr;
    }
  }
  
  
  Rc<DxvkBuffer> D3D11Buffer::GetDXVKBuffer() {
    return m_resource->GetDXVKBuffer();
  }
  
}
