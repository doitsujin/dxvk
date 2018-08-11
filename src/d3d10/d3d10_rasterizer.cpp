#include "d3d10_rasterizer.h"

#include "../d3d11/d3d11_device.h"
#include "../d3d11/d3d11_rasterizer.h"

namespace dxvk {

  HRESULT STDMETHODCALLTYPE D3D10RasterizerState::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_d3d11->QueryInterface(riid, ppvObject);
  }


  ULONG STDMETHODCALLTYPE D3D10RasterizerState::AddRef() {
    return m_d3d11->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D10RasterizerState::Release() {
    return m_d3d11->Release();
  }


  void STDMETHODCALLTYPE D3D10RasterizerState::GetDevice(
          ID3D10Device**            ppDevice) {
    GetD3D10Device(m_d3d11, ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D10RasterizerState::GetPrivateData(
          REFGUID                   guid,
          UINT*                     pDataSize,
          void*                     pData) {
    return m_d3d11->GetPrivateData(guid, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10RasterizerState::SetPrivateData(
          REFGUID                   guid,
          UINT                      DataSize,
    const void*                     pData) {
    return m_d3d11->SetPrivateData(guid, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10RasterizerState::SetPrivateDataInterface(
          REFGUID                   guid,
    const IUnknown*                 pData) {
    return m_d3d11->SetPrivateDataInterface(guid, pData);
  }


  void STDMETHODCALLTYPE D3D10RasterizerState::GetDesc(
          D3D10_RASTERIZER_DESC*    pDesc) {
    static_assert(sizeof(D3D10_RASTERIZER_DESC) == sizeof(D3D11_RASTERIZER_DESC));
    m_d3d11->GetDesc(reinterpret_cast<D3D11_RASTERIZER_DESC*>(pDesc));
  }

}