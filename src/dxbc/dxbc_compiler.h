#pragma once

#include "dxbc_chunk_shex.h"
#include "dxbc_names.h"

#include "../spirv/spirv_module.h"

namespace dxvk {
  
  struct DxbcRegTypeR {
    uint32_t varType;
    uint32_t ptrType;
    uint32_t varId;
  };
  
  
  struct DxbcValueType {
    spv::Op           componentType   = spv::OpTypeVoid;
    uint32_t          componentWidth  = 0;
    uint32_t          componentSigned = 0;
    uint32_t          componentCount  = 0;
  };
  
  
  struct DxbcValue {
    DxbcValueType type;
    uint32_t      typeId;
    uint32_t      valueId;
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
    
    std::vector<uint32_t>     m_interfaces;
    std::vector<DxbcRegTypeR> m_rRegs;
    
    uint32_t m_entryPointId = 0;
    
    uint32_t m_typeVoid     = 0;
    uint32_t m_typeFunction = 0;
    
    bool m_useRestrictedMath = false;
    
    
    void declareCapabilities();
    void declareMemoryModel();
    
    bool dclGlobalFlags(DxbcGlobalFlags flags);
    bool dclInput(const DxbcInstruction& ins);
    bool dclTemps(uint32_t n);
    
  };
  
}