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

    CreateLegacyShader(pDevice, ShaderKey, ModuleInfo, pShaderBytecode);

    if (!dumpPath.empty()) {
      std::ofstream dumpStream(
        str::topath(str::format(dumpPath, "/", name, ".spv").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc);
      m_shader->dump(dumpStream);
    }

    pDevice->GetDXVKDevice()->registerShader(m_shader);
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

    // Use the shader's unique key for the lookup
    { std::unique_lock<dxvk::mutex> lock(m_mutex);

      auto entry = m_modules.find(ShaderKey);
      if (entry != m_modules.end()) {
        *pShader = entry->second;
        return D3D_OK;
      }
    }

    // This shader has not been compiled yet, so we have to create a
    // new module. This takes a while, so we won't lock the structure.
    *pShader = D3D9CommonShader(pDevice, ShaderKey, std::move(ShaderAnalysis), ModuleInfo, pShaderBytecode);

    // Insert the new module into the lookup table. If another thread
    // has compiled the same shader in the meantime, we should return
    // that object instead and discard the newly created module.
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    auto status = m_modules.insert({ ShaderKey, *pShader });
    if (!status.second)
      *pShader = status.first->second;

    return D3D_OK;
  }


}
