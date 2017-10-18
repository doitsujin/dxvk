#pragma once

#include "dxbc_chunk_shex.h"

#include "../spirv/gen/spirv_gen_capability.h"
#include "../spirv/gen/spirv_gen_entrypoint.h"
#include "../spirv/gen/spirv_gen_typeinfo.h"

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
    SpirvIdCounter      m_counter;
    
    SpirvCapabilities   m_spvCapabilities;
    SpirvEntryPoint     m_spvEntryPoints;
    SpirvTypeInfo       m_spvTypeInfo;
    SpirvCodeBuffer     m_spvCode;
    
    void declareCapabilities();
    void declareMemoryModel();
    
  };
  
}