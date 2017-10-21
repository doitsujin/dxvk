#pragma once

#include "dxbc_chunk_shex.h"

#include "../spirv/gen/spirv_gen_capability.h"
#include "../spirv/gen/spirv_gen_constant.h"
#include "../spirv/gen/spirv_gen_debuginfo.h"
#include "../spirv/gen/spirv_gen_decoration.h"
#include "../spirv/gen/spirv_gen_entrypoint.h"
#include "../spirv/gen/spirv_gen_typeinfo.h"
#include "../spirv/gen/spirv_gen_variable.h"

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
    bool processInstruction(DxbcInstruction ins);
    
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
    SpirvDebugInfo      m_spvDebugInfo;
    SpirvDecorations    m_spvDecorations;
    SpirvTypeInfo       m_spvTypeInfo;
    SpirvConstants      m_spvConstants;
    SpirvVariables      m_spvVariables;
    SpirvCodeBuffer     m_spvCode;
    
    uint32_t m_entryPointId = 0;
    
    bool handleDcl(DxbcInstruction ins);
    
    void declareCapabilities();
    void declareMemoryModel();
    
  };
  
}