#include "d3d9_shader.h"

#include "d3d9_caps.h"
#include "d3d9_device.h"
#include "d3d9_util.h"

#include <sm3/sm3_parser.h>
#include <sm3/sm3_converter.h>
#include <sm3/sm3_types.h>

namespace dxvk {

  class D3D9SpecializationConstantLayout : public dxbc_spv::sm3::SpecializationConstantLayout {

  public:

    dxbc_spv::sm3::SpecializationConstantBits getSpecConstantLayout(dxbc_spv::sm3::SpecConstantId id) const {
      D3D9SpecConstantId specConstId = D3D9SpecConstantId(id);
      const auto& layoutEntry = D3D9SpecializationInfo::Layout[specConstId];
      return { layoutEntry.dwordOffset, layoutEntry.bitOffset, layoutEntry.sizeInBits };
    }

    uint32_t getSamplerSpecConstIndex(dxbc_spv::sm3::ShaderType shaderType, uint32_t perShaderSamplerIndex) {
      return shaderType == dxbc_spv::sm3::ShaderType::eVertex
        ? perShaderSamplerIndex + FirstVSSamplerSlot
        : perShaderSamplerIndex;
    }

    uint32_t getOptimizedDwordOffset() const {
      return MaxNumSpecConstants;
    }

  };

  class D3D9ShaderConverter : public DxvkIrShaderConverter {

  public:

    D3D9ShaderConverter(
      const DxvkShaderHash&           ShaderKey,
      const D3D9ShaderOptions&        Options,
      const void*                     pShaderBytecode,
            size_t                    BytecodeLength)
    : m_key(ShaderKey), m_options(Options) {
      m_dxbc.resize(BytecodeLength);
      std::memcpy(m_dxbc.data(), pShaderBytecode, BytecodeLength);
    }

    ~D3D9ShaderConverter() { }

    void convertShader(
            dxbc_spv::ir::Builder&    builder) {
      auto debugName = m_key.toString();

      dxbc_spv::sm3::Converter::Options options = { };
      options.name = debugName.c_str();
      options.includeDebugNames = true;
      options.fastFloatEmulation = m_options.d3d9FloatEmulation == D3D9FloatEmulation::Enabled;
      options.isSWVP = m_options.isSWVP && m_key.stage() == VK_SHADER_STAGE_VERTEX_BIT;

      dxbc_spv::util::ByteReader reader(m_dxbc.data(), m_dxbc.size());

      D3D9SpecializationConstantLayout specConstLayout;

      dxbc_spv::sm3::Converter converter(reader, specConstLayout, options);

      if (!converter.convertShader(builder))
        throw DxvkError(str::format("Failed to convert shader: ", m_key.toString()));
    }

    uint32_t determineResourceIndex(
            dxbc_spv::ir::ShaderStage stage,
            dxbc_spv::ir::ScalarType  type,
            uint32_t                  regSpace,
            uint32_t                  regIndex) const {

      // D3D9ShaderType has pixel shaders at 1, dxbc_spv has vertex shaders at 1.
      D3D9ShaderType shaderType = stage == dxbc_spv::ir::ShaderStage::ePixel
        ? D3D9ShaderType::PixelShader
        : D3D9ShaderType::VertexShader;

      switch (type) {
        case dxbc_spv::ir::ScalarType::eCbv:
          switch (regIndex) {
            case dxbc_spv::sm3::FastSpecConstCbvRegIdx:
              return D3D9ShaderResourceMapping::getSpecConstantBufferSlot();

            case dxbc_spv::sm3::PSSharedDataCbvRegIdx: {
              if (shaderType == D3D9ShaderType::PixelShader) {
                return D3D9ShaderResourceMapping::computeCbvBinding(shaderType,
                  D3D9ShaderResourceMapping::ConstantBuffers::PSShared);
              } else { //case dxbc_spv::sm3::VSClipPlanesCbvRegIdx: (same index)
                return D3D9ShaderResourceMapping::computeCbvBinding(shaderType,
                D3D9ShaderResourceMapping::ConstantBuffers::VSClipPlanes);
              }
            }

            // the index for the D3D9 constant buffer are is the same for VS and PS */
            case dxbc_spv::sm3::FloatIntCbvRegIdx:
                return D3D9ShaderResourceMapping::computeCbvBinding(shaderType,
                  D3D9ShaderResourceMapping::ConstantBuffers::VSConstantBuffer);

            case dxbc_spv::sm3::SWVPIntCbvRegIdx:
                return D3D9ShaderResourceMapping::computeCbvBinding(shaderType,
                D3D9ShaderResourceMapping::ConstantBuffers::VSIntConstantBuffer);

            case dxbc_spv::sm3::SWVPBoolCbvRegIdx:
                return D3D9ShaderResourceMapping::computeCbvBinding(shaderType,
                  D3D9ShaderResourceMapping::ConstantBuffers::VSBoolConstantBuffer);

            default: break;
          }
          break;

        case dxbc_spv::ir::ScalarType::eSrv:
        case dxbc_spv::ir::ScalarType::eSampler:
          return D3D9ShaderResourceMapping::computeTextureBinding(shaderType, regIndex);

        default: break;
      }

      Logger::err(str::format("Missing Resource index. Stage: ", stage, ", regSpace: ", regSpace, ", regIndex: ", regIndex));
      return -1u;
    }

