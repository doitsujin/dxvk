#pragma once

#include <array>
#include <vector>

#include "../spirv/spirv_module.h"

#include "dxbc_chunk_isgn.h"
#include "dxbc_decoder.h"
#include "dxbc_defs.h"
#include "dxbc_names.h"
#include "dxbc_util.h"

namespace dxvk {
  
  /**
   * \brief Vector type
   * 
   * Convenience struct that stores a scalar
   * type and a component count. The compiler
   * can use this to generate SPIR-V types.
   */
  struct DxbcVectorType {
    DxbcScalarType    ctype;
    uint32_t          ccount;
  };
  
  
  /**
   * \brief Array type
   * 
   * Convenience struct that stores a scalar type, a
   * component count and an array size. An array of
   * length 0 will be evaluated to a vector type. The
   * compiler can use this to generate SPIR-V types.
   */
  struct DxbcArrayType {
    DxbcScalarType    ctype;
    uint32_t          ccount;
    uint32_t          alength;
  };
  
  
  /**
   * \brief Register info
   * 
   * Stores the array type of a register and
   * its storage class. The compiler can use
   * this to generate SPIR-V pointer types.
   */
  struct DxbcRegisterInfo {
    DxbcArrayType     type;
    spv::StorageClass sclass;
  };
  
  
  /**
   * \brief Register value
   * 
   * Stores a vector type and a SPIR-V ID that
   * represents an intermediate value. This is
   * used to track the type of such values.
   */
  struct DxbcRegisterValue {
    DxbcVectorType    type;
    uint32_t          id;
  };
  
  
  /**
   * \brief Register pointer
   * 
   * Stores a vector type and a SPIR-V ID that
   * represents a pointer to such a vector. This
   * can be used to load registers conveniently.
   */
  struct DxbcRegisterPointer {
    DxbcVectorType    type;
    uint32_t          id;
  };
  
  
  struct DxbcXreg {
    uint32_t ccount = 0;
    uint32_t varId  = 0;
  };
  
  
  /**
   * \brief Vertex shader-specific structure
   */
  struct DxbcCompilerVsPart {
    uint32_t functionId = 0;
  };
  
  
  /**
   * \brief Geometry shader-specific structure
   */
  struct DxbcCompilerGsPart {
    DxbcPrimitive         inputPrimitive      = DxbcPrimitive::Undefined;
    DxbcPrimitiveTopology outputTopology      = DxbcPrimitiveTopology::Undefined;
    uint32_t              outputVertexCount   = 0;
    uint32_t              functionId          = 0;
  };
  
  
  /**
   * \brief Pixel shader-specific structure
   */
  struct DxbcCompilerPsPart {
    uint32_t functionId = 0;
    
    std::array<DxbcVectorType, DxbcMaxInterfaceRegs> oTypes;
  };
  
  
  enum class DxbcCfgBlockType : uint32_t {
    If, Loop,
  };
  
  
  struct DxbcCfgBlockIf {
    uint32_t labelIf;
    uint32_t labelElse;
    uint32_t labelEnd;
    bool     hadElse;
  };
  
  
  struct DxbcCfgBlockLoop {
    uint32_t labelHeader;
    uint32_t labelBegin;
    uint32_t labelContinue;
    uint32_t labelBreak;
  };
  
  
  struct DxbcCfgBlock {
    DxbcCfgBlockType type;
    
    union {
      DxbcCfgBlockIf   b_if;
      DxbcCfgBlockLoop b_loop;
    };
  };
  
  
  /**
   * \brief DXBC to SPIR-V shader compiler
   * 
   * Processes instructions from a DXBC shader and creates
   * a DXVK shader object, which contains the SPIR-V module
   * and information about the shader resource bindings.
   */
  class DxbcCompiler {
    
  public:
    
    DxbcCompiler(
      const DxbcProgramVersion& version,
      const Rc<DxbcIsgn>&       isgn,
      const Rc<DxbcIsgn>&       osgn);
    ~DxbcCompiler();
    
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
    
    ////////////////////////////////////////////////
    // Temporary r# vector registers with immediate
    // indexing, and x# vector array registers.
    std::vector<uint32_t> m_rRegs;
    std::vector<DxbcXreg> m_xRegs;
    
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
    
    ///////////////////////////////////////////////
    // Control flow information. Stores labels for
    // currently active if-else blocks and loops.
    std::vector<DxbcCfgBlock> m_controlFlowBlocks;
    
    ///////////////////////////////////////////////////////////
    // Array of input values. Since v# registers are indexable
    // in DXBC, we need to copy them into an array first.
    uint32_t m_vArray = 0;
    
    ////////////////////////////////////////////////////
    // Per-vertex input and output blocks. Depending on
    // the shader stage, these may be declared as arrays.
    uint32_t m_perVertexIn  = 0;
    uint32_t m_perVertexOut = 0;
    
