#pragma once

#include <chrono>

#include "d3d11_context.h"

namespace dxvk {
  
  class D3D11Buffer;
  class D3D11CommonTexture;
  
  class D3D11ImmediateContext : public D3D11DeviceContext {
  public:
    
    D3D11ImmediateContext(
            D3D11Device*    pParent,
      const Rc<DxvkDevice>& Device);
    ~D3D11ImmediateContext();
    
    ULONG STDMETHODCALLTYPE AddRef() final;
    
    ULONG STDMETHODCALLTYPE Release() final;
    
    D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType() final;
    
    UINT STDMETHODCALLTYPE GetContextFlags() final;
    
    HRESULT STDMETHODCALLTYPE GetData(
            ID3D11Asynchronous*               pAsync,
            void*                             pData,
            UINT                              DataSize,
            UINT                              GetDataFlags) final;
    
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
    
    void STDMETHODCALLTYPE CopySubresourceRegion(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
            UINT                              DstX,
            UINT                              DstY,
            UINT                              DstZ,
            ID3D11Resource*                   pSrcResource,
            UINT                              SrcSubresource,
      const D3D11_BOX*                        pSrcBox) final;
    
    void STDMETHODCALLTYPE CopySubresourceRegion1(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
            UINT                              DstX,
            UINT                              DstY,
            UINT                              DstZ,
            ID3D11Resource*                   pSrcResource,
            UINT                              SrcSubresource,
      const D3D11_BOX*                        pSrcBox,
            UINT                              CopyFlags) final;
    
    void STDMETHODCALLTYPE CopyResource(
            ID3D11Resource*                   pDstResource,
            ID3D11Resource*                   pSrcResource) final;
    
    void STDMETHODCALLTYPE GenerateMips(
            ID3D11ShaderResourceView*         pShaderResourceView) final;
    
    void STDMETHODCALLTYPE UpdateSubresource(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
      const D3D11_BOX*                        pDstBox,
      const void*                             pSrcData,
            UINT                              SrcRowPitch,
            UINT                              SrcDepthPitch) final;
    
    void STDMETHODCALLTYPE UpdateSubresource1(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
      const D3D11_BOX*                        pDstBox,
      const void*                             pSrcData,
            UINT                              SrcRowPitch,
            UINT                              SrcDepthPitch,
            UINT                              CopyFlags) final;
    
    void STDMETHODCALLTYPE ResolveSubresource(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
            ID3D11Resource*                   pSrcResource,
            UINT                              SrcSubresource,
            DXGI_FORMAT                       Format) final;
            
    void STDMETHODCALLTYPE OMSetRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView) final;
    
    void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(
            UINT                              NumRTVs,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView,
            UINT                              UAVStartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
      const UINT*                             pUAVInitialCounts) final;
    
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
