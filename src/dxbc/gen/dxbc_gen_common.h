#pragma once

#include "../dxbc_common.h"
#include "../dxbc_decoder.h"
#include "../dxbc_type.h"

#include "../../spirv/spirv_module.h"

namespace dxvk {
  
  /**
   * \brief System value mapping
   * 
   * Maps a system value to a given set of
   * components of an input or output register.
   */
  struct DxbcSvMapping {
    uint32_t          regId;
    DxbcComponentMask regMask;
    DxbcSystemValue   sv;
  };
  
  
  /**
   * \brief DXBC code generator
   * 
   * SPIR-V code generator. Implements simple micro ops that are
   * generated when parsing the DXBC shader code. Some of these
   * may require different implementations for each shader stage
   * and are therefore implemented in a sub class.
   */
  class DxbcCodeGen : public RcObject {
    
  public:
    
    DxbcCodeGen();
    
    virtual ~DxbcCodeGen();
    
    /**
     * \brief Declares temporary registers
     * \param [in] n Number of temp registers
     */
    void dclTemps(uint32_t n);
    
    /**
     * \brief Declares an interface variable
     * 
     * \param [in] regType Register type
     * \param [in] regId Interface register index
     * \param [in] regDim Array dimension of interface variable
     * \param [in] regMask Component mask for this declaration
     * \param [in] sv System value to map to the given components
     */
    virtual void dclInterfaceVar(
            DxbcOperandType   regType,
            uint32_t          regId,
            uint32_t          regDim,
            DxbcComponentMask regMask,
            DxbcSystemValue   sv) = 0;
    
    /**
     * \brief Finalizes shader
     * 
     * Depending on the shader stage, this may generate
     * additional code to set up input variables, output
     * variables, and execute shader phases.
     * \returns DXVK shader module
     */
    virtual Rc<DxvkShader> finalize() = 0;
    
    /**
     * \brief Creates code generator for a given program type
     * 
     * \param [in] version Program version
     * \returns The code generator
     */
    static Rc<DxbcCodeGen> create(
      const DxbcProgramVersion& version);
    
  protected:
    
    constexpr static uint32_t PerVertex_Position  = 0;
    constexpr static uint32_t PerVertex_PointSize = 1;
    constexpr static uint32_t PerVertex_CullDist  = 2;
    constexpr static uint32_t PerVertex_ClipDist  = 3;
    
    SpirvModule m_module;
    
    std::vector<uint32_t> m_entryPointInterfaces;
    uint32_t              m_entryPointId = 0;
    
    std::vector<DxbcPointer> m_rRegs;
    
    uint32_t defScalarType(
            DxbcScalarType          type);
    
    uint32_t defValueType(
      const DxbcValueType&          type);
    
    uint32_t defPointerType(
      const DxbcPointerType&        type);
    
    uint32_t defPerVertexBlock();
    
  };
  
}