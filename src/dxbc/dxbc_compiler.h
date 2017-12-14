#pragma once

#include "../spirv/spirv_module.h"

#include "dxbc_chunk_isgn.h"
#include "dxbc_decoder.h"
#include "dxbc_defs.h"

namespace dxvk {
  
  /**
   * \brief Expression value
   * 
   * Tracks the type and the SPIR-V variable
   * ID when evaluating DXBC instructions.
   */
  struct DxbcValue {
    DxbcScalarType componentType  = DxbcScalarType::Float32;
    uint32_t       componentCount = 0;
    uint32_t       valueId        = 0;
  };
  
  /**
   * \brief Variable pointer 
   * 
   * Stores the SPIR-V pointer ID and the
   * type of the referenced variable. Used
   * to access variables and resources.
   */
  struct DxbcPointer {
    DxbcScalarType componentType  = DxbcScalarType::Float32;
    uint32_t       componentCount = 0;
    uint32_t       pointerId      = 0;
  };
  
  /**
   * \brief Constant buffer binding
   * 
   * Stores information required to
   * access a constant buffer.
   */
  struct DxbcConstantBuffer {
    uint32_t varId = 0;
    uint32_t size  = 0;
  };
  
  /**
   * \brief Sampler binding
   * 
   * Stores a sampler variable that can be
   * used together with a texture resource.
   */
  struct DxbcSampler {
    uint32_t varId  = 0;
    uint32_t typeId = 0;
  };
  
  
  /**
   * \brief Shader resource binding
   * 
   * Stores a resource variable
   * and associated type IDs.
   */
  struct DxbcShaderResource {
    uint32_t varId         = 0;
    uint32_t sampledTypeId = 0;
    uint32_t textureTypeId = 0;
  };
  
  /**
   * \brief System value mapping
   * 
   * Maps a system value to a given set of
   * components of an input or output register.
   */
  struct DxbcSvMapping {
    uint32_t        regId;
    DxbcRegMask     regMask;
    DxbcSystemValue sv;
  };
  
  /**
   * \brief Compiler error code
   * 
   * Helps identify the type of error
   * that may occur during compilation.
   */
  enum class DxbcError {
    sOk,
    eInternal,
    eInstructionFormat,
    eInvalidOperand,
    eInvalidOperandIndex,
    eTypeMismatch,
    eUnhandledOpcode,
    eUnsupported,
  };
  
  
  /**
   * \brief Operand index type
   * 
   * Defines whether a register index
   * is relative or constant.
   */
  enum class DxbcIndexType {
    Immediate,  ///< Index is a constant value
    Relative,   ///< Index depends on a r# register
  };
  
  
  /**
   * \brief Instruction operand index
   * 
   * Stores the type of the index as well as the
   * register (if relative) and the constant offset.
   */
  struct DxbcInstOpIndex {
    DxbcIndexType   type = DxbcIndexType::Immediate;
    uint32_t        immediate = 0;
    uint32_t        tempRegId = 0;
    uint32_t        tempRegComponent = 0;
  };
  
  /**
   * \brief Instruction operand
   * 
   * Stores all information about a single
   * operand, including the register index.
   */
  struct DxbcInstOp {
    DxbcOperandType       type = DxbcOperandType::Temp;
    DxbcOperandModifiers  modifiers     = 0;
    uint32_t              immediates[4] = { 0u, 0u, 0u, 0u };
    
    uint32_t              indexDim = 0;
    DxbcInstOpIndex       index[3];
    
    uint32_t              componentCount = 0;
    DxbcRegMode           componentMode  = DxbcRegMode::Mask;
    
    DxbcRegMask           mask    = { false, false, false, false };
    DxbcRegSwizzle        swizzle = { 0, 0, 0, 0 };
    uint32_t              select1 = 0;
  };
  
  
  /**
   * \brief Decoded instruction
   * 
   * Stores all information about a single
   * instruction, including its operands.
   */
  struct DxbcInst {
    DxbcOpcode            opcode = DxbcOpcode::Nop;
    DxbcOpcodeControl     control = 0;
    DxbcInstFormat        format;
    DxbcInstOp            operands[DxbcMaxOperandCount];
  };
  
  
  /**
   * \brief Vertex shader-specific data
   */
  struct DxbcVsSpecifics {
    uint32_t  functionId = 0;
  };
  
  
  /**
   * \brief Pixel shader-specific data
   */
  struct DxbcPsSpecifics {
    uint32_t  functionId = 0;
    
    std::array<DxbcPointer, DxbcMaxInterfaceRegs> oregs;
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
     * 
     * \param [in] ins The instruction
     * \returns An error code, or \c sOK
     */
    DxbcError processInstruction(
      const DxbcInstruction&  ins);
    
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
    
    //////////////////////////////////////////////////////////
    // o# registers as defined by the shader. In the fragment
    // shader stage, these registers are typed by the signature,
    // in all other stages, they are float4 registers or arrays.
    std::array<uint32_t, DxbcMaxInterfaceRegs> m_oRegs;
    
    //////////////////////////////////////////////////////
    // Shader resource variables. These provide access to
    // constant buffers, samplers, textures, and UAVs.
    std::array<DxbcConstantBuffer,  16> m_constantBuffers;
    std::array<DxbcSampler,         16> m_samplers;
    std::array<DxbcShaderResource, 128> m_textures;
    
