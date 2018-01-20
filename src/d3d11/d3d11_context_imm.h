#pragma once

#include "d3d11_context.h"

namespace dxvk {
  
  class D3D11ImmediateContext : public D3D11DeviceContext {
    
  public:
    
    D3D11ImmediateContext(
      D3D11Device*    parent,
      Rc<DxvkDevice>  device);
    ~D3D11ImmediateContext();
    
    ULONG STDMETHODCALLTYPE AddRef() final;
    
    ULONG STDMETHODCALLTYPE Release() final;
    
    D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType() final;
    
    UINT STDMETHODCALLTYPE GetContextFlags() final;
    
    void STDMETHODCALLTYPE Flush() final;
    
    void STDMETHODCALLTYPE ExecuteCommandList(
            ID3D11CommandList*  pCommandList,
            WINBOOL             RestoreContextState) final;
    
    HRESULT STDMETHODCALLTYPE FinishCommandList(
            WINBOOL             RestoreDeferredContextState,
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
    
    void Synchronize();
    void SynchronizeCs();
    
    void EmitCsChunk();
    
  };
  
}