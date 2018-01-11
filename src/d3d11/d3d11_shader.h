#pragma once

#include <dxbc_module.h>
#include <dxvk_device.h>

#include "../util/sha1/sha1_util.h"

#include "../util/util_env.h"

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11Device;
  
  /**
   * \brief Shader module
   * 
   * 
   */
  class D3D11ShaderModule {
    
  public:
    
    D3D11ShaderModule();
    D3D11ShaderModule(
      const DxbcOptions*  pDxbcOptions,
            D3D11Device*  pDevice,
      const void*         pShaderBytecode,
            size_t        BytecodeLength);
    ~D3D11ShaderModule();
    
    Rc<DxvkShader> GetShader() const {
      return m_shader;
    }
    
    const std::string& GetName() const {
      return m_name;
    }
    
  private:
    
    std::string    m_name;
    Rc<DxvkShader> m_shader;
    
    Sha1Hash ComputeShaderHash(
      const void*   pShaderBytecode,
            size_t  BytecodeLength) const;
    
    std::string ConstructFileName(
      const Sha1Hash&         hash,
      const DxbcProgramType&  type) const;
    
  };
  
  
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
    
    D3D11Shader(D3D11Device* device, D3D11ShaderModule&& module)
    : m_device(device), m_module(std::move(module)) { }
    
    ~D3D11Shader() { }
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final {
      COM_QUERY_IFACE(riid, ppvObject, IUnknown);
      COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
      COM_QUERY_IFACE(riid, ppvObject, Base);
      
      Logger::warn("D3D11Shader::QueryInterface: Unknown interface query");
      return E_NOINTERFACE;
    }
    
    void STDMETHODCALLTYPE GetDevice(ID3D11Device **ppDevice) final {
      *ppDevice = m_device.ref();
    }
    
    Rc<DxvkShader> STDMETHODCALLTYPE GetShader() const {
      return m_module.GetShader();
    }
    
    const std::string& GetName() const {
      return m_module.GetName();
    }
    
  private:
    
    Com<D3D11Device>  m_device;
    D3D11ShaderModule m_module;
    
  };
  
  using D3D11VertexShader   = D3D11Shader<ID3D11VertexShader>;
  using D3D11HullShader     = D3D11Shader<ID3D11HullShader>;
  using D3D11DomainShader   = D3D11Shader<ID3D11DomainShader>;
  using D3D11GeometryShader = D3D11Shader<ID3D11GeometryShader>;
  using D3D11PixelShader    = D3D11Shader<ID3D11PixelShader>;
  using D3D11ComputeShader  = D3D11Shader<ID3D11ComputeShader>;
  
}
