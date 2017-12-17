#pragma once

#include <array>
#include <vector>

#include "../spirv/spirv_module.h"

#include "dxbc_chunk_isgn.h"
#include "dxbc_decoder_2.h"
#include "dxbc_defs.h"
#include "dxbc_names.h"
#include "dxbc_util.h"

namespace dxvk {
  
  struct DxbcVectorType {
    DxbcScalarType    ctype;
    uint32_t          ccount;
  };
  
  struct DxbcRegisterInfo {
    DxbcVectorType    type;
    spv::StorageClass sclass;
  };
  
  struct DxbcRegisterValue {
    DxbcVectorType    type;
    uint32_t          id;
  };
  
  struct DxbcRegisterPointer {
    DxbcVectorType    type;
    uint32_t          id;
  };
  
  struct DxbcCompilerVsPart {
    uint32_t functionId;
  };
  
  struct DxbcCompilerPsPart {
    uint32_t functionId;
    std::array<DxbcVectorType, DxbcMaxInterfaceRegs> oTypes;
  };
  
  /**
   * \brief DXBC to SPIR-V shader compiler
   * 
   * Processes instructions from a DXBC shader and creates
   * a DXVK shader object, which contains the SPIR-V module
   * and information about the shader resource bindings.
   */
  class DxbcCompiler2 {
    
  public:
    
    DxbcCompiler2(
      const DxbcProgramVersion& version,
      const Rc<DxbcIsgn>&       isgn,
      const Rc<DxbcIsgn>&       osgn);
    ~DxbcCompiler2();
    
    /**
     * \brief Processes a single instruction
     * \param [in] ins The instruction
     */
    void processInstruction(
      const DxbcShaderInstruction&  ins);
    
    /**
     * \brief Finalizes the shader
     * \returns The final shader object
     */
    Rc<DxvkShader> finalize();
    
  private:
    
    DxbcProgramVersion  m_version;
    SpirvModule         m_module;
    
    Rc<DxbcIsgn>        m_isgn;
    Rc<DxbcIsgn>        m_osgn;
    
    ///////////////////////////////////////////////////////
    // Resource slot description for the shader. This will
    // be used to map D3D11 bindings to DXVK bindings.
    std::vector<DxvkResourceSlot> m_resourceSlots;
    
    ///////////////////////////////
    // r# registers of type float4
    std::vector<uint32_t> m_rRegs;
    
    ///////////////////////////////////////////////////////////
    // v# registers as defined by the shader. The type of each
    // of these inputs is either float4 or an array of float4.
    std::array<uint32_t, DxbcMaxInterfaceRegs> m_vRegs;
    std::vector<DxbcSvMapping>                 m_vMappings;
    
    //////////////////////////////////////////////////////////
    // o# registers as defined by the shader. In the fragment
    // shader stage, these registers are typed by the signature,
    // in all other stages, they are float4 registers or arrays.
    std::array<uint32_t, DxbcMaxInterfaceRegs> m_oRegs;
    std::vector<DxbcSvMapping>                 m_oMappings;
    
    //////////////////////////////////////////////////////
    // Shader resource variables. These provide access to
    // constant buffers, samplers, textures, and UAVs.
    std::array<DxbcConstantBuffer,  16> m_constantBuffers;
    std::array<DxbcSampler,         16> m_samplers;
    std::array<DxbcShaderResource, 128> m_textures;
    
    ///////////////////////////////////////////////////////////
    // Array of input values. Since v# registers are indexable
    // in DXBC, we need to copy them into an array first.
    uint32_t m_vArray = 0;
    
    ////////////////////////////////////////////////////
    // Per-vertex input and output blocks. Depending on
    // the shader stage, these may be declared as arrays.
    uint32_t m_perVertexIn  = 0;
    uint32_t m_perVertexOut = 0;
    
    ///////////////////////////////////////////////////
    // Entry point description - we'll need to declare
    // the function ID and all input/output variables.
    std::vector<uint32_t> m_entryPointInterfaces;
    uint32_t              m_entryPointId = 0;
    
    ///////////////////////////////////
    // Shader-specific data structures
    DxbcCompilerVsPart m_vs;
    DxbcCompilerPsPart m_ps;
    
    /////////////////////////////////////////////////////
    // Shader interface and metadata declaration methods
    void emitDclGlobalFlags(
      const DxbcShaderInstruction& ins);
    
    void emitDclTemps(
      const DxbcShaderInstruction& ins);
    
    void emitDclInterfaceReg(
      const DxbcShaderInstruction& ins);
    
