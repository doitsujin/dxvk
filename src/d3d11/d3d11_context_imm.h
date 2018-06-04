#pragma once

#include <chrono>

#include "d3d11_context.h"

namespace dxvk {
  
  class D3D11Buffer;
  class D3D11CommonTexture;
  
  class D3D11ImmediateContext : public D3D11DeviceContext {
    constexpr static uint32_t MinFlushIntervalUs = 2500;
    constexpr static uint32_t MaxPendingSubmits  = 2;
  public:
    
    D3D11ImmediateContext(
            D3D11Device*    pParent,
      const Rc<DxvkDevice>& Device);
    ~D3D11ImmediateContext();
    
    ULONG STDMETHODCALLTYPE AddRef() final;
    
    ULONG STDMETHODCALLTYPE Release() final;
    
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
    
    void STDMETHODCALLTYPE OMSetRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView) final;
    
    void SynchronizeCsThread();
    
  private:
    
    DxvkCsThread m_csThread;
    bool         m_csIsBusy = false;

    std::chrono::high_resolution_clock::time_point m_lastFlush
      = std::chrono::high_resolution_clock::now();
    
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
            UINT                              MapFlags);
    
    void EmitCsChunk(Rc<DxvkCsChunk>&& chunk) final;

    void FlushImplicit();
    
  };
  
}