    ////////////////////////////////////////////////////////
    // Input/Output system value mappings. These will need
    // to be set up before or after the main function runs.
    std::vector<DxbcSvMapping> m_vSvs;
    std::vector<DxbcSvMapping> m_oSvs;
    
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
    
    ////////////////////////////////////////
    // Data structures for each shader type
    DxbcVsSpecifics m_vs;
    DxbcPsSpecifics m_ps;
    
    //////////////////////////////
    // Instruction class handlers
    DxbcError handleDeclaration(
      const DxbcInst& ins);
    
    DxbcError handleControlFlow(
      const DxbcInst& ins);
    
    DxbcError handleTextureSample(
      const DxbcInst& ins);
    
    DxbcError handleVectorAlu(
      const DxbcInst& ins);
    
    DxbcError handleVectorDot(
      const DxbcInst& ins);
    
    DxbcError handleVectorSinCos(
      const DxbcInst& ins);
    
    ///////////////////////
    // Declaration methods
    DxbcError declareGlobalFlags(
      const DxbcInst& ins);
    
    DxbcError declareTemps(
      const DxbcInst& ins);
    
    DxbcError declareInterfaceVar(
      const DxbcInst& ins);
    
    DxbcError declareConstantBuffer(
      const DxbcInst& ins);
    
    DxbcError declareSampler(
      const DxbcInst& ins);
    
    DxbcError declareResource(
      const DxbcInst& ins);
    
    DxbcError declareInputVar(
            uint32_t              regId,
            uint32_t              regDim,
            DxbcRegMask           regMask,
            DxbcSystemValue       sv,
            DxbcInterpolationMode im);
    
    DxbcError declareOutputVar(
            uint32_t              regId,
            uint32_t              regDim,
            DxbcRegMask           regMask,
            DxbcSystemValue       sv,
            DxbcInterpolationMode im);
    
    ////////////////////////////////////
    // Register manipulation operations
    DxbcValue bitcastReg(
      const DxbcValue&            src,
            DxbcScalarType        type);
    
    DxbcValue insertReg(
      const DxbcValue&            dst,
      const DxbcValue&            src,
            DxbcRegMask           mask);
    
    DxbcValue extractReg(
      const DxbcValue&            src,
            DxbcRegMask           mask);
    
    DxbcValue swizzleReg(
      const DxbcValue&            src,
      const DxbcRegSwizzle&       swizzle,
            DxbcRegMask           mask);
    
    DxbcValue regVector(
      const DxbcValue&            src,
            uint32_t              size);
    
    DxbcValue extendReg(
      const DxbcValue&            src,
            uint32_t              size);
    
    ////////////////////////////
    // Operand modifier methods
    DxbcValue applyOperandModifiers(
          DxbcValue             value,
          DxbcOperandModifiers  modifiers);
    
    DxbcValue applyResultModifiers(
          DxbcValue             value,
          DxbcOpcodeControl     control);
    
    /////////////////////////
    // Load/Store operations
    DxbcValue loadOp(
      const DxbcInstOp&           srcOp,
            DxbcRegMask           srcMask,
            DxbcScalarType        dstType);
    
    DxbcValue loadImm32(
      const DxbcInstOp&           srcOp,
            DxbcRegMask           srcMask,
            DxbcScalarType        dstType);
    
    DxbcValue loadRegister(
      const DxbcInstOp&           srcOp,
            DxbcRegMask           srcMask,
            DxbcScalarType        dstType);
    
    void storeOp(
      const DxbcInstOp&           dstOp,
      const DxbcValue&            srcValue);
    
    DxbcValue loadPtr(
      const DxbcPointer&          ptr);
    
    void storePtr(
      const DxbcPointer&          ptr,
      const DxbcValue&            value,
            DxbcRegMask           mask);
    
    DxbcValue loadIndex(
      const DxbcInstOpIndex&      idx);
    
    ///////////////////////////
    // Operand pointer methods
    DxbcPointer getOperandPtr(
      const DxbcInstOp&           op);
    
    DxbcPointer getConstantBufferPtr(
      const DxbcInstOp&           op);
    
    /////////////////////////////////
    // Shader initialization methods
    void beginVertexShader(const Rc<DxbcIsgn>& isgn);
    void beginPixelShader (const Rc<DxbcIsgn>& osgn);
    
    /////////////////////////////
    // Input preparation methods
    void prepareVertexInputs();
    void preparePixelInputs();
    
    //////////////////////////////
    // Output preparation methods
    void prepareVertexOutputs();
    void preparePixelOutputs();
    
    ///////////////////////////////
    // Shader finalization methods
    void endVertexShader();
    void endPixelShader();
    
    ///////////////////////////
    // Type definition methods
    uint32_t definePerVertexBlock();
    
    uint32_t defineScalarType(
            DxbcScalarType        componentType);
    
    uint32_t defineVectorType(
            DxbcScalarType        componentType,
            uint32_t              componentCount);
    
    uint32_t definePointerType(
            DxbcScalarType        componentType,
            uint32_t              componentCount,
            spv::StorageClass     storageClass);
    
    /////////////////////////
    // DXBC decoding methods
    DxbcError parseInstruction(
      const DxbcInstruction& ins,
            DxbcInst&        out);
    
  };
  
}