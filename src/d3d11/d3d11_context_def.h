#pragma once

#include "d3d11_cmdlist.h"
#include "d3d11_context.h"

namespace dxvk {
  
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
    
    Com<D3D11CommandList> m_commandList;
    
    Com<D3D11CommandList> CreateCommandList();
    
    void EmitCsChunk(Rc<DxvkCsChunk>&& chunk) final;
    
  };
  
}