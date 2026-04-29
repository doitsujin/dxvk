#pragma once

#include "../dxso/dxso_module.h"

#include "../dxvk/dxvk_shader.h"
#include "../dxvk/dxvk_shader_key.h"
#ifndef DXSO
#include "../dxvk/dxvk_shader_ir.h"
#endif

#include "d3d9_resource.h"
#include "d3d9_util.h"
#include "d3d9_mem.h"

#include <array>

namespace dxvk {

#ifndef DXSO

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

  struct D3D9ShaderCreateInfo {
    DxvkIrShaderCreateInfo irCreateInfo;

    D3D9ShaderOptions shaderOptions;

    DxsoAnalysisInfo analysisInfo;
  };

#else

  struct D3D9ShaderCreateInfo {};

#endif

  /**
   * \brief Shader resource mapping
   *
   * Helper class to compute backend resource
   * indices for D3D9 binding slots.
   */
  struct D3D9ShaderResourceMapping {
    enum ConstantBuffers : uint32_t {
      VSConstantBuffer = 0,
      VSFloatConstantBuffer = 0,
      VSIntConstantBuffer = 1,
      VSBoolConstantBuffer = 2,
      VSClipPlanes     = 3,
      VSFixedFunction  = 4,
      VSVertexBlendData = 5,
      VSCount,

      PSConstantBuffer = 0,
      PSFixedFunction  = 1,
      PSShared         = 2,
      PSCount
    };

    static constexpr uint32_t computeCbvBinding(D3D9ShaderType shaderType, uint32_t index) {
      const uint32_t stageOffset = (ConstantBuffers::VSCount + caps::MaxTexturesVS) * computeStageIndex(shaderType);
      return index + stageOffset;
    }

    static constexpr uint32_t computeTextureBinding(D3D9ShaderType shaderType, uint32_t index) {
      const uint32_t stageIndex = computeStageIndex(shaderType);
      const uint32_t stageOffset = (ConstantBuffers::VSCount + caps::MaxTexturesVS) * stageIndex;
      return index + stageOffset
        + (stageIndex == 1u
          ? ConstantBuffers::PSCount
          : ConstantBuffers::VSCount);
    }

    static constexpr uint32_t computeStageIndex(D3D9ShaderType shaderType) {
      return uint32_t(shaderType);
    }

    static constexpr uint32_t getSWVPBufferSlot() {
      return ConstantBuffers::VSCount + caps::MaxTexturesVS + ConstantBuffers::PSCount + caps::MaxTexturesPS + 1; // From last pixel shader slot, above.
    }

    static constexpr uint32_t getSpecConstantBufferSlot() {
      return getSWVPBufferSlot() + 1;
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

    D3D9CommonShader();

    D3D9CommonShader(
            D3D9DeviceEx*         pDevice,
      const DxvkShaderHash&       ShaderKey,
      const D3D9ShaderCreateInfo& ModuleInfo,
      const void*                 pShaderBytecode);


    Rc<DxvkShader> GetShader() const {
      return m_shader;
    }

    std::string GetName() const {
      return m_shader->debugName();
    }

    const DxsoIsgn& GetIsgn() const {
      return m_isgn;
    }

    const DxsoShaderMetaInfo& GetMeta() const { return m_meta; }
    const DxsoDefinedConstants& GetConstants() const { return m_constants; }

    D3D9ShaderMasks GetShaderMask() const { return D3D9ShaderMasks{ m_usedSamplers, m_usedRTs }; }

    const DxsoProgramInfo& GetInfo() const { return m_info; }

    int32_t GetMaxDefinedFloatConstant() const { return m_maxDefinedFloatConst; }

    int32_t GetMaxDefinedIntConstant() const { return m_maxDefinedIntConst; }

    int32_t GetMaxDefinedBoolConstant() const { return m_maxDefinedBoolConst; }

    VkImageViewType GetImageViewType(uint32_t samplerSlot) const {
      const uint32_t offset = samplerSlot * 2;
      const uint32_t mask = 0b11;
      return static_cast<VkImageViewType>((m_textureTypes >> offset) & mask);
    }

  private:

    void CreateLegacyShader(
            D3D9DeviceEx*         pDevice,
      const DxvkShaderHash&       ShaderKey,
      const D3D9ShaderCreateInfo& ModuleInfo,
      const void*                 pShaderBytecode);

    DxsoIsgn              m_isgn;
    uint32_t              m_usedSamplers;
    uint32_t              m_usedRTs;
    uint32_t              m_textureTypes;

    DxsoProgramInfo       m_info;
    DxsoShaderMetaInfo    m_meta;
    DxsoDefinedConstants  m_constants;
    int32_t               m_maxDefinedFloatConst = -1;
    int32_t               m_maxDefinedIntConst = -1;
    int32_t               m_maxDefinedBoolConst = -1;

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
