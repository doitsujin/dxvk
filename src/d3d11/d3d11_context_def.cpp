#include "d3d11_context_def.h"

namespace dxvk {
  
  D3D11DeferredContext::D3D11DeferredContext(
    D3D11Device*    pParent,
    Rc<DxvkDevice>  Device,
    UINT            ContextFlags)
  : D3D11DeviceContext(pParent, Device),
    m_contextFlags(ContextFlags),
    m_commandList (CreateCommandList()) {
    ClearState();
  }
  
  
  D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE D3D11DeferredContext::GetType() {
    return D3D11_DEVICE_CONTEXT_DEFERRED;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11DeferredContext::GetContextFlags() {
    return m_contextFlags;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeferredContext::Flush() {
    Logger::err("D3D11: Flush called on a deferred context");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeferredContext::ExecuteCommandList(
          ID3D11CommandList*  pCommandList,
          BOOL                RestoreContextState) {
    FlushCsChunk();
    
    static_cast<D3D11CommandList*>(pCommandList)->EmitToCommandList(m_commandList.ptr());
    
    if (RestoreContextState)
      RestoreState();
    else
      ClearState();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeferredContext::FinishCommandList(
          BOOL                RestoreDeferredContextState,
          ID3D11CommandList   **ppCommandList) {
    FlushCsChunk();
    
    if (ppCommandList != nullptr)
      *ppCommandList = m_commandList.ref();
    m_commandList = CreateCommandList();
    
    if (RestoreDeferredContextState)
      RestoreState();
    else
      ClearState();
    
    m_mappedResources.clear();
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeferredContext::Map(
          ID3D11Resource*             pResource,
          UINT                        Subresource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    if (MapType != D3D11_MAP_WRITE_DISCARD
     && MapType != D3D11_MAP_WRITE_NO_OVERWRITE)
      return E_INVALIDARG;
    
    if (resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      return MapBuffer(
        static_cast<D3D11Buffer*>(pResource),
        MapType, MapFlags, pMappedResource);
    } else {
      return MapImage(
        GetCommonTexture(pResource),
        Subresource, MapType, MapFlags,
        pMappedResource);
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeferredContext::Unmap(
          ID3D11Resource*             pResource,
          UINT                        Subresource) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    if (resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      UnmapBuffer(static_cast<D3D11Buffer*>(pResource));
    } else {
      UnmapImage(
        GetCommonTexture(pResource),
        Subresource);
    }
  }
  
  
  HRESULT D3D11DeferredContext::MapBuffer(
          D3D11Buffer*                pResource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    Rc<DxvkBuffer> buffer = pResource->GetBuffer();
    
    if (!(buffer->memFlags() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
      Logger::err("D3D11: Cannot map a device-local buffer");
      return E_INVALIDARG;
    }
    
    if (pMappedResource == nullptr)
      return S_FALSE;
    
    auto entry = FindMapEntry(pResource, 0);
    
    if (MapType == D3D11_MAP_WRITE_DISCARD) {
      if (entry != m_mappedResources.end())
        m_mappedResources.erase(entry);
      
      D3D11DeferredContextMapEntry mapEntry;
      mapEntry.pResource          = pResource;
      mapEntry.Subresource        = 0;
      mapEntry.MapType            = D3D11_MAP_WRITE_DISCARD;
      mapEntry.RowPitch           = pResource->GetSize();
      mapEntry.DepthPitch         = pResource->GetSize();
      mapEntry.DataSlice          = AllocUpdateBufferSlice(pResource->GetSize());
      m_mappedResources.push_back(mapEntry);
      
      pMappedResource->pData      = mapEntry.DataSlice.ptr();
      pMappedResource->RowPitch   = mapEntry.RowPitch;
      pMappedResource->DepthPitch = mapEntry.DepthPitch;
      return S_OK;
    } else {
      // The resource must be mapped with D3D11_MAP_WRITE_DISCARD
      // before it can be mapped with D3D11_MAP_WRITE_NO_OVERWRITE.
      if (entry == m_mappedResources.end())
        return E_INVALIDARG;
      
      // Return same memory region as earlier
      entry->MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
      
      pMappedResource->pData      = entry->DataSlice.ptr();
      pMappedResource->RowPitch   = entry->RowPitch;
      pMappedResource->DepthPitch = entry->DepthPitch;
      return S_OK;
    }
  }
  
  
  HRESULT D3D11DeferredContext::MapImage(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    Logger::err("D3D11DeferredContext::MapImage: Not implemented");
    return E_NOTIMPL;
  }
  
  
  void D3D11DeferredContext::UnmapBuffer(
          D3D11Buffer*                pResource) {
    auto entry = FindMapEntry(pResource, 0);
    
    if (entry == m_mappedResources.end()) {
      Logger::err("D3D11DeferredContext::Unmap: Buffer not mapped");
      return;
    }
    
    if (entry->MapType == D3D11_MAP_WRITE_DISCARD) {
      EmitCs([
        cDstBuffer = pResource->GetBuffer(),
        cDataSlice = entry->DataSlice
      ] (DxvkContext* ctx) {
        DxvkPhysicalBufferSlice slice = cDstBuffer->allocPhysicalSlice();
        std::memcpy(slice.mapPtr(0), cDataSlice.ptr(), cDataSlice.length());
        ctx->invalidateBuffer(cDstBuffer, slice);
      });
    }
  }
  
  
  void D3D11DeferredContext::UnmapImage(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource) {
    Logger::err("D3D11DeferredContext::UnmapImage: Not implemented");
  }
  
  
  Com<D3D11CommandList> D3D11DeferredContext::CreateCommandList() {
    return new D3D11CommandList(m_parent, m_contextFlags);
  }
  
  
  void D3D11DeferredContext::EmitCsChunk(Rc<DxvkCsChunk>&& chunk) {
    m_commandList->AddChunk(std::move(chunk));
  }

}