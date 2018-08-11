#pragma once

#include "d3d10_util.h"

namespace dxvk {

  class D3D11BlendState;
  class D3D11Device;

  class D3D10BlendState : public ID3D10BlendState1 {

  public:

    D3D10BlendState(D3D11BlendState* pParent)
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
            D3D10_BLEND_DESC*         pDesc);

    void STDMETHODCALLTYPE GetDesc1(
            D3D10_BLEND_DESC1*        pDesc);

    D3D11BlendState* GetD3D11Iface() {
      return m_d3d11;
    }
    
  private:

    D3D11BlendState* m_d3d11;

  };

}