#pragma once

#include "d3d10_util.h"

namespace dxvk {

  class D3D11RasterizerState;
  class D3D11Device;

  class D3D10RasterizerState : public ID3D10RasterizerState {

  public:

    D3D10RasterizerState(D3D11RasterizerState* pParent)
    : m_d3d11(pParent) { }

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject);

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    void STDMETHODCALLTYPE GetDevice(
            ID3D10Device**            ppDevice);

    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID                   guid,
            UINT*                     pDataSize,
            void*                     pData);

    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID                   guid,
            UINT                      DataSize,
      const void*                     pData);

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID                   guid,
      const IUnknown*                 pData);
    
    void STDMETHODCALLTYPE GetDesc(
            D3D10_RASTERIZER_DESC*    pDesc);

    D3D11RasterizerState* GetD3D11Iface() {
      return m_d3d11;
    }
    
  private:

    D3D11RasterizerState* m_d3d11;

  };

}