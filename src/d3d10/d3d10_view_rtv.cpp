#include "d3d10_view_rtv.h"

#include "../d3d11/d3d11_device.h"
#include "../d3d11/d3d11_view_rtv.h"

namespace dxvk {

  HRESULT STDMETHODCALLTYPE D3D10RenderTargetView::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_d3d11->QueryInterface(riid, ppvObject);
  }


  ULONG STDMETHODCALLTYPE D3D10RenderTargetView::AddRef() {
    return m_d3d11->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D10RenderTargetView::Release() {
    return m_d3d11->Release();
  }


  void STDMETHODCALLTYPE D3D10RenderTargetView::GetDevice(
          ID3D10Device**            ppDevice) {
    GetD3D10Device(m_d3d11, ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D10RenderTargetView::GetPrivateData(
          REFGUID                   guid,
          UINT*                     pDataSize,
          void*                     pData) {
    return m_d3d11->GetPrivateData(guid, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10RenderTargetView::SetPrivateData(
          REFGUID                   guid,
          UINT                      DataSize,
    const void*                     pData) {
    return m_d3d11->SetPrivateData(guid, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10RenderTargetView::SetPrivateDataInterface(
          REFGUID                   guid,
    const IUnknown*                 pData) {
    return m_d3d11->SetPrivateDataInterface(guid, pData);
  }


  void STDMETHODCALLTYPE D3D10RenderTargetView::GetResource(
          ID3D10Resource**          ppResource) {
    GetD3D10ResourceFromView(m_d3d11, ppResource);
  }

  
  void STDMETHODCALLTYPE D3D10RenderTargetView::GetDesc(
          D3D10_RENDER_TARGET_VIEW_DESC* pDesc) {
    static_assert(sizeof(D3D10_RENDER_TARGET_VIEW_DESC) ==
                  sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
    
    m_d3d11->GetDesc(reinterpret_cast<D3D11_RENDER_TARGET_VIEW_DESC*>(pDesc));
  }

}