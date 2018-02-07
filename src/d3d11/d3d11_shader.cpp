#include "d3d11_device.h"
#include "d3d11_shader.h"

namespace dxvk {
  
  D3D11ShaderModule:: D3D11ShaderModule() { }
  D3D11ShaderModule::~D3D11ShaderModule() { }
  
  
  D3D11ShaderModule::D3D11ShaderModule(
    const DxbcOptions*  pDxbcOptions,
          D3D11Device*  pDevice,
    const void*         pShaderBytecode,
          size_t        BytecodeLength) {
    DxbcReader reader(
      reinterpret_cast<const char*>(pShaderBytecode),
      BytecodeLength);
    
    DxbcModule module(reader);
    
    // Construct the shader name that we'll use for
    // debug messages and as the dump/read file name
    m_name = ConstructFileName(
      ComputeShaderHash(pShaderBytecode, BytecodeLength),
      module.version().type());
    
    Logger::debug(str::format("Compiling shader ", m_name));
    
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
      m_shader->dump(std::ofstream(str::format(dumpPath, "/", m_name, ".spv"),
        std::ios_base::binary | std::ios_base::trunc));
    }
    
    // If requested by the user, replace
    // the shader with another file.
    if (readPath.size() != 0) {
      // Check whether the file exists
      std::ifstream readStream(
        str::format(readPath, "/", m_name, ".spv"),
        std::ios_base::binary);
      
      if (readStream)
        m_shader->read(std::move(readStream));
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
