#pragma once

#include <mutex>
#include <unordered_map>

#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_shader.h"
#include "../dxvk/dxvk_shader_key.h"
#include "../dxvk/dxvk_shader_ir.h"

#include "../d3d10/d3d10_shader.h"

#include "../util/sha1/sha1_util.h"

#include "../util/util_env.h"

#include "d3d11_device_child.h"
#include "d3d11_interfaces.h"
#include "d3d11_util.h"

namespace dxvk {
  
  class D3D11Device;

  constexpr uint32_t D3D11ShaderTypeCount = 6u;

  using D3D11ShaderType = dxbc_spv::dxbc::ShaderType;
  using D3D11ShaderTypeFlags = Flags<dxbc_spv::dxbc::ShaderType>;

  /**
   * \brief Translates D3D11 shader stage to corresponding Vulkan stage
   *
   * \param [in] ProgramType DXBC program type
   * \returns Corresponding Vulkan shader stage
   */
  constexpr VkShaderStageFlagBits GetShaderStage(D3D11ShaderType ProgramType) {
    constexpr uint64_t lut
      = (uint64_t(VK_SHADER_STAGE_VERTEX_BIT)                   << (8u * uint32_t(D3D11ShaderType::eVertex)))
      | (uint64_t(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)     << (8u * uint32_t(D3D11ShaderType::eHull)))
      | (uint64_t(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)  << (8u * uint32_t(D3D11ShaderType::eDomain)))
      | (uint64_t(VK_SHADER_STAGE_GEOMETRY_BIT)                 << (8u * uint32_t(D3D11ShaderType::eGeometry)))
      | (uint64_t(VK_SHADER_STAGE_FRAGMENT_BIT)                 << (8u * uint32_t(D3D11ShaderType::ePixel)))
      | (uint64_t(VK_SHADER_STAGE_COMPUTE_BIT)                  << (8u * uint32_t(D3D11ShaderType::eCompute)));

    return VkShaderStageFlagBits((lut >> (8u * uint32_t(ProgramType))) & 0xff);
  }


  /**
   * \brief Shader resource mapping
   *
   * Helper class to compute backend resource
   * indices for D3D11 binding slots.
   */
  struct D3D11ShaderResourceMapping {
    static constexpr uint32_t StageCount        = 6u;
    static constexpr uint32_t CbvPerStage       = 16u;
    static constexpr uint32_t SamplersPerStage  = 16u;
    static constexpr uint32_t SrvPerStage       = 128u;
    static constexpr uint32_t SrvTotal          = SrvPerStage * StageCount;
    static constexpr uint32_t UavPerPipeline    = 64u;
    static constexpr uint32_t UavTotal          = UavPerPipeline * 4u;
    static constexpr uint32_t UavIndexGraphics  = SrvTotal;
    static constexpr uint32_t UavIndexCompute   = UavIndexGraphics + UavPerPipeline * 2u;

    template<typename T>
    static uint32_t computeCbvBinding(T stage, uint32_t index) {
      return computeStageIndex(stage) * CbvPerStage + index;
    }

    template<typename T>
    static uint32_t computeSamplerBinding(T stage, uint32_t index) {
      return computeStageIndex(stage) * SamplersPerStage + index;
    }

    template<typename T>
    static uint32_t computeSrvBinding(T stage, uint32_t index) {
      return computeStageIndex(stage) * SrvPerStage + index;
    }

    template<typename T>
    static uint32_t computeUavBinding(T stage, uint32_t index) {
      return (computeStageIndex(stage) == computeStageIndex(dxbc_spv::ir::ShaderStage::eCompute) ? UavIndexCompute : UavIndexGraphics) + index;
    }

    template<typename T>
    static uint32_t computeUavCounterBinding(T stage, uint32_t index) {
      return computeUavBinding(stage, index) + UavPerPipeline;
    }

    static constexpr uint32_t computeStageIndex(dxbc_spv::ir::ShaderStage stage) {
      switch (stage) {
        case dxbc_spv::ir::ShaderStage::ePixel:     return 0u;
        case dxbc_spv::ir::ShaderStage::eVertex:    return 1u;
        case dxbc_spv::ir::ShaderStage::eGeometry:  return 2u;
        case dxbc_spv::ir::ShaderStage::eHull:      return 3u;
        case dxbc_spv::ir::ShaderStage::eDomain:    return 4u;
        case dxbc_spv::ir::ShaderStage::eCompute:   return 5u;
        default:                                    return -1u;
      }
    }

