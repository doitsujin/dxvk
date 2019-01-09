#pragma once

#include "d3d11_buffer.h"
#include "d3d11_cmdlist.h"
#include "d3d11_context.h"
#include "d3d11_texture.h"

#include <algorithm>
#include <vector>

namespace dxvk {
  
  struct D3D11DeferredContextMapEntry {
    Com<ID3D11Resource>     pResource;
    UINT                    Subresource;
    D3D11_MAP               MapType;
    UINT                    RowPitch;
    UINT                    DepthPitch;
    DxvkDataSlice           DataSlice;
    DxvkBufferSliceHandle   BufferSlice;
    void*                   MapPointer;
  };
  
  class D3D11DeferredContext : public D3D11DeviceContext {
    
  public:
    
    D3D11DeferredContext(
            D3D11Device*    pParent,
      const Rc<DxvkDevice>& Device,
            UINT            ContextFlags);
    
    D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType();
    
    UINT STDMETHODCALLTYPE GetContextFlags();
    
    HRESULT STDMETHODCALLTYPE GetData(
            ID3D11Asynchronous*               pAsync,
            void*                             pData,
            UINT                              DataSize,
            UINT                              GetDataFlags);
    
    void STDMETHODCALLTYPE Flush();
    
    void STDMETHODCALLTYPE ExecuteCommandList(
            ID3D11CommandList*  pCommandList,
            BOOL                RestoreContextState);
    
    HRESULT STDMETHODCALLTYPE FinishCommandList(
            BOOL                RestoreDeferredContextState,
            ID3D11CommandList   **ppCommandList);
    
    HRESULT STDMETHODCALLTYPE Map(
            ID3D11Resource*             pResource,
            UINT                        Subresource,
            D3D11_MAP                   MapType,
            UINT                        MapFlags,
            D3D11_MAPPED_SUBRESOURCE*   pMappedResource);
    
    void STDMETHODCALLTYPE Unmap(
            ID3D11Resource*             pResource,
            UINT                        Subresource);
    
  private:
    
    const UINT m_contextFlags;
    
    // Command list that we're recording
    Com<D3D11CommandList> m_commandList;
    
    // Info about currently mapped (sub)resources. Using a vector
    // here is reasonable since there will usually only be a small
    // number of mapped resources per command list.
    std::vector<D3D11DeferredContextMapEntry> m_mappedResources;
    
    HRESULT MapBuffer(
            ID3D11Resource*               pResource,
            D3D11_MAP                     MapType,
            UINT                          MapFlags,
            D3D11DeferredContextMapEntry* pMapEntry);
    
    HRESULT MapImage(
            ID3D11Resource*               pResource,
            UINT                          Subresource,
            D3D11_MAP                     MapType,
            UINT                          MapFlags,
            D3D11DeferredContextMapEntry* pMapEntry);
    
    void UnmapBuffer(
            ID3D11Resource*               pResource,
      const D3D11DeferredContextMapEntry* pMapEntry);
    
    void UnmapImage(
            ID3D11Resource*               pResource,
            UINT                          Subresource,
      const D3D11DeferredContextMapEntry* pMapEntry);
    
    Com<D3D11CommandList> CreateCommandList();
    
    void EmitCsChunk(DxvkCsChunkRef&& chunk);

    static DxvkCsChunkFlags GetCsChunkFlags(
            D3D11Device*                  pDevice);
    
    auto FindMapEntry(ID3D11Resource* pResource, UINT Subresource) {
      return std::find_if(m_mappedResources.rbegin(), m_mappedResources.rend(),
        [pResource, Subresource] (const D3D11DeferredContextMapEntry& entry) {
          return entry.pResource   == pResource
              && entry.Subresource == Subresource;
        });
    }
    
  };
  
}
