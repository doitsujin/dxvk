#pragma once

#include <unordered_set>

#include "../dxvk/dxvk_shader.h"

#include "dxbc_chunk_shex.h"

namespace dxvk {
  
  /**
   * \brief DXBC to SPIR-V compiler
   * 
   * 
   */
  class DxbcCompiler {
    
  public:
    
    DxbcCompiler(DxbcProgramVersion version);
    ~DxbcCompiler();
    
    /**
     * \brief Processes a single instruction
     * \param [in] ins The instruction
     */
    void processInstruction(DxbcInstruction ins);
    
    /**
     * \brief Creates actual shader object
     * 
     * Combines all information gatherd during the
     * shader compilation into one shader object.
     */
    Rc<DxvkShader> finalize();
    
  private:
    
    DxbcProgramVersion  m_version;
    DxvkSpirvIdCounter  m_counter;
    
    std::unordered_set<spv::Capability> m_capabilities;
    
    DxvkSpirvCodeBuffer m_spirvCapabilities;
    DxvkSpirvCodeBuffer m_spirvProgramCode;
    
    VkShaderStageFlagBits shaderStage() const;
    
    void enableCapability(spv::Capability cap);
    
  };
  
}