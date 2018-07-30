#pragma once

#include <mutex>
#include <unordered_map>

#include "../dxbc/dxbc_module.h"
#include "../dxvk/dxvk_device.h"

#include "../util/sha1/sha1_util.h"

#include "../util/util_env.h"

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"

namespace dxvk {
  
  class D3D11Device;
  
  /**
   * \brief Shader key
   * 
   * A unique identifier for a shader consisting
   * of the program type and the SHA-1 hash of
   * the shader's original bytecode.
   */
  class D3D11ShaderKey {
    
  public:
    
    D3D11ShaderKey(
            DxbcProgramType ProgramType,
      const void*           pShaderBytecode,
            size_t          BytecodeLength);
    
    std::string GetName() const;
    
    size_t GetHash() const;
    
    bool operator == (const D3D11ShaderKey& other) const {
      return m_type == other.m_type
          && m_hash == other.m_hash;
    }
    
  private:
    
    DxbcProgramType m_type;
    Sha1Hash        m_hash;
    
  };
  
  struct D3D11ShaderKeyHash {
    size_t operator () (const D3D11ShaderKey& a) const {
      return a.GetHash();
    }
  };
  
  
  /**
   * \brief Common shader object
   * 
   * Stores the compiled SPIR-V shader and the SHA-1
   * hash of the original DXBC shader, which can be
   * used to identify the shader.
   */
  class D3D11CommonShader {
    
  public:
    
    D3D11CommonShader();
    D3D11CommonShader(
            D3D11Device*    pDevice,
      const D3D11ShaderKey* pShaderKey,
      const DxbcModuleInfo* pDxbcModuleInfo,
      const void*           pShaderBytecode,
            size_t          BytecodeLength);
    ~D3D11CommonShader();

    Rc<DxvkShader> GetShader() const {
      return m_shader;
    }

    Rc<DxvkBuffer> GetIcb() const {
      return m_buffer;
    }
    
    std::string GetName() const {
      return m_name;
    }
    
  private:
    
    std::string    m_name;
    Rc<DxvkShader> m_shader;
    Rc<DxvkBuffer> m_buffer;
    
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
    
    D3D11Shader(D3D11Device* device, const D3D11CommonShader& shader)
    : m_device(device), m_shader(shader) { }
    
    ~D3D11Shader() { }
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final {
      *ppvObject = nullptr;
      
      if (riid == __uuidof(IUnknown)
       || riid == __uuidof(ID3D11DeviceChild)
       || riid == __uuidof(Base)) {
        *ppvObject = ref(this);
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
    
  private:
    
    Com<D3D11Device>  m_device;
    D3D11CommonShader m_shader;
    
  };
  
  using D3D11VertexShader   = D3D11Shader<ID3D11VertexShader>;
  using D3D11HullShader     = D3D11Shader<ID3D11HullShader>;
  using D3D11DomainShader   = D3D11Shader<ID3D11DomainShader>;
  using D3D11GeometryShader = D3D11Shader<ID3D11GeometryShader>;
  using D3D11PixelShader    = D3D11Shader<ID3D11PixelShader>;
  using D3D11ComputeShader  = D3D11Shader<ID3D11ComputeShader>;
  
  
  /**
   * \brief Shader module set
   * 
   * Some applications may compile the same shader multiple
   * times, so we should cache the resulting shader modules
   * and reuse them rather than creating new ones. This
   * class is thread-safe.
   */
  class D3D11ShaderModuleSet {
    
  public:
    
    D3D11ShaderModuleSet();
    ~D3D11ShaderModuleSet();
    
    D3D11CommonShader GetShaderModule(
            D3D11Device*    pDevice,
      const DxbcModuleInfo* pDxbcModuleInfo,
      const void*           pShaderBytecode,
            size_t          BytecodeLength,
            DxbcProgramType ProgramType);
    
  private:
    
    std::mutex m_mutex;
    
    std::unordered_map<
      D3D11ShaderKey,
      D3D11CommonShader,
      D3D11ShaderKeyHash> m_modules;
    
  };
  
}
