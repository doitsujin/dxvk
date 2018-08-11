#pragma once

#include "d3d10_util.h"

namespace dxvk {

  class D3D11Device;
  class D3D11ShaderResourceView;

  class D3D10ShaderResourceView : public ID3D10ShaderResourceView1 {

  public:

    D3D10ShaderResourceView(D3D11ShaderResourceView* pParent)
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

    void STDMETHODCALLTYPE GetResource(
            ID3D10Resource**          ppResource);
    
    void STDMETHODCALLTYPE GetDesc(
            D3D10_SHADER_RESOURCE_VIEW_DESC* pDesc);

    void STDMETHODCALLTYPE GetDesc1(
            D3D10_SHADER_RESOURCE_VIEW_DESC1* pDesc);

    D3D11ShaderResourceView* GetD3D11Iface() {
      return m_d3d11;
    }
    
  private:

    D3D11ShaderResourceView* m_d3d11;

  };

}