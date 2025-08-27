#include "d3d9_shader.h"

#include "d3d9_caps.h"
#include "d3d9_device.h"
#include "d3d9_util.h"

namespace dxvk {

  D3D9CommonShader::D3D9CommonShader() {}

  D3D9CommonShader::D3D9CommonShader(
            D3D9DeviceEx*         pDevice,
            VkShaderStageFlagBits ShaderStage,
      const DxvkShaderKey&        Key,
      const DxsoModuleInfo*       pDxsoModuleInfo,
      const void*                 pShaderBytecode,
      const DxsoAnalysisInfo&     AnalysisInfo,
            DxsoModule*           pModule) {
    const uint32_t bytecodeLength = AnalysisInfo.bytecodeByteLength;

    const std::string name = Key.toString();
    Logger::debug(str::format("Compiling shader ", name));
    
    // If requested by the user, dump both the raw DXBC
    // shader and the compiled SPIR-V module to a file.
    const std::string& dumpPath = pDevice->GetOptions()->shaderDumpPath;
    
    if (dumpPath.size() != 0) {
      DxsoReader reader(
        reinterpret_cast<const char*>(pShaderBytecode));

      reader.store(std::ofstream(str::topath(str::format(dumpPath, "/", name, ".dxso").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc), bytecodeLength);

      char comment[2048];
      Com<ID3DBlob> blob;
      HRESULT hr = DisassembleShader(
        pShaderBytecode,
        TRUE,
        comment, 
        &blob);
      
      if (SUCCEEDED(hr)) {
        std::ofstream disassembledOut(str::topath(str::format(dumpPath, "/", name, ".dxso.dis").c_str()).c_str(), std::ios_base::binary | std::ios_base::trunc);
        disassembledOut.write(
          reinterpret_cast<const char*>(blob->GetBufferPointer()),
          blob->GetBufferSize());
      }
    }
    
    // Decide whether we need to create a pass-through
    // geometry shader for vertex shader stream output

    const D3D9ConstantLayout& constantLayout = ShaderStage == VK_SHADER_STAGE_VERTEX_BIT
      ? pDevice->GetVertexConstantLayout()
      : pDevice->GetPixelConstantLayout();
    m_shader       = pModule->compile(*pDxsoModuleInfo, name, AnalysisInfo, constantLayout);
    m_isgn         = pModule->isgn();
    m_usedSamplers = pModule->usedSamplers();
    m_textureTypes = pModule->textureTypes();

    // Shift up these sampler bits so we can just
    // do an or per-draw in the device.
    // We shift by 17 because 16 ps samplers + 1 dmap (tess)
    if (ShaderStage == VK_SHADER_STAGE_VERTEX_BIT)
      m_usedSamplers <<= FirstVSSamplerSlot;

    m_usedRTs              = pModule->usedRTs();

    m_info                 = pModule->info();
    m_meta                 = pModule->meta();
    m_constants            = pModule->constants();
    m_maxDefinedFloatConst = pModule->maxDefinedFloatConstant();
    m_maxDefinedIntConst   = pModule->maxDefinedIntConstant();
    m_maxDefinedBoolConst  = pModule->maxDefinedBoolConstant();

    if (dumpPath.size() != 0) {
      std::ofstream dumpStream(
        str::topath(str::format(dumpPath, "/", name, ".spv").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc);
      
      m_shader->dump(dumpStream);
    }

    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }


  void D3D9ShaderModuleSet::GetShaderModule(
            D3D9DeviceEx*         pDevice,
            D3D9CommonShader*     pShaderModule,
            uint32_t*             pLength,
            VkShaderStageFlagBits ShaderStage,
      const DxsoModuleInfo*       pDxbcModuleInfo,
      const void*                 pShaderBytecode) {
    DxsoReader reader(
      reinterpret_cast<const char*>(pShaderBytecode));

    DxsoModule module(reader);

    if (unlikely(module.info().shaderStage() != ShaderStage))
      throw DxvkError("GetShaderModule: Bytecode does not match shader stage");

    auto* options = pDevice->GetOptions();

    const uint32_t majorVersion = module.info().majorVersion();
    const uint32_t minorVersion = module.info().minorVersion();

    // Vertex shader version checks
    if (ShaderStage == VK_SHADER_STAGE_VERTEX_BIT) {
      // Late fixed-function capable hardware exposed support for VS 1.1
      const uint32_t shaderModelVS = pDevice->IsD3D8Compatible() ? 1u : std::max(1u, options->shaderModel);

      if (unlikely(majorVersion > shaderModelVS
               || (majorVersion == 1 && minorVersion > 1)
               // Skip checking the SM2 minor version, as it has a 2_x mode apparently
               || (majorVersion == 3 && minorVersion != 0))) {
        throw DxvkError(str::format("GetShaderModule: Unsupported VS version ", majorVersion, ".", minorVersion));
      }
    // Pixel shader version checks
    } else if (ShaderStage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      const uint32_t shaderModelPS = pDevice->IsD3D8Compatible() ? std::min(1u, options->shaderModel) : options->shaderModel;

      if (unlikely(majorVersion > shaderModelPS
               || (majorVersion == 1 && minorVersion > 4)
               // Skip checking the SM2 minor version, as it has a 2_x mode apparently
               || (majorVersion == 3 && minorVersion != 0))) {
        throw DxvkError(str::format("GetShaderModule: Unsupported PS version ", majorVersion, ".", minorVersion));
      }
    } else {
      throw DxvkError("GetShaderModule: Unsupported shader stage");
    }

    DxsoAnalysisInfo info = module.analyze();
    *pLength = info.bytecodeByteLength;

    DxvkShaderKey lookupKey = DxvkShaderKey(
      ShaderStage,
      Sha1Hash::compute(pShaderBytecode, info.bytecodeByteLength));

    // Use the shader's unique key for the lookup
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto entry = m_modules.find(lookupKey);
      if (entry != m_modules.end()) {
        *pShaderModule = entry->second;
        return;
      }
    }
    
    // This shader has not been compiled yet, so we have to create a
    // new module. This takes a while, so we won't lock the structure.
    *pShaderModule = D3D9CommonShader(
      pDevice, ShaderStage, lookupKey,
      pDxbcModuleInfo, pShaderBytecode,
      info, &module);

    const int32_t maxFloatConstantIndex = pShaderModule->GetMaxDefinedFloatConstant();
    const int32_t maxIntConstantIndex = pShaderModule->GetMaxDefinedIntConstant();
    const int32_t maxBoolConstantIndex = pShaderModule->GetMaxDefinedBoolConstant();

    // Vertex shader specific validations. These validations are not
    // performed on SWVP devices or on MIXED devices, even if
    // SetSoftwareVertexProcessing(FALSE) is used to disable SWVP mode.
    if (!pDevice->CanSWVP() && ShaderStage == VK_SHADER_STAGE_VERTEX_BIT) {
      // Validate the float constant value advertised in pCaps->MaxFloatConstantsVS for HWVP.
      if (unlikely(maxFloatConstantIndex > static_cast<int32_t>(caps::MaxFloatConstantsVS - 1)))
        throw DxvkError(str::format("GetShaderModule: Invalid VS float constant index ", maxFloatConstantIndex));
      // Validate the integer constant value advertised in pCaps->MaxOtherConstants for HWVP.
      if (unlikely(maxIntConstantIndex > static_cast<int32_t>(caps::MaxOtherConstants - 1)))
        throw DxvkError(str::format("GetShaderModule: Invalid VS int constant index ", maxIntConstantIndex));
      // Validate the bool constant value advertised in pCaps->MaxOtherConstants for HWVP.
      if (unlikely(maxBoolConstantIndex > static_cast<int32_t>(caps::MaxOtherConstants - 1)))
        throw DxvkError(str::format("GetShaderModule: Invalid VS bool constant index ", maxBoolConstantIndex));
    // Pixel shader specific validations.
    } else if (ShaderStage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      const bool isSM2XOrNewer = majorVersion == 3 || (majorVersion == 2 && minorVersion != 0);
      // Pixel shader model version 2_x has the same limits here as version 2_0
      const uint32_t maxFloatConstantsPS = majorVersion == 3 ? caps::MaxSM3FloatConstantsPS :
                                           majorVersion == 2 ? caps::MaxSM2FloatConstantsPS :
                                           caps::MaxSM1FloatConstantsPS;
      // Validate the float constant value coresponding to the supported shader model version.
      if (unlikely(!pDevice->CanSWVP() && maxFloatConstantIndex > static_cast<int32_t>(maxFloatConstantsPS - 1)))
        throw DxvkError(str::format("GetShaderModule: Invalid PS float constant index ", maxFloatConstantIndex));
      // Pixel shaders below version 2_x can not use integer constants, not even in SWVP/MIXED mode
      if (unlikely(!isSM2XOrNewer && maxIntConstantIndex != -1))
        throw DxvkError("GetShaderModule: Invalid use of PS int constant");
      // Validate the integer constant value advertised in pCaps->MaxOtherConstants for HWVP.
      else if (unlikely(isSM2XOrNewer && !pDevice->CanSWVP() &&
                        maxIntConstantIndex > static_cast<int32_t>(caps::MaxOtherConstants - 1)))
        throw DxvkError(str::format("GetShaderModule: Invalid PS int constant index ", maxIntConstantIndex));
      // Pixel shaders below version 2_x can not use bool constants, not even in SWVP/MIXED mode
      if (unlikely(!isSM2XOrNewer && maxBoolConstantIndex != -1))
        throw DxvkError("GetShaderModule: Invalid use of PS bool constant");
      // Validate the bool constant value advertised in pCaps->MaxOtherConstants for HWVP.
      else if (unlikely(isSM2XOrNewer && !pDevice->CanSWVP() &&
                        maxBoolConstantIndex > static_cast<int32_t>(caps::MaxOtherConstants - 1)))
        throw DxvkError(str::format("GetShaderModule: Invalid PS bool constant index ", maxBoolConstantIndex));
    }
    
    // Insert the new module into the lookup table. If another thread
    // has compiled the same shader in the meantime, we should return
    // that object instead and discard the newly created module.
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto status = m_modules.insert({ lookupKey, *pShaderModule });
      if (!status.second) {
        *pShaderModule = status.first->second;
        return;
      }
    }
  }

}
