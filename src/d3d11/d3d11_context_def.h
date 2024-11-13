#pragma once

#include "d3d11_cmdlist.h"
#include "d3d11_context.h"

#include <vector>

namespace dxvk {
  
  struct D3D11DeferredContextMapEntry {
    uint64_t                  ResourceCookie = 0u;
    D3D11_MAPPED_SUBRESOURCE  MapInfo = { };
  };
  
  class D3D11DeferredContext : public D3D11CommonContext<D3D11DeferredContext> {
    friend class D3D11CommonContext<D3D11DeferredContext>;
  public:
    
    D3D11DeferredContext(
            D3D11Device*    pParent,
      const Rc<DxvkDevice>& Device,
            UINT            ContextFlags);
    
    HRESULT STDMETHODCALLTYPE GetData(
            ID3D11Asynchronous*         pAsync,
            void*                       pData,
            UINT                        DataSize,
            UINT                        GetDataFlags);
    
    void STDMETHODCALLTYPE Begin(
            ID3D11Asynchronous*         pAsync);

    void STDMETHODCALLTYPE End(
            ID3D11Asynchronous*         pAsync);

    void STDMETHODCALLTYPE Flush();

    void STDMETHODCALLTYPE Flush1(
            D3D11_CONTEXT_TYPE          ContextType,
            HANDLE                      hEvent);

    HRESULT STDMETHODCALLTYPE Signal(
            ID3D11Fence*                pFence,
            UINT64                      Value);
    
    HRESULT STDMETHODCALLTYPE Wait(
            ID3D11Fence*                pFence,
            UINT64                      Value);

    void STDMETHODCALLTYPE ExecuteCommandList(
            ID3D11CommandList*          pCommandList,
            BOOL                        RestoreContextState);
    
    HRESULT STDMETHODCALLTYPE FinishCommandList(
            BOOL                        RestoreDeferredContextState,
            ID3D11CommandList**         ppCommandList);
    
    HRESULT STDMETHODCALLTYPE Map(
            ID3D11Resource*             pResource,
            UINT                        Subresource,
            D3D11_MAP                   MapType,
            UINT                        MapFlags,
            D3D11_MAPPED_SUBRESOURCE*   pMappedResource);
    
    void STDMETHODCALLTYPE Unmap(
            ID3D11Resource*             pResource,
            UINT                        Subresource);
    
    void STDMETHODCALLTYPE SwapDeviceContextState(
           ID3DDeviceContextState*           pState,
           ID3DDeviceContextState**          ppPreviousState);

    D3D10DeviceLock LockContext() {
      return D3D10DeviceLock();
    }

  private:
    
    // Command list that we're recording
    Com<D3D11CommandList> m_commandList;
    
    // Info about currently mapped (sub)resources. Using a vector
    // here is reasonable since there will usually only be a small
    // number of mapped resources per command list.
    std::vector<D3D11DeferredContextMapEntry> m_mappedResources;
    
    // Begun and ended queries, will also be stored in command list
    std::vector<Com<D3D11Query, false>> m_queriesBegun;

    // Chunk ID within the current command list
    uint64_t m_chunkId = 0ull;

    HRESULT MapBuffer(
            ID3D11Resource*               pResource,
            D3D11_MAPPED_SUBRESOURCE*     pMappedResource);
    
    HRESULT MapImage(
            ID3D11Resource*               pResource,
            UINT                          Subresource,
            D3D11_MAPPED_SUBRESOURCE*     pMappedResource);

    void UpdateMappedBuffer(
            D3D11Buffer*                  pDstBuffer,
            UINT                          Offset,
            UINT                          Length,
      const void*                         pSrcData,
            UINT                          CopyFlags);

    void FinalizeQueries();

    Com<D3D11CommandList> CreateCommandList();
    
    void EmitCsChunk(DxvkCsChunkRef&& chunk);

    uint64_t GetCurrentChunkId() const;

    void TrackTextureSequenceNumber(
            D3D11CommonTexture*           pResource,
            UINT                          Subresource);

    void TrackBufferSequenceNumber(
            D3D11Buffer*                  pResource);

    D3D11_MAPPED_SUBRESOURCE FindMapEntry(
            uint64_t                      Coookie);

    void AddMapEntry(
            uint64_t                      Cookie,
      const D3D11_MAPPED_SUBRESOURCE&     MapInfo);

    static DxvkCsChunkFlags GetCsChunkFlags(
            D3D11Device*                  pDevice);
    
  };
  
}