    static constexpr uint32_t computeStageIndex(D3D11ShaderType stage) {
      return uint32_t(stage);
    }

  };


  /**
   * \brief Immediate constant buffer info
   */
  struct D3D11ShaderIcbInfo {
    /// Size in dwords
    size_t size = 0u;
    /// Constant data
    const uint32_t* data = nullptr;
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
            D3D11Device*            pDevice,
      const DxvkShaderHash&         ShaderKey,
      const DxvkIrShaderCreateInfo& ModuleInfo,
      const void*                   pShaderBytecode,
            size_t                  BytecodeLength,
      const D3D11ShaderIcbInfo&     Icb,
      const D3D11BindingMask&       BindingMask);
    ~D3D11CommonShader();

    Rc<DxvkShader> GetShader() const {
      return m_shader;
    }

    DxvkBufferSlice GetIcb() const {
      return m_buffer != nullptr
        ? DxvkBufferSlice(m_buffer)
        : DxvkBufferSlice();
    }
    
    std::string GetName() const {
      return m_shader->debugName();
    }

    D3D11BindingMask GetBindingMask() const {
      return m_bindings;
    }

  private:

    Rc<DxvkShader> m_shader;
    Rc<DxvkBuffer> m_buffer;

    D3D11BindingMask m_bindings = { };

    void CreateIrShader(
            D3D11Device*            pDevice,
      const DxvkShaderHash&         ShaderKey,
      const DxvkIrShaderCreateInfo& ModuleInfo,
      const void*                   pShaderBytecode,
            size_t                  BytecodeLength,
      const D3D11ShaderIcbInfo&     Icb);

  };


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
    : D3D11DeviceChild<D3D11Interface>(device),
      m_shader(shader), m_d3d10(this),
      m_destructionNotifier(this) { }
    
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

      if (riid == __uuidof(ID3DDestructionNotifier)) {
        *ppvObject = ref(&m_destructionNotifier);
        return S_OK;
      }

      if (logQueryInterfaceError(__uuidof(D3D11Interface), riid)) {
        Logger::warn("D3D11Shader::QueryInterface: Unknown interface query");
        Logger::warn(str::format(riid));
      }

      return E_NOINTERFACE;
    }
    
    const D3D11CommonShader* GetCommonShader() const {
      return &m_shader;
    }

    D3D10ShaderClass* GetD3D10Iface() {
      return &m_d3d10;
    }

  private:
    
    D3D11CommonShader m_shader;
    D3D10ShaderClass  m_d3d10;

    D3DDestructionNotifier m_destructionNotifier;
    
  };
  
  using D3D11VertexShader   = D3D11Shader<ID3D11VertexShader,   ID3D10VertexShader>;
  using D3D11HullShader     = D3D11Shader<ID3D11HullShader,     ID3D10DeviceChild>;
  using D3D11DomainShader   = D3D11Shader<ID3D11DomainShader,   ID3D10DeviceChild>;
  using D3D11GeometryShader = D3D11Shader<ID3D11GeometryShader, ID3D10GeometryShader>;
  using D3D11PixelShader    = D3D11Shader<ID3D11PixelShader,    ID3D10PixelShader>;
  using D3D11ComputeShader  = D3D11Shader<ID3D11ComputeShader,  ID3D10DeviceChild>;
  
  
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
    
    HRESULT GetShaderModule(
            D3D11Device*            pDevice,
      const DxvkShaderHash&         ShaderKey,
      const DxvkIrShaderCreateInfo& ModuleInfo,
      const void*                   pShaderBytecode,
            size_t                  BytecodeLength,
      const D3D11ShaderIcbInfo&     Icb,
      const D3D11BindingMask&       BindingMask,
            D3D11CommonShader*      pShader);
    
  private:
    
    dxvk::mutex m_mutex;
    
    std::unordered_map<
      DxvkShaderHash,
      D3D11CommonShader,
      DxvkHash, DxvkEq> m_modules;
    
  };
  
}
