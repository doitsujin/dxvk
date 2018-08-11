#include "d3d10_view_srv.h"

#include "../d3d11/d3d11_device.h"
#include "../d3d11/d3d11_view_srv.h"

namespace dxvk {

  HRESULT STDMETHODCALLTYPE D3D10ShaderResourceView::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_d3d11->QueryInterface(riid, ppvObject);
  }


  ULONG STDMETHODCALLTYPE D3D10ShaderResourceView::AddRef() {
    return m_d3d11->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D10ShaderResourceView::Release() {
    return m_d3d11->Release();
  }


  void STDMETHODCALLTYPE D3D10ShaderResourceView::GetDevice(
          ID3D10Device**            ppDevice) {
    GetD3D10Device(m_d3d11, ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D10ShaderResourceView::GetPrivateData(
          REFGUID                   guid,
          UINT*                     pDataSize,
          void*                     pData) {
    return m_d3d11->GetPrivateData(guid, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10ShaderResourceView::SetPrivateData(
          REFGUID                   guid,
          UINT                      DataSize,
    const void*                     pData) {
    return m_d3d11->SetPrivateData(guid, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10ShaderResourceView::SetPrivateDataInterface(
          REFGUID                   guid,
    const IUnknown*                 pData) {
    return m_d3d11->SetPrivateDataInterface(guid, pData);
  }


  void STDMETHODCALLTYPE D3D10ShaderResourceView::GetResource(
          ID3D10Resource**          ppResource) {
    GetD3D10ResourceFromView(m_d3d11, ppResource);
  }

  
  void STDMETHODCALLTYPE D3D10ShaderResourceView::GetDesc(
          D3D10_SHADER_RESOURCE_VIEW_DESC* pDesc) {
    static_assert(sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC) ==
                  sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    
    m_d3d11->GetDesc(reinterpret_cast<D3D11_SHADER_RESOURCE_VIEW_DESC*>(pDesc));

    if (pDesc->ViewDimension > D3D10_SRV_DIMENSION_TEXTURECUBE)
      pDesc->ViewDimension = D3D10_SRV_DIMENSION_UNKNOWN;
  }


  void STDMETHODCALLTYPE D3D10ShaderResourceView::GetDesc1(
          D3D10_SHADER_RESOURCE_VIEW_DESC1* pDesc) {
    static_assert(sizeof(D3D10_SHADER_RESOURCE_VIEW_DESC1) ==
                  sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    
    m_d3d11->GetDesc(reinterpret_cast<D3D11_SHADER_RESOURCE_VIEW_DESC*>(pDesc));

    if (pDesc->ViewDimension > D3D10_1_SRV_DIMENSION_TEXTURECUBEARRAY)
      pDesc->ViewDimension = D3D10_1_SRV_DIMENSION_UNKNOWN;
  }

}