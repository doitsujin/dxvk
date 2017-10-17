#pragma once

#include "dxbc_capability.h"
#include "dxbc_chunk_shex.h"
#include "dxbc_entrypoint.h"
#include "dxbc_typeinfo.h"

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
    
    DxbcCapabilities    m_spvCapabilities;
    DxbcEntryPoint      m_spvEntryPoints;
    DxbcTypeInfo        m_spvTypeInfo;
    DxvkSpirvCodeBuffer m_spvCode;
    
    uint32_t m_entryPointId = 0;
    
  };
  
}