    //////////////////////////////////////////////////
    // Immediate constant buffer. If defined, this is
    // an array of four-component uint32 vectors.
    uint32_t m_immConstBuf = 0;
    
    ///////////////////////////////////////////////////
    // Entry point description - we'll need to declare
    // the function ID and all input/output variables.
    std::vector<uint32_t> m_entryPointInterfaces;
    uint32_t              m_entryPointId = 0;
    
    ///////////////////////////////////
    // Shader-specific data structures
    DxbcCompilerVsPart m_vs;
    DxbcCompilerGsPart m_gs;
    DxbcCompilerPsPart m_ps;
    
    /////////////////////////////////////////////////////
    // Shader interface and metadata declaration methods
    void emitDcl(
      const DxbcShaderInstruction&  ins);
    
    void emitDclGlobalFlags(
      const DxbcShaderInstruction&  ins);
    
    void emitDclTemps(
      const DxbcShaderInstruction&  ins);
    
    void emitDclIndexableTemp(
      const DxbcShaderInstruction&  ins);
    
    void emitDclInterfaceReg(
      const DxbcShaderInstruction&  ins);
    
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
    
    void emitDclGsInputPrimitive(
      const DxbcShaderInstruction&  ins);
    
    void emitDclGsOutputTopology(
      const DxbcShaderInstruction&  ins);
    
    void emitDclMaxOutputVertexCount(
      const DxbcShaderInstruction&  ins);
    
    ////////////////////////
    // Custom data handlers
    void emitDclImmediateConstantBuffer(
      const DxbcShaderInstruction&  ins);
    
    void emitCustomData(
      const DxbcShaderInstruction&  ins);
    
    //////////////////////////////
    // Instruction class handlers
    void emitVectorAlu(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorCmov(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorCmp(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorDeriv(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorDot(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorIdiv(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorImul(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorShift(
      const DxbcShaderInstruction&  ins);
    
    void emitVectorSinCos(
      const DxbcShaderInstruction&  ins);
    
    void emitGeometryEmit(
      const DxbcShaderInstruction&  ins);
    
    void emitSample(
      const DxbcShaderInstruction&  ins);
    
    /////////////////////////////////////
    // Control flow instruction handlers
    void emitControlFlowIf(
      const DxbcShaderInstruction&  ins);
    
    void emitControlFlowElse(
      const DxbcShaderInstruction&  ins);
    
    void emitControlFlowEndIf(
      const DxbcShaderInstruction&  ins);
    
    void emitControlFlowLoop(
      const DxbcShaderInstruction&  ins);
    
    void emitControlFlowEndLoop(
      const DxbcShaderInstruction&  ins);
    
    void emitControlFlowBreakc(
      const DxbcShaderInstruction&  ins);
    
    void emitControlFlowRet(
      const DxbcShaderInstruction&  ins);
    
    void emitControlFlowDiscard(
      const DxbcShaderInstruction&  ins);
    
    void emitControlFlow(
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
    
    DxbcRegisterValue emitRegisterZeroTest(
            DxbcRegisterValue       value,
            DxbcZeroTest            test);
    
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
    
    DxbcRegisterPointer emitGetIndexableTempPtr(
      const DxbcRegister&           operand);
    
    DxbcRegisterPointer emitGetInputPtr(
      const DxbcRegister&           operand);
    
    DxbcRegisterPointer emitGetOutputPtr(
      const DxbcRegister&           operand);
    
    DxbcRegisterPointer emitGetConstBufPtr(
      const DxbcRegister&           operand);
    
    DxbcRegisterPointer emitGetImmConstBufPtr(
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
    void emitGsInputSetup();
    void emitPsInputSetup();
    
    //////////////////////////////
    // Output preparation methods
    void emitVsOutputSetup();
    void emitGsOutputSetup();
    void emitPsOutputSetup();
    
    /////////////////////////////////
    // Shader initialization methods
    void emitVsInit();
    void emitGsInit();
    void emitPsInit();
    
    ///////////////////////////////
    // Shader finalization methods
    void emitVsFinalize();
    void emitGsFinalize();
    void emitPsFinalize();
    
    ///////////////////////////////
    // Variable definition methods
    uint32_t emitNewVariable(
      const DxbcRegisterInfo& info);
    
    /////////////////////////////////////
    // Control flow block search methods
    DxbcCfgBlock* cfgFindLoopBlock();
    
    ///////////////////////////
    // Type definition methods
    uint32_t getScalarTypeId(
            DxbcScalarType type);
    
    uint32_t getVectorTypeId(
      const DxbcVectorType& type);
    
    uint32_t getArrayTypeId(
      const DxbcArrayType& type);
    
    uint32_t getPointerTypeId(
      const DxbcRegisterInfo& type);
    
    uint32_t getPerVertexBlockId();
    
  };
  
}