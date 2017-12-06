#pragma once

#include <dxvk_device.h>

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11Device;
  
  
  /**
   * \brief Common shader interface
   * 
   * Implements methods for all D3D11*Shader
   * interfaces and stores the actual shader
   * module object.
   */
  template<typename Base>
  class D3D11Shader : public D3D11DeviceChild<Base> {
    
  public:
    
    D3D11Shader(D3D11Device* device)
    : m_device(device) { }
    
    ~D3D11Shader() { }
    
    HRESULT QueryInterface(REFIID riid, void** ppvObject) final {
      COM_QUERY_IFACE(riid, ppvObject, IUnknown);
      COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
      COM_QUERY_IFACE(riid, ppvObject, Base);
      
      Logger::warn("D3D11Shader::QueryInterface: Unknown interface query");
      return E_NOINTERFACE;
    }
    
    void GetDevice(ID3D11Device **ppDevice) final {
      *ppDevice = m_device.ref();
    }
    
  private:
    
    Com<D3D11Device> m_device;
    
  };
  
  using D3D11VertexShader   = D3D11Shader<ID3D11VertexShader>;
  using D3D11HullShader     = D3D11Shader<ID3D11HullShader>;
  using D3D11DomainShader   = D3D11Shader<ID3D11DomainShader>;
  using D3D11GeometryShader = D3D11Shader<ID3D11GeometryShader>;
  using D3D11PixelShader    = D3D11Shader<ID3D11PixelShader>;
  using D3D11ComputeShader  = D3D11Shader<ID3D11ComputeShader>;
  
}
