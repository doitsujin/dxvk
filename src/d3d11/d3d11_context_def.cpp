#include "d3d11_context_def.h"

namespace dxvk {
  
  D3D11DeferredContext::D3D11DeferredContext(
    D3D11Device*    pParent,
    Rc<DxvkDevice>  Device,
    UINT            ContextFlags)
  : D3D11DeviceContext(pParent, Device),
    m_contextFlags(ContextFlags) {
    
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
          WINBOOL             RestoreContextState) {
    Logger::err("D3D11DeferredContext::ExecuteCommandList: Not implemented");
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeferredContext::FinishCommandList(
          WINBOOL             RestoreDeferredContextState,
          ID3D11CommandList   **ppCommandList) {
    Logger::err("D3D11DeferredContext::FinishCommandList: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeferredContext::Map(
          ID3D11Resource*             pResource,
          UINT                        Subresource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    Logger::err("D3D11DeferredContext::Map: Not implemented");
    return E_NOTIMPL;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeferredContext::Unmap(
          ID3D11Resource*             pResource,
          UINT                        Subresource) {
    Logger::err("D3D11DeferredContext::Unmap: Not implemented");
  }
  
  
  void D3D11DeferredContext::EmitCsChunk() {
    
  }

}