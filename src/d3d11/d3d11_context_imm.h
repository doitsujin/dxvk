#pragma once

#include "../util/util_time.h"

#include "../util/sync/sync_signal_win32.h"

#include "d3d11_context.h"
#include "d3d11_state_object.h"
#include "d3d11_video.h"

namespace dxvk {
  
  class D3D11Buffer;
  class D3D11CommonTexture;
  
  class D3D11ImmediateContext : public D3D11DeviceContext {
    friend class D3D11SwapChain;
    friend class D3D11VideoContext;
  public:
    
    D3D11ImmediateContext(
            D3D11Device*    pParent,
      const Rc<DxvkDevice>& Device);
    ~D3D11ImmediateContext();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject);

    D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType();
    
    UINT STDMETHODCALLTYPE GetContextFlags();
    
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
            
    void STDMETHODCALLTYPE UpdateSubresource(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
      const D3D11_BOX*                        pDstBox,
      const void*                             pSrcData,
            UINT                              SrcRowPitch,
            UINT                              SrcDepthPitch);
    
    void STDMETHODCALLTYPE UpdateSubresource1(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
      const D3D11_BOX*                        pDstBox,
      const void*                             pSrcData,
            UINT                              SrcRowPitch,
            UINT                              SrcDepthPitch,
            UINT                              CopyFlags);

    void STDMETHODCALLTYPE OMSetRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView);
    
    void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(
            UINT                              NumRTVs,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView,
            UINT                              UAVStartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
      const UINT*                             pUAVInitialCounts);
    
    void STDMETHODCALLTYPE SwapDeviceContextState(
           ID3DDeviceContextState*           pState,
           ID3DDeviceContextState**          ppPreviousState);

    void SynchronizeCsThread();
    
  private:
    
    DxvkCsThread m_csThread;
    bool         m_csIsBusy = false;

    Rc<sync::Win32Fence> m_eventSignal;
    uint64_t             m_eventCount = 0;

    dxvk::high_resolution_clock::time_point m_lastFlush
      = dxvk::high_resolution_clock::now();
    
    D3D11VideoContext            m_videoContext;
    Com<D3D11DeviceContextState> m_stateObject;
    
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
    
    void UnmapImage(
            D3D11CommonTexture*         pResource,
            UINT                        Subresource);
    
    void SynchronizeDevice();
    
    bool WaitForResource(
      const Rc<DxvkResource>&                 Resource,
            D3D11_MAP                         MapType,
            UINT                              MapFlags);
    
    void EmitCsChunk(DxvkCsChunkRef&& chunk);

    void FlushImplicit(BOOL StrongHint);

    void SignalEvent(HANDLE hEvent);
    
  };
  
}
