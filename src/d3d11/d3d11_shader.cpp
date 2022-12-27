#include "d3d11_device.h"
#include "d3d11_shader.h"

namespace dxvk {
  
  D3D11CommonShader:: D3D11CommonShader() { }
  D3D11CommonShader::~D3D11CommonShader() { }
  
  
  D3D11CommonShader::D3D11CommonShader(
          D3D11Device*    pDevice,
    const DxvkShaderKey*  pShaderKey,
    const DxbcModuleInfo* pDxbcModuleInfo,
    const void*           pShaderBytecode,
          size_t          BytecodeLength) {
    const std::string name = pShaderKey->toString();
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

    // Error out if the shader is invalid
    DxbcModule module(reader);
    auto programInfo = module.programInfo();

    if (!programInfo)
      throw DxvkError("Invalid shader binary.");

    // Decide whether we need to create a pass-through
    // geometry shader for vertex shader stream output
    bool passthroughShader = pDxbcModuleInfo->xfb != nullptr
      && (programInfo->type() == DxbcProgramType::VertexShader
       || programInfo->type() == DxbcProgramType::DomainShader);

    if (programInfo->shaderStage() != pShaderKey->type() && !passthroughShader)
      throw DxvkError("Mismatching shader type.");

    m_shader = passthroughShader
      ? module.compilePassthroughShader(*pDxbcModuleInfo, name)
      : module.compile                 (*pDxbcModuleInfo, name);
    m_shader->setShaderKey(*pShaderKey);
    
    if (dumpPath.size() != 0) {
      std::ofstream dumpStream(
        str::topath(str::format(dumpPath, "/", name, ".spv").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc);
      
      m_shader->dump(dumpStream);
    }
    
    // Create shader constant buffer if necessary
    const DxvkShaderCreateInfo& shaderInfo = m_shader->info();

    if (shaderInfo.uniformSize) {
      DxvkBufferCreateInfo info;
      info.size   = shaderInfo.uniformSize;
      info.usage  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      info.stages = util::pipelineStages(shaderInfo.stage);
      info.access = VK_ACCESS_UNIFORM_READ_BIT;
      
      VkMemoryPropertyFlags memFlags
        = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      
      m_buffer = pDevice->GetDXVKDevice()->createBuffer(info, memFlags);
      std::memcpy(m_buffer->mapPtr(0), shaderInfo.uniformData, shaderInfo.uniformSize);
    }

    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }

  
  D3D11ShaderModuleSet:: D3D11ShaderModuleSet() { }
  D3D11ShaderModuleSet::~D3D11ShaderModuleSet() { }
  
  
  HRESULT D3D11ShaderModuleSet::GetShaderModule(
          D3D11Device*        pDevice,
    const DxvkShaderKey*      pShaderKey,
    const DxbcModuleInfo*     pDxbcModuleInfo,
    const void*               pShaderBytecode,
          size_t              BytecodeLength,
          D3D11CommonShader*  pShader) {
    // Use the shader's unique key for the lookup
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto entry = m_modules.find(*pShaderKey);
      if (entry != m_modules.end()) {
        *pShader = entry->second;
        return S_OK;
      }
    }
    
    // This shader has not been compiled yet, so we have to create a
    // new module. This takes a while, so we won't lock the structure.
    D3D11CommonShader module;
    
    try {
      module = D3D11CommonShader(pDevice, pShaderKey,
        pDxbcModuleInfo, pShaderBytecode, BytecodeLength);
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
    
    // Insert the new module into the lookup table. If another thread
    // has compiled the same shader in the meantime, we should return
    // that object instead and discard the newly created module.
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto status = m_modules.insert({ *pShaderKey, module });
      if (!status.second) {
        *pShader = status.first->second;
        return S_OK;
      }
    }
    
    *pShader = std::move(module);
    return S_OK;
  }
  

  D3D11ExtShader::D3D11ExtShader(
          ID3D11DeviceChild*      pParent,
          D3D11CommonShader*      pShader)
  : m_parent(pParent), m_shader(pShader) {

  }


  D3D11ExtShader::~D3D11ExtShader() {

  }


  ULONG STDMETHODCALLTYPE D3D11ExtShader::AddRef() {
    return m_parent->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D11ExtShader::Release() {
    return m_parent->Release();
  }


  HRESULT STDMETHODCALLTYPE D3D11ExtShader::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_parent->QueryInterface(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11ExtShader::GetSpirvCode(
          SIZE_T*                 pCodeSize,
          void*                   pCode) {
    auto shader = m_shader->GetShader();
    auto code = shader->getRawCode();

    HRESULT hr = S_OK;

    if (pCode) {
      size_t size = code.size();

      if (size > *pCodeSize) {
        size = *pCodeSize;
        hr = S_FALSE;
      }

      std::memcpy(pCode, code.data(), size);
      *pCodeSize = size;
      return hr;
    } else {
      *pCodeSize = code.size();
      return hr;
    }
  }

}
