#include "d3d10_sampler.h"

#include "../d3d11/d3d11_device.h"
#include "../d3d11/d3d11_sampler.h"

namespace dxvk {

  HRESULT STDMETHODCALLTYPE D3D10SamplerState::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_d3d11->QueryInterface(riid, ppvObject);
  }


  ULONG STDMETHODCALLTYPE D3D10SamplerState::AddRef() {
    return m_d3d11->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D10SamplerState::Release() {
    return m_d3d11->Release();
  }


  void STDMETHODCALLTYPE D3D10SamplerState::GetDevice(
          ID3D10Device**            ppDevice) {
    GetD3D10Device(m_d3d11, ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D10SamplerState::GetPrivateData(
          REFGUID                   guid,
          UINT*                     pDataSize,
          void*                     pData) {
    return m_d3d11->GetPrivateData(guid, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10SamplerState::SetPrivateData(
          REFGUID                   guid,
          UINT                      DataSize,
    const void*                     pData) {
    return m_d3d11->SetPrivateData(guid, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10SamplerState::SetPrivateDataInterface(
          REFGUID                   guid,
    const IUnknown*                 pData) {
    return m_d3d11->SetPrivateDataInterface(guid, pData);
  }


  void STDMETHODCALLTYPE D3D10SamplerState::GetDesc(
          D3D10_SAMPLER_DESC*       pDesc) {
    D3D11_SAMPLER_DESC d3d11Desc;
    m_d3d11->GetDesc(&d3d11Desc);

    pDesc->Filter            = D3D10_FILTER(d3d11Desc.Filter);
    pDesc->AddressU          = D3D10_TEXTURE_ADDRESS_MODE(d3d11Desc.AddressU);
    pDesc->AddressV          = D3D10_TEXTURE_ADDRESS_MODE(d3d11Desc.AddressV);
    pDesc->AddressW          = D3D10_TEXTURE_ADDRESS_MODE(d3d11Desc.AddressW);
    pDesc->MipLODBias        = d3d11Desc.MipLODBias;
    pDesc->MaxAnisotropy     = d3d11Desc.MaxAnisotropy;
    pDesc->ComparisonFunc    = D3D10_COMPARISON_FUNC(d3d11Desc.ComparisonFunc);
    pDesc->MinLOD            = d3d11Desc.MinLOD;
    pDesc->MaxLOD            = d3d11Desc.MaxLOD;

    for (uint32_t i = 0; i < 4; i++)
      pDesc->BorderColor[i] = d3d11Desc.BorderColor[i];
  }

}