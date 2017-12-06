#include "d3d11_shader.h"

namespace dxvk {
  
  static std::string GetEnvVar(LPCWSTR name) {
    DWORD len = ::GetEnvironmentVariableW(name, nullptr, 0);
    
    std::wstring result;
    
    while (len > result.size()) {
      result.resize(len);
      len = ::GetEnvironmentVariableW(
        name, result.data(), result.size());
    }
    
    result.resize(len);
    return str::fromws(result);
  }
  
  
  D3D11ShaderModule:: D3D11ShaderModule() { }
  D3D11ShaderModule::~D3D11ShaderModule() { }
  
  
  D3D11ShaderModule::D3D11ShaderModule(
    const void*   pShaderBytecode,
          size_t  BytecodeLength) {
    
    DxbcReader reader(
      reinterpret_cast<const char*>(pShaderBytecode),
      BytecodeLength);
    
    DxbcModule module(reader);
    m_code = module.compile();
    
    // TODO pre-process shader bindings
    
    // If requested by the user, dump both the raw DXBC
    // shader and the compiled SPIR-V module to a file.
    const std::string dumpPath = GetEnvVar(L"DXVK_SHADER_DUMP_PATH");
    
    if (dumpPath.size() != 0) {
      const std::string baseName = str::format(dumpPath, "/",
        ConstructFileName(ComputeShaderHash(pShaderBytecode, BytecodeLength),
        module.version().type()));
      
      reader.store(std::ofstream(str::format(baseName, ".dxbc"),
        std::ios_base::binary | std::ios_base::trunc));
      
      m_code.store(std::ofstream(str::format(baseName, ".spv"),
        std::ios_base::binary | std::ios_base::trunc));
    }
  }
  
  
  Sha1Hash D3D11ShaderModule::ComputeShaderHash(
    const void*   pShaderBytecode,
          size_t  BytecodeLength) const {
    return Sha1Hash::compute(
      reinterpret_cast<const uint8_t*>(pShaderBytecode),
      BytecodeLength);
  }
  
  
  std::string D3D11ShaderModule::ConstructFileName(
    const Sha1Hash&         hash,
    const DxbcProgramType&  type) const {
    
    std::string typeStr;
    
    switch (type) {
      case DxbcProgramType::PixelShader:    typeStr = "PS_"; break;
      case DxbcProgramType::VertexShader:   typeStr = "VS_"; break;
      case DxbcProgramType::GeometryShader: typeStr = "GS_"; break;
      case DxbcProgramType::HullShader:     typeStr = "HS_"; break;
      case DxbcProgramType::DomainShader:   typeStr = "DS_"; break;
      case DxbcProgramType::ComputeShader:  typeStr = "CS_"; break;
    }
    
    return str::format(typeStr, hash.toString());
  }
  
}
