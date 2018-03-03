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
          WINBOOL             RestoreContextState) {
    static_cast<D3D11CommandList*>(pCommandList)->EmitToCommandList(m_commandList.ptr());
    
    if (RestoreContextState)
      RestoreState();
    else
      ClearState();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeferredContext::FinishCommandList(
          WINBOOL             RestoreDeferredContextState,
          ID3D11CommandList   **ppCommandList) {
    if (ppCommandList != nullptr)
      *ppCommandList = m_commandList.ref();
    m_commandList = CreateCommandList();
    
    if (!RestoreDeferredContextState)
      ClearState();
    else
      RestoreState();
    
    return S_OK;
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
  
  
  Com<D3D11CommandList> D3D11DeferredContext::CreateCommandList() {
    return new D3D11CommandList(m_parent, m_contextFlags);
  }
  
  
  void D3D11DeferredContext::EmitCsChunk(Rc<DxvkCsChunk>&& chunk) {
    m_commandList->AddChunk(std::move(chunk));
  }

}