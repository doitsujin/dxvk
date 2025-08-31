#include "d3d11_device.h"
#include "d3d11_shader.h"

namespace dxvk {
  
  D3D11CommonShader:: D3D11CommonShader() { }
  D3D11CommonShader::~D3D11CommonShader() { }
  
  
  D3D11CommonShader::D3D11CommonShader(
          D3D11Device*    pDevice,
    const DxvkShaderHash& ShaderKey,
    const DxbcModuleInfo* pDxbcModuleInfo,
    const void*           pShaderBytecode,
          size_t          BytecodeLength) {
    const std::string name = ShaderKey.toString();
    Logger::debug(str::format("Compiling shader ", name));
    
    DxbcReader reader(
      reinterpret_cast<const char*>(pShaderBytecode),
      BytecodeLength);
    
    // If requested by the user, dump both the raw DXBC
    // shader and the compiled SPIR-V module to a file.
    const std::string& dumpPath = pDevice->GetOptions()->shaderDumpPath;
    
    if (dumpPath.size() != 0) {
      reader.store(std::ofstream(str::topath(str::format(dumpPath, "/", name, ".dxbc").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc));
    }

    // Compute legacy SHA-1 hash to pass as shader name
    Sha1Hash sha1Hash = Sha1Hash::compute(
      pShaderBytecode, BytecodeLength);

    DxvkShaderKey legacyKey(ShaderKey.stage(), sha1Hash);

    // Error out if the shader is invalid
    DxbcModule module(reader);
    auto programInfo = module.programInfo();

    if (!programInfo)
      throw DxvkError("Invalid shader binary.");

    // Decide whether we need to create a pass-through
    // geometry shader for vertex shader stream output
    bool isPassthroughShader = pDxbcModuleInfo->xfb != nullptr
      && (programInfo->type() == DxbcProgramType::VertexShader
       || programInfo->type() == DxbcProgramType::DomainShader);

    if (programInfo->shaderStage() != ShaderKey.stage() && !isPassthroughShader)
      throw DxvkError("Mismatching shader type.");

    m_shader = isPassthroughShader
      ? module.compilePassthroughShader(*pDxbcModuleInfo, legacyKey.toString())
      : module.compile                 (*pDxbcModuleInfo, legacyKey.toString());
    
    if (dumpPath.size() != 0) {
      std::ofstream dumpStream(
        str::topath(str::format(dumpPath, "/", name, ".spv").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc);
      
      m_shader->dump(dumpStream);
    }
    
    // Create shader constant buffer if necessary
    auto icb = module.icbInfo();

    if (icb.size) {
      DxvkBufferCreateInfo info = { };
      info.size   = align(icb.size, 256u);
      info.usage  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                  | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                  | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      info.stages = util::pipelineStages(m_shader->metadata().stage);
      info.access = VK_ACCESS_UNIFORM_READ_BIT
                  | VK_ACCESS_TRANSFER_READ_BIT
                  | VK_ACCESS_TRANSFER_WRITE_BIT;
      info.debugName = "Icb";

      m_buffer = pDevice->GetDXVKDevice()->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      // Upload immediate constant buffer to VRAM
      pDevice->InitShaderIcb(this, icb.size, icb.data);
    }

    pDevice->GetDXVKDevice()->registerShader(m_shader);

    // Write back binding mask
    auto bindings = module.bindings();

    if (bindings)
      m_bindings = *bindings;
  }

  
  D3D11ShaderModuleSet:: D3D11ShaderModuleSet() { }
  D3D11ShaderModuleSet::~D3D11ShaderModuleSet() { }
  
  
  HRESULT D3D11ShaderModuleSet::GetShaderModule(
          D3D11Device*        pDevice,
    const DxvkShaderHash&     ShaderKey,
    const DxbcModuleInfo*     pDxbcModuleInfo,
    const void*               pShaderBytecode,
          size_t              BytecodeLength,
          D3D11CommonShader*  pShader) {
    // Use the shader's unique key for the lookup
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto entry = m_modules.find(ShaderKey);
      if (entry != m_modules.end()) {
        *pShader = entry->second;
        return S_OK;
      }
    }
    
    // This shader has not been compiled yet, so we have to create a
    // new module. This takes a while, so we won't lock the structure.
    D3D11CommonShader module;
    
    try {
      module = D3D11CommonShader(pDevice, ShaderKey,
        pDxbcModuleInfo, pShaderBytecode, BytecodeLength);
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
    
    // Insert the new module into the lookup table. If another thread
    // has compiled the same shader in the meantime, we should return
    // that object instead and discard the newly created module.
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto status = m_modules.insert({ ShaderKey, module });

      if (!status.second) {
        *pShader = status.first->second;
        return S_OK;
      }
    }
    
    *pShader = std::move(module);
    return S_OK;
  }

}
