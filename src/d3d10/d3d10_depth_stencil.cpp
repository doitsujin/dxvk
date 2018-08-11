#include "d3d10_depth_stencil.h"

#include "../d3d11/d3d11_depth_stencil.h"
#include "../d3d11/d3d11_device.h"

namespace dxvk {

  HRESULT STDMETHODCALLTYPE D3D10DepthStencilState::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_d3d11->QueryInterface(riid, ppvObject);
  }


  ULONG STDMETHODCALLTYPE D3D10DepthStencilState::AddRef() {
    return m_d3d11->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D10DepthStencilState::Release() {
    return m_d3d11->Release();
  }


  void STDMETHODCALLTYPE D3D10DepthStencilState::GetDevice(
          ID3D10Device**            ppDevice) {
    GetD3D10Device(m_d3d11, ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D10DepthStencilState::GetPrivateData(
          REFGUID                   guid,
          UINT*                     pDataSize,
          void*                     pData) {
    return m_d3d11->GetPrivateData(guid, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10DepthStencilState::SetPrivateData(
          REFGUID                   guid,
          UINT                      DataSize,
    const void*                     pData) {
    return m_d3d11->SetPrivateData(guid, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10DepthStencilState::SetPrivateDataInterface(
          REFGUID                   guid,
    const IUnknown*                 pData) {
    return m_d3d11->SetPrivateDataInterface(guid, pData);
  }


  void STDMETHODCALLTYPE D3D10DepthStencilState::GetDesc(
          D3D10_DEPTH_STENCIL_DESC* pDesc) {
    static_assert(sizeof(D3D10_DEPTH_STENCIL_DESC) == sizeof(D3D11_DEPTH_STENCIL_DESC));
    m_d3d11->GetDesc(reinterpret_cast<D3D11_DEPTH_STENCIL_DESC*>(pDesc));
  }

}