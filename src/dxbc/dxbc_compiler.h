#pragma once

#include "dxbc_chunk_shex.h"

#include "../spirv/spirv_module.h"

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
    
    DxbcCompiler             (DxbcCompiler&&) = delete;
    DxbcCompiler& operator = (DxbcCompiler&&) = delete;
    
    /**
     * \brief Processes a single instruction
     * 
     * \param [in] ins The instruction
     * \returns \c true on success
     */
    bool processInstruction(
      const DxbcInstruction& ins);
    
    /**
     * \brief Creates actual shader object
     * 
     * Combines all information gatherd during the
     * shader compilation into one shader object.
     */
    Rc<DxvkShader> finalize();
    
  private:
    
    DxbcProgramVersion  m_version;
    SpirvModule         m_module;
    
    uint32_t m_entryPointId = 0;
    
    void declareCapabilities();
    void declareMemoryModel();
    
  };
  
}