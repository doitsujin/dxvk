#pragma once

#include "dxbc_chunk_isgn.h"
#include "dxbc_chunk_shex.h"
#include "dxbc_names.h"
#include "dxbc_type.h"

#include "../spirv/spirv_module.h"

namespace dxvk {
  
  /**
   * \brief DXBC to SPIR-V compiler
   */
  class DxbcCompiler {
    
  public:
    
    DxbcCompiler(
            DxbcProgramVersion  version,
      const Rc<DxbcIsgn>&       inputSig,
      const Rc<DxbcIsgn>&       outputSig);
    ~DxbcCompiler();
    
    DxbcCompiler             (DxbcCompiler&&) = delete;
    DxbcCompiler& operator = (DxbcCompiler&&) = delete;
    
    /**
     * \brief Processes a single instruction
     * 
     * \param [in] ins The instruction
     * \returns \c true on success
     */
    void processInstruction(
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
    
    Rc<DxbcIsgn>        m_inputSig;
    Rc<DxbcIsgn>        m_outputSig;
    
    std::vector<uint32_t>     m_interfaces;
    std::vector<DxbcPointer>  m_rRegs;  // Temps
    
    DxbcPointer               m_svPosition;
    std::vector<DxbcPointer>  m_svClipDistance;
    std::vector<DxbcPointer>  m_svCullDistance;
    
    uint32_t m_entryPointId = 0;
    
    uint32_t m_typeVoid     = 0;
    uint32_t m_typeFunction = 0;
    
    bool m_useRestrictedMath = false;
    
    
    
    void declareCapabilities();
    void declareMemoryModel();
    
    void dclGlobalFlags(
      const DxbcInstruction& ins);
    
    void dclInput(
      const DxbcInstruction& ins);
    
    void dclOutputSiv(
      const DxbcInstruction& ins);
    
    void dclTemps(
      const DxbcInstruction& ins);
    
    void dclThreadGroup(
      const DxbcInstruction& ins);
    
    
    void opMov(
      const DxbcInstruction& ins);
    
    void opRet(
      const DxbcInstruction& ins);
    
    uint32_t getScalarTypeId(
      const DxbcScalarType& type);
    
    uint32_t getValueTypeId(
      const DxbcValueType& type);
    
    uint32_t getPointerTypeId(
      const DxbcPointerType& type);
    
    
    DxbcValue loadPointer(
      const DxbcPointer&        pointer);
    
    DxbcValue loadOperand(
      const DxbcOperand&        operand,
      const DxbcValueType&      type);
    
    
    void storePointer(
      const DxbcPointer&        pointer,
      const DxbcValue&          value);
    
    void storeOperand(
      const DxbcOperand&        operand,
      const DxbcValueType&      srcType,
            uint32_t            srcValue);
    
  };
  
}