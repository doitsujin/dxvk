#pragma once

#include "../dxvk/dxvk_shader.h"
#include "../dxvk/dxvk_shader_key.h"
#include "../dxvk/dxvk_shader_ir.h"

#include <sm3/sm3_parser.h>

#include "d3d9_resource.h"
#include "d3d9_util.h"
#include "d3d9_mem.h"

#include <array>
#include <string>

#include "d3d9_shader_analysis.h"

namespace dxvk {

  struct D3D9ShaderOptions {
    /** Whether the device is configured to do SWVP.
     * This must only be true for vertex shaders.
     * It results in significantly larger amounts of shader constants.
     */
    bool isSWVP;

    /** Whether to emulate d3d9 float behaviour using clampps
     * True:  Perform emulation to emulate behaviour (ie. anything * 0 = 0)
     * False: Don't do anything.
     */
    D3D9FloatEmulation d3d9FloatEmulation;

    /** Always use a spec constant to determine sampler type (instead of just in PS 1.x)
     * Works around a game bug in Halo CE where it gives cube textures to 2d/volume samplers
     */
    bool forceSamplerTypeSpecConstants;
  };

  static_assert(sizeof(D3D9ShaderOptions) == 3u);

  struct D3D9ShaderCreateInfo {
    DxvkIrShaderCreateInfo irCreateInfo;

    D3D9ShaderOptions shaderOptions;
  };

  /**
   * \brief Shader resource mapping
   *
   * Helper class to compute backend resource
   * indices for D3D9 binding slots.
   */
  struct D3D9ShaderResourceMapping {
    enum CbvIndex : uint32_t {
      VSClipPlanes            = 0u,
      VSFixedFunction         = 1u,
      VSVertexBlendData       = 2u,
      VSStaticConstants       = 3u,
      VSDynamicConstants      = 4u,
      PSShared                = 5u,
      PSStaticConstants       = 6u,

      Count
    };

    static constexpr uint32_t computeTextureBinding(D3D9ShaderType shaderType, uint32_t index) {
      auto base = (shaderType == D3D9ShaderType::VertexShader) ? FirstVSSamplerSlot : 0u;
      return base + index;
    }

    static constexpr uint32_t getSwvpBufferIndex() {
      return caps::MaxTextures;
    }

    static constexpr std::pair<VkShaderStageFlags, uint32_t> getTextureSlotInfo(uint32_t index) {
      // Sampler slot and binding indices match 1:1, see above
      return std::make_pair(IsVSSampler(index) ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT, index);
    }
  };

  /**
   * \brief Common shader object
   * 
   * Stores the compiled SPIR-V shader and the SHA-1
   * hash of the original DXBC shader, which can be
   * used to identify the shader.
   */
  class D3D9CommonShader {

  public:

    D3D9CommonShader() = default;

    D3D9CommonShader(
            D3D9DeviceEx*         pDevice,
      const DxvkShaderHash&       ShaderKey,
            D3D9ShaderAnalysis&&  Analysis,
      const D3D9ShaderCreateInfo& ModuleInfo,
      const void*                 pShaderBytecode);


    Rc<DxvkShader> GetShader() const {
      return m_shader;
    }

    std::string GetName() const {
      return m_shader->debugName();
    }

    const D3D9InputSignature& GetInputSignature() const {
      return m_analysis.GetInputSignature();
    }

    const D3D9ShaderConstantsInfo& GetConstantsInfo() const { return m_analysis.GetConstantsInfo(); }
    const D3D9ImmediateConstantsInfo& GetImmediateConstants() const { return m_analysis.GetImmediateConstants(); }

    const D3D9ConstantBufferCopy* GetConstantLayout() const {
      return m_analysis.GetConstantLayout();
    }

    D3D9ShaderMasks GetShaderMask() const { return D3D9ShaderMasks{ m_analysis.GetSamplerMask(), m_analysis.GetRenderTargetMask() }; }

    dxbc_spv::sm3::ShaderInfo GetInfo() const { return m_analysis.GetShaderInfo(); }

  private:

