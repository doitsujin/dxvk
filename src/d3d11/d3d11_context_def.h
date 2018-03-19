#pragma once

#include "d3d11_buffer.h"
#include "d3d11_cmdlist.h"
#include "d3d11_context.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  struct D3D11DeferredContextMapEntry {
    Com<ID3D11Resource> pResource;
    UINT                Subresource;
    D3D11_MAP           MapType;
    UINT                RowPitch;
    UINT                DepthPitch;
    DxvkDataSlice       DataSlice;
  };
  
  class D3D11DeferredContext : public D3D11DeviceContext {
    
  public:
    
    D3D11DeferredContext(
      D3D11Device*    pParent,
      Rc<DxvkDevice>  Device,
      UINT            ContextFlags);
    
    D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType() final;
    
    UINT STDMETHODCALLTYPE GetContextFlags() final;
    
    void STDMETHODCALLTYPE Flush() final;
    
    void STDMETHODCALLTYPE ExecuteCommandList(
            ID3D11CommandList*  pCommandList,
            BOOL                RestoreContextState) final;
    
    HRESULT STDMETHODCALLTYPE FinishCommandList(
            BOOL                RestoreDeferredContextState,
            ID3D11CommandList   **ppCommandList) final;
    
    HRESULT STDMETHODCALLTYPE Map(
            ID3D11Resource*             pResource,
            UINT                        Subresource,
            D3D11_MAP                   MapType,
            UINT                        MapFlags,
            D3D11_MAPPED_SUBRESOURCE*   pMappedResource) final;
    
    void STDMETHODCALLTYPE Unmap(
            ID3D11Resource*             pResource,
            UINT                        Subresource) final;
    
  private:
    
    const UINT m_contextFlags;
    
    // Command list that we're recording
    Com<D3D11CommandList> m_commandList;
    
    // Info about currently mapped (sub)resources. Using a vector
    // here is reasonable since there will usually only be a small
    // number of mapped resources per command list.
    std::vector<D3D11DeferredContextMapEntry> m_mappedResources;
    
    HRESULT MapBuffer(
            D3D11Buffer*                pResource,
            D3D11_MAP                   MapType,
            UINT                        MapFlags,
            D3D11_MAPPED_SUBRESOURCE*   pMappedResource);
    
    HRESULT MapImage(
            D3D11CommonTexture*         pResource,
            UINT                        Subresource,
            D3D11_MAP                   MapType,
            UINT                        MapFlags,
            D3D11_MAPPED_SUBRESOURCE*   pMappedResource);
    
    void UnmapBuffer(
            D3D11Buffer*                pResource);
    
    void UnmapImage(
            D3D11CommonTexture*         pResource,
            UINT                        Subresource);
    
    Com<D3D11CommandList> CreateCommandList();
    
    void EmitCsChunk(Rc<DxvkCsChunk>&& chunk) final;
    
    auto FindMapEntry(ID3D11Resource* pResource, UINT Subresource) {
      return std::find_if(m_mappedResources.begin(), m_mappedResources.end(),
        [pResource, Subresource] (const D3D11DeferredContextMapEntry& entry) {
          return entry.pResource   == pResource
              && entry.Subresource == Subresource;
        });
    }
    
  };
  
}