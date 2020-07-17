#pragma once

#include "d3d11_device.h"
#include "d3d11_shader.h"

namespace dxvk {

  /**
   * \brief Common shader interface
   * 
   * Implements methods for all D3D11*Shader
   * interfaces and stores the actual shader
   * module object.
   */
  template<typename D3D11Interface, typename D3D10Interface>
  class D3D11Shader : public D3D11DeviceChild<D3D11Interface> {
    using D3D10ShaderClass = D3D10Shader<D3D10Interface, D3D11Interface>;
  public:
    
    D3D11Shader(D3D11Device* device, const D3D11CommonShader& shader)
    : m_device(device), m_shader(shader), m_d3d10(this) { }
    
    ~D3D11Shader() { }
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final {
      *ppvObject = nullptr;
      
      if (riid == __uuidof(IUnknown)
       || riid == __uuidof(ID3D11DeviceChild)
       || riid == __uuidof(D3D11Interface)) {
        *ppvObject = ref(this);
        return S_OK;
      }
      
      if (riid == __uuidof(IUnknown)
       || riid == __uuidof(ID3D10DeviceChild)
       || riid == __uuidof(D3D10Interface)) {
        *ppvObject = ref(&m_d3d10);
        return S_OK;
      }
      
      Logger::warn("D3D11Shader::QueryInterface: Unknown interface query");
      return E_NOINTERFACE;
    }
    
    void STDMETHODCALLTYPE GetDevice(ID3D11Device **ppDevice) final {
      *ppDevice = m_device.ref();
    }
    
    const D3D11CommonShader* GetCommonShader() const {
      return &m_shader;
    }

    D3D10ShaderClass* GetD3D10Iface() {
      return &m_d3d10;
    }

  private:
    
    Com<D3D11Device>  m_device;
    D3D11CommonShader m_shader;
    D3D10ShaderClass  m_d3d10;
    
  };
  
  using D3D11VertexShader   = D3D11Shader<ID3D11VertexShader,   ID3D10VertexShader>;
  using D3D11HullShader     = D3D11Shader<ID3D11HullShader,     ID3D10DeviceChild>;
  using D3D11DomainShader   = D3D11Shader<ID3D11DomainShader,   ID3D10DeviceChild>;
  using D3D11GeometryShader = D3D11Shader<ID3D11GeometryShader, ID3D10GeometryShader>;
  using D3D11PixelShader    = D3D11Shader<ID3D11PixelShader,    ID3D10PixelShader>;
  using D3D11ComputeShader  = D3D11Shader<ID3D11ComputeShader,  ID3D10DeviceChild>;

}