    void dumpSource(const std::string& path) const {
      std::ofstream file(str::topath(str::format(path, "/", m_key.toString(), ".sm3_dxbc").c_str()).c_str(), std::ios_base::trunc | std::ios_base::binary);
      file.write(reinterpret_cast<const char*>(m_dxbc.data()), m_dxbc.size());
    }

    std::string getDebugName() const {
      return m_key.toString();
    }

  private:

    std::vector<uint8_t> m_dxbc;

    DxvkShaderHash       m_key;

    D3D9ShaderOptions    m_options;

  };

  D3D9CommonShader::D3D9CommonShader(
            D3D9DeviceEx*         pDevice,
      const DxvkShaderHash&       ShaderKey,
            D3D9ShaderAnalysis&&  ShaderAnalysis,
      const D3D9ShaderCreateInfo& ModuleInfo,
      const void*                 pShaderBytecode)
        : m_analysis(std::move(ShaderAnalysis)) {
    const std::string name = ShaderKey.toString();
    Logger::debug(str::format("Compiling shader ", name));
    
    // If requested by the user, dump both the raw DXBC
    // shader and the compiled SPIR-V module to a file.
    const std::string& dumpPath = pDevice->GetOptions()->shaderDumpPath;
    
    if (dumpPath.size() != 0) {
      const uint32_t bytecodeLength = m_analysis.GetLength();

      std::ofstream file(str::topath(str::format(dumpPath, "/", name, ".sm3_dxbc").c_str()).c_str(), std::ios_base::binary | std::ios_base::trunc);
      file.write(reinterpret_cast<const char*>(pShaderBytecode), bytecodeLength);
    }

    if (pDevice->GetOptions()->useDxbcSpirv)
      CreateIrShader(pDevice, ShaderKey, ModuleInfo, pShaderBytecode, m_analysis.GetLength());
    else
      CreateLegacyShader(pDevice, ShaderKey, ModuleInfo, pShaderBytecode);

    if (!dumpPath.empty()) {
      std::ofstream dumpStream(
        str::topath(str::format(dumpPath, "/", name, ".spv").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc);
      m_shader->dump(dumpStream);
    }

    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }


  void D3D9CommonShader::CreateIrShader(
          D3D9DeviceEx*           pDevice,
    const DxvkShaderHash&         ShaderKey,
    const D3D9ShaderCreateInfo&   ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength) {
    m_shader = pDevice->GetDXVKDevice()->createCachedShader(
      ShaderKey.toString(), ModuleInfo.irCreateInfo, nullptr);

    if (!m_shader) {
      Rc<D3D9ShaderConverter> converter = new D3D9ShaderConverter(ShaderKey, ModuleInfo.shaderOptions,
        pShaderBytecode, BytecodeLength);

      m_shader = pDevice->GetDXVKDevice()->createCachedShader(
        ShaderKey.toString(), ModuleInfo.irCreateInfo, std::move(converter));
    }
  }


  void D3D9CommonShader::CreateLegacyShader(
            D3D9DeviceEx*         pDevice,
      const DxvkShaderHash&       ShaderKey,
      const D3D9ShaderCreateInfo& ModuleInfo,
      const void*                 pShaderBytecode) {

    DxsoReader reader(
      reinterpret_cast<const char*>(pShaderBytecode));
    DxsoModule module(reader);

    const D3D9ConstantLayout& constantLayout = module.info().shaderStage() == VK_SHADER_STAGE_VERTEX_BIT
      ? pDevice->GetVertexConstantLayout()
      : pDevice->GetPixelConstantLayout();

    DxsoModuleInfo moduleInfo;
    moduleInfo.options.d3d9FloatEmulation = ModuleInfo.shaderOptions.d3d9FloatEmulation;
    moduleInfo.options.forceSamplerTypeSpecConstants = ModuleInfo.shaderOptions.forceSamplerTypeSpecConstants;
    moduleInfo.options.sincosEmulation = ModuleInfo.irCreateInfo.options.flags.test(DxvkShaderCompileFlag::LowerSinCos);
    moduleInfo.options.forceSampleRateShading = ModuleInfo.irCreateInfo.options.flags.test(DxvkShaderCompileFlag::EnableSampleRateShading);
    moduleInfo.options.vertexFloatConstantBufferAsSSBO = ModuleInfo.irCreateInfo.options.maxUniformBufferSize < constantLayout.totalSize();

    m_shader       = module.compile(moduleInfo, ShaderKey.toString(), module.analyze(), constantLayout);
  }


  HRESULT D3D9ShaderModuleSet::GetShaderModule(
          D3D9DeviceEx*           pDevice,
    const DxvkShaderHash&         ShaderKey,
          D3D9ShaderAnalysis&&    ShaderAnalysis,
    const D3D9ShaderCreateInfo&   ModuleInfo,
    const void*                   pShaderBytecode,
          D3D9CommonShader*       pShader) {
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    auto entry = m_modules.find(ShaderKey);
    if (entry != m_modules.end()) {
      *pShader = entry->second;
      return D3D_OK;
    }

    *pShader = D3D9CommonShader(pDevice, ShaderKey, std::move(ShaderAnalysis), ModuleInfo, pShaderBytecode);

    m_modules.insert({ ShaderKey, *pShader });
    return D3D_OK;
  }


}
