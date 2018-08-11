#pragma once

#include "d3d10_util.h"

namespace dxvk {

  template<typename D3D11Interface, typename D3D10Interface>
  class D3D11Shader;

  template<typename D3D10Interface, typename D3D11Interface> 
  class D3D10Shader : public D3D10Interface {
    using D3D11ShaderClass = D3D11Shader<D3D11Interface, D3D10Interface>;
  public:

    D3D10Shader(D3D11Shader<D3D11Interface, D3D10Interface>* pParent)
    : m_d3d11(pParent) { }


    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject) {
      return m_d3d11->QueryInterface(riid, ppvObject);
    }


    ULONG STDMETHODCALLTYPE AddRef() {
      return m_d3d11->AddRef();
    }


    ULONG STDMETHODCALLTYPE Release() {
      return m_d3d11->Release();
    }


    void STDMETHODCALLTYPE GetDevice(
            ID3D10Device**            ppDevice) {
      GetD3D10Device(m_d3d11, ppDevice);
    }


    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID                   guid,
            UINT*                     pDataSize,
            void*                     pData) {
      return m_d3d11->GetPrivateData(guid, pDataSize, pData);
    }


    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID                   guid,
            UINT                      DataSize,
      const void*                     pData) {
      return m_d3d11->SetPrivateData(guid, DataSize, pData);
    }


    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID                   guid,
      const IUnknown*                 pData) {
      return m_d3d11->SetPrivateDataInterface(guid, pData);
    }
    
    D3D11ShaderClass* GetD3D11Iface() {
      return m_d3d11;
    }

  private:

    D3D11ShaderClass* m_d3d11;

  };

  using D3D10VertexShader   = D3D10Shader<ID3D10VertexShader,   ID3D11VertexShader>;
  using D3D10GeometryShader = D3D10Shader<ID3D10GeometryShader, ID3D11GeometryShader>;
  using D3D10PixelShader    = D3D10Shader<ID3D10PixelShader,    ID3D11PixelShader>;

}