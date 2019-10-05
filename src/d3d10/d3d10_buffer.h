#pragma once

#include "d3d10_util.h"

namespace dxvk {

  class D3D11Buffer;
  class D3D11Device;
  class D3D10Device;

  class D3D10Buffer : public ID3D10Buffer {

  public:

    D3D10Buffer(D3D11Buffer* pParent)
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

    void STDMETHODCALLTYPE GetType(
            D3D10_RESOURCE_DIMENSION* rType);

    void STDMETHODCALLTYPE SetEvictionPriority(
            UINT                      EvictionPriority);

    UINT STDMETHODCALLTYPE GetEvictionPriority();

    HRESULT STDMETHODCALLTYPE Map(
            D3D10_MAP                 MapType,
            UINT                      MapFlags,
            void**                    ppData);

    void STDMETHODCALLTYPE Unmap();

    void STDMETHODCALLTYPE GetDesc(
            D3D10_BUFFER_DESC*        pDesc);
    
    D3D11Buffer* GetD3D11Iface() {
      return m_d3d11;
    }

  private:

    D3D11Buffer* m_d3d11;

  };

}