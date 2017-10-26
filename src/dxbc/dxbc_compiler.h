#pragma once

#include "dxbc_chunk_shex.h"

#include "../spirv/spirv_module.h"

namespace dxvk {
  
  struct DxbcRegTypeR {
    uint32_t varType;
    uint32_t ptrType;
    uint32_t varId;
  };
  
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
    
    std::vector<DxbcRegTypeR> m_rRegs;
    
    uint32_t m_entryPointId = 0;
    
    uint32_t m_typeVoid     = 0;
    uint32_t m_typeFunction = 0;
    
    void declareCapabilities();
    void declareMemoryModel();
    
    void dclTemps(uint32_t n);
    
  };
  
}