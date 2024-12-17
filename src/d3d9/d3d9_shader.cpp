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
      m_usedSamplers <<= caps::MaxTexturesPS + 1;

    m_usedRTs      = pModule->usedRTs();

    m_info      = pModule->info();
    m_meta      = pModule->meta();
    m_constants = pModule->constants();
    m_maxDefinedConst = pModule->maxDefinedConstant();

    m_shader->setShaderKey(Key);

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

    if (module.info().majorVersion() > pDxbcModuleInfo->options.shaderModel)
      throw DxvkError("GetShaderModule: Out of range of supported shader model");

    if (module.info().shaderStage() != ShaderStage)
      throw DxvkError("GetShaderModule: Bytecode does not match shader stage");

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