    D3D9ShaderAnalysis    m_analysis;
    Rc<DxvkShader>        m_shader;

  };

  /**
   * \brief Common shader interface
   * 
   * Implements methods for all D3D11*Shader
   * interfaces and stores the actual shader
   * module object.
   */
  template <typename Base>
  class D3D9Shader : public D3D9DeviceChild<Base> {

  public:

    D3D9Shader(
            D3D9DeviceEx*        pDevice,
            D3D9MemoryAllocator* pAllocator,
      const D3D9CommonShader&    CommonShader,
      const void*                pShaderBytecode,
            uint32_t             BytecodeLength)
      : D3D9DeviceChild<Base>( pDevice )
      , m_shader             ( CommonShader )
      , m_bytecode           ( pAllocator->Alloc(BytecodeLength) )
      , m_bytecodeLength     ( BytecodeLength ) {
      m_bytecode.Map();
      std::memcpy(m_bytecode.Ptr(), pShaderBytecode, BytecodeLength);
      m_bytecode.Unmap();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      if (ppvObject == nullptr)
        return E_POINTER;

      *ppvObject = nullptr;

      if (riid == __uuidof(IUnknown)
       || riid == __uuidof(Base)) {
        *ppvObject = ref(this);
        return S_OK;
      }

      if (logQueryInterfaceError(__uuidof(Base), riid)) {
        Logger::warn("D3D9Shader::QueryInterface: Unknown interface query");
        Logger::warn(str::format(riid));
      }

      return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE GetFunction(void* pOut, UINT* pSizeOfData) {
      if (pSizeOfData == nullptr)
        return D3DERR_INVALIDCALL;

      if (pOut == nullptr) {
        *pSizeOfData = m_bytecodeLength;
        return D3D_OK;
      }

      m_bytecode.Map();
      uint32_t copyAmount = std::min(*pSizeOfData, m_bytecodeLength);
      std::memcpy(pOut, m_bytecode.Ptr(), copyAmount);
      m_bytecode.Unmap();

      return D3D_OK;
    }

    const D3D9CommonShader* GetCommonShader() const {
      return &m_shader;
    }

  private:

    D3D9CommonShader m_shader;

    D3D9Memory       m_bytecode;
    uint32_t         m_bytecodeLength;

  };

  // Needs their own classes and not usings for forward decl.

  class D3D9VertexShader final : public D3D9Shader<IDirect3DVertexShader9> {

  public:

    D3D9VertexShader(
            D3D9DeviceEx*        pDevice,
            D3D9MemoryAllocator* pAllocator,
      const D3D9CommonShader&    CommonShader,
      const void*                pShaderBytecode,
            uint32_t             BytecodeLength)
      : D3D9Shader<IDirect3DVertexShader9>( pDevice, pAllocator, CommonShader, pShaderBytecode, BytecodeLength ) { }

  };

  class D3D9PixelShader final : public D3D9Shader<IDirect3DPixelShader9> {

  public:

    D3D9PixelShader(
            D3D9DeviceEx*        pDevice,
            D3D9MemoryAllocator* pAllocator,
      const D3D9CommonShader&    CommonShader,
      const void*                pShaderBytecode,
            uint32_t             BytecodeLength)
      : D3D9Shader<IDirect3DPixelShader9>( pDevice, pAllocator, CommonShader, pShaderBytecode, BytecodeLength ) { }

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
    
    HRESULT GetShaderModule(
            D3D9DeviceEx*           pDevice,
      const DxvkShaderHash&         ShaderKey,
            D3D9ShaderAnalysis&&    ShaderAnalysis,
      const D3D9ShaderCreateInfo&   ModuleInfo,
      const void*                   pShaderBytecode,
            D3D9CommonShader*       pShader);
    
  private:
    
    dxvk::mutex m_mutex;
    
    std::unordered_map<
      DxvkShaderHash,
      D3D9CommonShader,
      DxvkHash, DxvkEq> m_modules;
    
  };

  template<typename T>
  const D3D9CommonShader* GetCommonShader(const T& pShader) {
    return pShader != nullptr ? pShader->GetCommonShader() : nullptr;
  }

}
