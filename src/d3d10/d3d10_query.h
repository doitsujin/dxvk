#pragma once

#include "d3d10_util.h"

namespace dxvk {

  class D3D10Device;
  class D3D11Device;
  class D3D11DeviceContext;
  class D3D11Query;

  class D3D10Query : public ID3D10Predicate {

  public:

    D3D10Query(D3D11Query* pParent)
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

    void STDMETHODCALLTYPE Begin();

    void STDMETHODCALLTYPE End();

    HRESULT STDMETHODCALLTYPE GetData(
            void*                     pData,
            UINT                      DataSize,
            UINT                      GetDataFlags);

    UINT STDMETHODCALLTYPE GetDataSize();
    
    void STDMETHODCALLTYPE GetDesc(
            D3D10_QUERY_DESC*         pDesc);
    
    D3D11Query* GetD3D11Iface() {
      return m_d3d11;
    }

  private:

    D3D11Query*  m_d3d11;

  };

}