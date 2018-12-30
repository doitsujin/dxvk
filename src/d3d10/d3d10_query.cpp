#include "d3d10_query.h"
#include "d3d10_device.h"

#include "../d3d11/d3d11_device.h"
#include "../d3d11/d3d11_context.h"
#include "../d3d11/d3d11_query.h"

namespace dxvk {

  HRESULT STDMETHODCALLTYPE D3D10Query::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_d3d11->QueryInterface(riid, ppvObject);
  }


  ULONG STDMETHODCALLTYPE D3D10Query::AddRef() {
    return m_d3d11->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D10Query::Release() {
    return m_d3d11->Release();
  }


  void STDMETHODCALLTYPE D3D10Query::GetDevice(
          ID3D10Device**            ppDevice) {
    GetD3D10Device(m_d3d11, ppDevice);
  }
  

  HRESULT STDMETHODCALLTYPE D3D10Query::GetPrivateData(
          REFGUID                   guid,
          UINT*                     pDataSize,
          void*                     pData) {
    return m_d3d11->GetPrivateData(guid, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10Query::SetPrivateData(
          REFGUID                   guid,
          UINT                      DataSize,
    const void*                     pData) {
    return m_d3d11->SetPrivateData(guid, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10Query::SetPrivateDataInterface(
          REFGUID                   guid,
    const IUnknown*                 pData) {
    return m_d3d11->SetPrivateDataInterface(guid, pData);
  }


  void STDMETHODCALLTYPE D3D10Query::Begin() {
    Com<ID3D11DeviceContext> ctx;
    GetD3D11Context(m_d3d11, &ctx);

    ctx->Begin(m_d3d11);
  }


  void STDMETHODCALLTYPE D3D10Query::End() {
    Com<ID3D11DeviceContext> ctx;
    GetD3D11Context(m_d3d11, &ctx);

    ctx->End(m_d3d11);
  }


  HRESULT STDMETHODCALLTYPE D3D10Query::GetData(
          void*                     pData,
          UINT                      DataSize,
          UINT                      GetDataFlags) {
    Com<ID3D11DeviceContext> ctx;
    GetD3D11Context(m_d3d11, &ctx);

    return ctx->GetData(m_d3d11,
      pData, DataSize, GetDataFlags);
  }


  UINT STDMETHODCALLTYPE D3D10Query::GetDataSize() {
    return m_d3d11->GetDataSize();
  }

  
  void STDMETHODCALLTYPE D3D10Query::GetDesc(
          D3D10_QUERY_DESC*         pDesc) {
    D3D11_QUERY_DESC d3d11Desc;
    m_d3d11->GetDesc(&d3d11Desc);

    pDesc->Query      = D3D10_QUERY(d3d11Desc.Query);
    pDesc->MiscFlags  = d3d11Desc.MiscFlags;
  }

}