#include "d3d11_device.h"
#include "d3d11_shader.h"

namespace dxvk {
  
  D3D11ShaderKey::D3D11ShaderKey(
          DxbcProgramType ProgramType,
    const void*           pShaderBytecode,
          size_t          BytecodeLength)
  : m_type(ProgramType),
    m_hash(Sha1Hash::compute(
      reinterpret_cast<const uint8_t*>(pShaderBytecode),
      BytecodeLength)) { }
  
  
  std::string D3D11ShaderKey::GetName() const {
    static const std::array<const char*, 6> s_prefix
      = {{ "PS_", "VS_", "GS_", "HS_", "DS_", "CS_" }};
    
    return str::format(
      s_prefix.at(uint32_t(m_type)),
      m_hash.toString());
  }
  
  
  size_t D3D11ShaderKey::GetHash() const {
    DxvkHashState result;
    result.add(uint32_t(m_type));
    
    const uint8_t* digest = m_hash.digest();
    for (uint32_t i = 0; i < 5; i++) {
      result.add(
          uint32_t(digest[4 + i + 0]) <<  0
        | uint32_t(digest[4 + i + 1]) <<  8
        | uint32_t(digest[4 + i + 2]) << 16
        | uint32_t(digest[4 + i + 3]) << 24);
    }
    
    return result;
  }
  
  
  D3D11ShaderModule:: D3D11ShaderModule() { }
  D3D11ShaderModule::~D3D11ShaderModule() { }
  
  
  D3D11ShaderModule::D3D11ShaderModule(
    const D3D11ShaderKey* pShaderKey,
    const DxbcOptions*    pDxbcOptions,
    const void*           pShaderBytecode,
          size_t          BytecodeLength)
  : m_name(pShaderKey->GetName()) {
    Logger::debug(str::format("Compiling shader ", m_name));
    
    DxbcReader reader(
      reinterpret_cast<const char*>(pShaderBytecode),
      BytecodeLength);
    
    DxbcModule module(reader);
    
    // If requested by the user, dump both the raw DXBC
    // shader and the compiled SPIR-V module to a file.
    const std::string dumpPath = env::getEnvVar(L"DXVK_SHADER_DUMP_PATH");
    const std::string readPath = env::getEnvVar(L"DXVK_SHADER_READ_PATH");
    
    if (dumpPath.size() != 0) {
      reader.store(std::ofstream(str::format(dumpPath, "/", m_name, ".dxbc"),
        std::ios_base::binary | std::ios_base::trunc));
    }
    
    m_shader = module.compile(*pDxbcOptions);
    m_shader->setDebugName(m_name);
    
    if (dumpPath.size() != 0) {
      std::ofstream dumpStream(
        str::format(dumpPath, "/", m_name, ".spv"),
        std::ios_base::binary | std::ios_base::trunc);
      
      m_shader->dump(dumpStream);
    }
    
    // If requested by the user, replace
    // the shader with another file.
    if (readPath.size() != 0) {
      // Check whether the file exists
      std::ifstream readStream(
        str::format(readPath, "/", m_name, ".spv"),
        std::ios_base::binary);
      
      if (readStream)
        m_shader->read(readStream);
    }
  }
  
  
  D3D11ShaderModuleSet:: D3D11ShaderModuleSet() { }
  D3D11ShaderModuleSet::~D3D11ShaderModuleSet() { }
  
  
  D3D11ShaderModule D3D11ShaderModuleSet::GetShaderModule(
    const DxbcOptions*    pDxbcOptions,
    const void*           pShaderBytecode,
          size_t          BytecodeLength,
          DxbcProgramType ProgramType) {
    // Compute the shader's unique key so that we can perform a lookup
    D3D11ShaderKey key(ProgramType, pShaderBytecode, BytecodeLength);
    
    { std::unique_lock<std::mutex> lock(m_mutex);
      
      auto entry = m_modules.find(key);
      if (entry != m_modules.end())
        return entry->second;
    }
    
    // This shader has not been compiled yet, so we have to create a
    // new module. This takes a while, so we won't lock the structure.
    D3D11ShaderModule module(&key, pDxbcOptions, pShaderBytecode, BytecodeLength);
    
    // Insert the new module into the lookup table. If another thread
    // has compiled the same shader in the meantime, we should return
    // that object instead and discard the newly created module.
    { std::unique_lock<std::mutex> lock(m_mutex);
      
      auto status = m_modules.insert({ key, module });
      if (!status.second)
        return status.first->second;
    }
    
    return module;
  }
  
}