    void emitDclInput(
            uint32_t                regIdx,
            uint32_t                regDim,
            DxbcRegMask             regMask,
            DxbcSystemValue         sv,
            DxbcInterpolationMode   im);
    
    void emitDclOutput(
            uint32_t                regIdx,
            uint32_t                regDim,
            DxbcRegMask             regMask,
            DxbcSystemValue         sv,
            DxbcInterpolationMode   im);
    
    void emitDclConstantBuffer(
      const DxbcShaderInstruction&  ins);
    
    void emitDclSampler(
      const DxbcShaderInstruction&  ins);
    
    void emitDclResource(
      const DxbcShaderInstruction&  ins);
    
    //////////////////////////////
    // Instruction class handlers
    void emitVectorAlu(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorCmov(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorCmp(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorDot(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorImul(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorSinCos(
      const DxbcShaderInstruction&  ins);
    
    void emitSample(
      const DxbcShaderInstruction&  ins);
    
    void emitRet(
      const DxbcShaderInstruction&  ins);
    
    
    /////////////////////////////////////////
    // Generic register manipulation methods
    DxbcRegisterValue emitRegisterBitcast(
            DxbcRegisterValue       srcValue,
            DxbcScalarType          dstType);
    
    DxbcRegisterValue emitRegisterSwizzle(
            DxbcRegisterValue       value,
            DxbcRegSwizzle          swizzle,
            DxbcRegMask             writeMask);
    
    DxbcRegisterValue emitRegisterExtract(
            DxbcRegisterValue       value,
            DxbcRegMask             mask);
    
    DxbcRegisterValue emitRegisterInsert(
            DxbcRegisterValue       dstValue,
            DxbcRegisterValue       srcValue,
            DxbcRegMask             srcMask);
    
    DxbcRegisterValue emitRegisterExtend(
            DxbcRegisterValue       value,
            uint32_t                size);
    
    DxbcRegisterValue emitRegisterAbsolute(
            DxbcRegisterValue       value);
    
    DxbcRegisterValue emitRegisterNegate(
            DxbcRegisterValue       value);
    
    DxbcRegisterValue emitSrcOperandModifiers(
            DxbcRegisterValue       value,
            DxbcRegModifiers        modifiers);
    
    DxbcRegisterValue emitDstOperandModifiers(
            DxbcRegisterValue       value,
            DxbcOpModifiers         modifiers);
    
    ////////////////////////
    // Address load methods
    DxbcRegisterPointer emitGetTempPtr(
      const DxbcRegister&           operand);
    
    DxbcRegisterPointer emitGetInputPtr(
      const DxbcRegister&           operand);
    
    DxbcRegisterPointer emitGetOutputPtr(
      const DxbcRegister&           operand);
    
    DxbcRegisterPointer emitGetConstBufPtr(
      const DxbcRegister&           operand);
    
    DxbcRegisterPointer emitGetOperandPtr(
      const DxbcRegister&           operand);
    
    //////////////////////////////
    // Operand load/store methods
    DxbcRegisterValue emitIndexLoad(
            DxbcRegIndex            index);
    
    DxbcRegisterValue emitValueLoad(
            DxbcRegisterPointer     ptr);
    
    void emitValueStore(
            DxbcRegisterPointer     ptr,
            DxbcRegisterValue       value,
            DxbcRegMask             writeMask);
    
    DxbcRegisterValue emitRegisterLoad(
      const DxbcRegister&           reg,
            DxbcRegMask             writeMask);
    
    void emitRegisterStore(
      const DxbcRegister&           reg,
            DxbcRegisterValue       value);
    
    /////////////////////////////
    // Input preparation methods
    void emitVsInputSetup();
    void emitPsInputSetup();
    
    //////////////////////////////
    // Output preparation methods
    void emitVsOutputSetup();
    void emitPsOutputSetup();
    
    /////////////////////////////////
    // Shader initialization methods
    void emitVsInit();
    void emitPsInit();
    
    ///////////////////////////////
    // Shader finalization methods
    void emitVsFinalize();
    void emitPsFinalize();
    
    ///////////////////////////////
    // Variable definition methods
    uint32_t emitNewVariable(
      const DxbcRegisterInfo& info);
    
    ///////////////////////////
    // Type definition methods
    uint32_t getScalarTypeId(
            DxbcScalarType type);
    
    uint32_t getVectorTypeId(
      const DxbcVectorType& type);
    
    uint32_t getPointerTypeId(
      const DxbcRegisterInfo& type);
    
    uint32_t getPerVertexBlockId();
    
  };
  
}