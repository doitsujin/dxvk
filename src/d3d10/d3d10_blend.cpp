#include "d3d10_blend.h"

#include "../d3d11/d3d11_blend.h"
#include "../d3d11/d3d11_device.h"

namespace dxvk {

  HRESULT STDMETHODCALLTYPE D3D10BlendState::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_d3d11->QueryInterface(riid, ppvObject);
  }


  ULONG STDMETHODCALLTYPE D3D10BlendState::AddRef() {
    return m_d3d11->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D10BlendState::Release() {
    return m_d3d11->Release();
  }


  void STDMETHODCALLTYPE D3D10BlendState::GetDevice(
          ID3D10Device**            ppDevice) {
    GetD3D10Device(m_d3d11, ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D10BlendState::GetPrivateData(
          REFGUID                   guid,
          UINT*                     pDataSize,
          void*                     pData) {
    return m_d3d11->GetPrivateData(guid, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10BlendState::SetPrivateData(
          REFGUID                   guid,
          UINT                      DataSize,
    const void*                     pData) {
    return m_d3d11->SetPrivateData(guid, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10BlendState::SetPrivateDataInterface(
          REFGUID                   guid,
    const IUnknown*                 pData) {
    return m_d3d11->SetPrivateDataInterface(guid, pData);
  }


  void STDMETHODCALLTYPE D3D10BlendState::GetDesc(
          D3D10_BLEND_DESC*         pDesc) {
    D3D11_BLEND_DESC d3d11Desc;
    m_d3d11->GetDesc(&d3d11Desc);

    pDesc->AlphaToCoverageEnable  = d3d11Desc.AlphaToCoverageEnable;
    pDesc->SrcBlend               = D3D10_BLEND   (d3d11Desc.RenderTarget[0].SrcBlend);
    pDesc->DestBlend              = D3D10_BLEND   (d3d11Desc.RenderTarget[0].DestBlend);
    pDesc->BlendOp                = D3D10_BLEND_OP(d3d11Desc.RenderTarget[0].BlendOp);
    pDesc->SrcBlendAlpha          = D3D10_BLEND   (d3d11Desc.RenderTarget[0].SrcBlendAlpha);
    pDesc->DestBlendAlpha         = D3D10_BLEND   (d3d11Desc.RenderTarget[0].DestBlendAlpha);
    pDesc->BlendOpAlpha           = D3D10_BLEND_OP(d3d11Desc.RenderTarget[0].BlendOpAlpha);

    for (uint32_t i = 0; i < 8; i++) {
      uint32_t srcId = d3d11Desc.IndependentBlendEnable ? i : 0;
      pDesc->BlendEnable[i]           = d3d11Desc.RenderTarget[srcId].BlendEnable;
      pDesc->RenderTargetWriteMask[i] = d3d11Desc.RenderTarget[srcId].RenderTargetWriteMask;
    }
  }


  void STDMETHODCALLTYPE D3D10BlendState::GetDesc1(
          D3D10_BLEND_DESC1*        pDesc) {
    static_assert(sizeof(D3D10_BLEND_DESC1) == sizeof(D3D11_BLEND_DESC));
    m_d3d11->GetDesc(reinterpret_cast<D3D11_BLEND_DESC*>(pDesc));
  }

}