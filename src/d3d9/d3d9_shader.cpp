#include "d3d9_shader.h"

#include "d3d9_util.h"

namespace dxvk {

  D3D9CommonShader::D3D9CommonShader() {}

  D3D9CommonShader::D3D9CommonShader(
            Direct3DDevice9Ex*    pDevice,
      const DxvkShaderKey*        pShaderKey,
      const DxsoModuleInfo*       pDxsoModuleInfo,
      const void*                 pShaderBytecode,
      const DxsoAnalysisInfo&     AnalysisInfo,
            DxsoModule*           pModule) {
    const uint32_t bytecodeLength = AnalysisInfo.bytecodeByteLength;
    m_bytecode.resize(bytecodeLength);
    std::memcpy(m_bytecode.data(), pShaderBytecode, bytecodeLength);

    const std::string name = pShaderKey->toString();
    Logger::debug(str::format("Compiling shader ", name));
    
    // If requested by the user, dump both the raw DXBC
    // shader and the compiled SPIR-V module to a file.
    const std::string dumpPath = env::getEnvVar("DXVK_SHADER_DUMP_PATH");
    
    if (dumpPath.size() != 0) {
      DxsoReader reader(
        reinterpret_cast<const char*>(pShaderBytecode));

      reader.store(std::ofstream(str::format(dumpPath, "/", name, ".dxso"),
        std::ios_base::binary | std::ios_base::trunc), bytecodeLength);

      char comment[2048];
      Com<ID3DBlob> blob;
      HRESULT hr = DisassembleShader(
        pShaderBytecode,
        TRUE,
        comment, 
        &blob);
      
      if (SUCCEEDED(hr)) {
        std::ofstream disassembledOut(str::format(dumpPath, "/", name, ".dxso.dis"), std::ios_base::binary | std::ios_base::trunc);
        disassembledOut.write(
          reinterpret_cast<const char*>(blob->GetBufferPointer()),
          blob->GetBufferSize());
      }
    }
    
    // Decide whether we need to create a pass-through
    // geometry shader for vertex shader stream output

    m_shader = pModule->compile(*pDxsoModuleInfo, name, AnalysisInfo);
    m_isgn   = pModule->isgn();
    m_shader->setShaderKey(*pShaderKey);
    
    if (dumpPath.size() != 0) {
      std::ofstream dumpStream(
        str::format(dumpPath, "/", name, ".spv"),
        std::ios_base::binary | std::ios_base::trunc);
      
      m_shader->dump(dumpStream);
    }

    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }

  D3D9CommonShader D3D9ShaderModuleSet::GetShaderModule(
            Direct3DDevice9Ex*    pDevice,
            VkShaderStageFlagBits ShaderStage,
      const DxsoModuleInfo*       pDxbcModuleInfo,
      const void*                 pShaderBytecode) {
    DxsoReader reader(
      reinterpret_cast<const char*>(pShaderBytecode));

    DxsoModule module(reader);
    DxsoAnalysisInfo info = module.analyze();

    Sha1Hash hash = Sha1Hash::compute(
      pShaderBytecode, info.bytecodeByteLength);

    DxvkShaderKey shaderKey = DxvkShaderKey(ShaderStage, hash);
    const DxvkShaderKey* pShaderKey = &shaderKey;

    // Use the shader's unique key for the lookup
    { std::unique_lock<std::mutex> lock(m_mutex);
      
      auto entry = m_modules.find(*pShaderKey);
      if (entry != m_modules.end())
        return entry->second;
    }
    
    // This shader has not been compiled yet, so we have to create a
    // new module. This takes a while, so we won't lock the structure.
    D3D9CommonShader commonShader(
      pDevice, pShaderKey,
      pDxbcModuleInfo, pShaderBytecode,
      info, &module);
    
    // Insert the new module into the lookup table. If another thread
    // has compiled the same shader in the meantime, we should return
    // that object instead and discard the newly created module.
    { std::unique_lock<std::mutex> lock(m_mutex);
      
      auto status = m_modules.insert({ *pShaderKey, commonShader });
      if (!status.second)
        return status.first->second;
    }
    
    return commonShader;
  }

}