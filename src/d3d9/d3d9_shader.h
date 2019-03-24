#pragma once

#include "d3d9_resource.h"
#include "../dxso/dxso_module.h"
#include "../dxso/dxso_decoder.h"

namespace dxvk {

  /**
   * \brief Common shader object
   * 
   * Stores the compiled SPIR-V shader and the SHA-1
   * hash of the original DXBC shader, which can be
   * used to identify the shader.
   */
  class D3D9CommonShader {

  public:

    D3D9CommonShader();

    D3D9CommonShader(
            Direct3DDevice9Ex*    pDevice,
      const DxvkShaderKey*        pShaderKey,
      const DxsoModuleInfo*       pDxbcModuleInfo,
      const void*                 pShaderBytecode,
            size_t                BytecodeLength);


    Rc<DxvkShader> GetShader() const {
      return m_shader;
    }

    std::string GetName() const {
      return m_shader->debugName();
    }

    const std::vector<uint8_t>& GetBytecode() const {
      return m_bytecode;
    }

    const std::array<DxsoDeclaration, 16>& GetDeclarations() const {
      return m_decl;
    }

  private:

    std::array<DxsoDeclaration, 16> m_decl;

    Rc<DxvkShader>        m_shader;

    std::vector<uint8_t>  m_bytecode;

  };

  /**
   * \brief Common shader interface
   * 
   * Implements methods for all D3D11*Shader
   * interfaces and stores the actual shader
   * module object.
   */
  template <typename Base>
  class D3D9Shader : public Direct3DDeviceChild9<Base> {

  public:

    D3D9Shader(
            Direct3DDevice9Ex* device,
      const D3D9CommonShader&  shader)
      : Direct3DDeviceChild9<Base>{ device }
      , m_shader{ shader } {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      if (ppvObject == nullptr)
        return E_POINTER;

      *ppvObject = nullptr;

      if (riid == __uuidof(IUnknown)
       || riid == __uuidof(Base)) {
        *ppvObject = ref(this);
        return S_OK;
      }

      Logger::warn("D3D9Shader::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
      return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE GetFunction(void* pOut, UINT* pSizeOfData) {
      if (pSizeOfData == nullptr)
        return D3DERR_INVALIDCALL;

      const auto& bytecode = m_shader.GetBytecode();

      if (pOut == nullptr) {
        *pSizeOfData = bytecode.size();
        return D3D_OK;
      }

      size_t copyAmount = std::min(size_t(*pSizeOfData), bytecode.size());
      std::memcpy(pOut, bytecode.data(), copyAmount);

      return D3D_OK;
    }

    const D3D9CommonShader* GetCommonShader() const {
      return &m_shader;
    }

  private:

    D3D9CommonShader m_shader;

  };

  // Needs their own classes and not usings for forward decl.

  class D3D9VertexShader final : public D3D9Shader<IDirect3DVertexShader9> {

  public:

    D3D9VertexShader(
            Direct3DDevice9Ex* device,
      const D3D9CommonShader&  shader)
      : D3D9Shader<IDirect3DVertexShader9>{ device, shader } {}

  };

  class D3D9PixelShader final : public D3D9Shader<IDirect3DPixelShader9> {

  public:

    D3D9PixelShader(
            Direct3DDevice9Ex* device,
      const D3D9CommonShader&  shader)
      : D3D9Shader<IDirect3DPixelShader9>{ device, shader } {}

  };

  /**
   * \brief Shader module set
   * 
   * Some applications may compile the same shader multiple
   * times, so we should cache the resulting shader modules
   * and reuse them rather than creating new ones. This
   * class is thread-safe.
   */
  class D3D9ShaderModuleSet : public RcObject {
    
  public:
    
    D3D9CommonShader GetShaderModule(
            Direct3DDevice9Ex* pDevice,
      const DxvkShaderKey*     pShaderKey,
      const DxsoModuleInfo*    pDxbcModuleInfo,
      const void*              pShaderBytecode,
            size_t             BytecodeLength);
    
  private:
    
    std::mutex m_mutex;
    
    std::unordered_map<
      DxvkShaderKey,
      D3D9CommonShader,
      DxvkHash, DxvkEq> m_modules;
    
};

}