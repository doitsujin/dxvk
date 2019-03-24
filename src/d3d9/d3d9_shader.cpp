#include "d3d9_shader.h"

namespace dxvk {

  D3D9CommonShader::D3D9CommonShader() {}

  D3D9CommonShader::D3D9CommonShader(
            Direct3DDevice9Ex*    pDevice,
      const DxvkShaderKey*        pShaderKey,
      const DxsoModuleInfo*       pDxsoModuleInfo,
      const void*                 pShaderBytecode,
            size_t                BytecodeLength) {
    m_bytecode.resize(BytecodeLength);
    std::memcpy(m_bytecode.data(), pShaderBytecode, BytecodeLength);

    const std::string name = pShaderKey->toString();
    Logger::debug(str::format("Compiling shader ", name));
    
    DxsoReader reader(
      reinterpret_cast<const char*>(pShaderBytecode),
      BytecodeLength);
    
    DxsoModule module(reader);
    
    // If requested by the user, dump both the raw DXBC
    // shader and the compiled SPIR-V module to a file.
    const std::string dumpPath = env::getEnvVar("DXVK_SHADER_DUMP_PATH");
    
    if (dumpPath.size() != 0) {
      reader.store(std::ofstream(str::format(dumpPath, "/", name, ".dxso"),
        std::ios_base::binary | std::ios_base::trunc));
    }
    
    // Decide whether we need to create a pass-through
    // geometry shader for vertex shader stream output

    m_shader = module.compile(*pDxsoModuleInfo, name);
    m_decl = module.getDecls();
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
            Direct3DDevice9Ex* pDevice,
      const DxvkShaderKey*     pShaderKey,
      const DxsoModuleInfo*    pDxbcModuleInfo,
      const void*              pShaderBytecode,
            size_t             BytecodeLength) {
    // Use the shader's unique key for the lookup
    { std::unique_lock<std::mutex> lock(m_mutex);
      
      auto entry = m_modules.find(*pShaderKey);
      if (entry != m_modules.end())
        return entry->second;
    }
    
    // This shader has not been compiled yet, so we have to create a
    // new module. This takes a while, so we won't lock the structure.
    D3D9CommonShader module(pDevice, pShaderKey,
      pDxbcModuleInfo, pShaderBytecode, BytecodeLength);
    
    // Insert the new module into the lookup table. If another thread
    // has compiled the same shader in the meantime, we should return
    // that object instead and discard the newly created module.
    { std::unique_lock<std::mutex> lock(m_mutex);
      
      auto status = m_modules.insert({ *pShaderKey, module });
      if (!status.second)
        return status.first->second;
    }
    
    return module;
  }

}