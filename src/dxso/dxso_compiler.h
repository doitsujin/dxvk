#pragma once

#include "dxso_decoder.h"
#include "dxso_header.h"
#include "dxso_modinfo.h"
#include "dxso_isgn.h"

#include "../d3d9/d3d9_constant_layout.h"
#include "../d3d9/d3d9_shader_permutations.h"
#include "../spirv/spirv_module.h"

namespace dxvk {

  /**
   * \brief Scalar value type
   * 
   * Enumerates possible register component
   * types. Scalar types are represented as
   * a one-component vector type.
   */
  enum class DxsoScalarType : uint32_t {
    Uint32    = 0,
    Sint32    = 1,
    Float32   = 2,
    Bool      = 3,
  };

  /**
   * \brief Vector type
   * 
   * Convenience struct that stores a scalar
   * type and a component count. The compiler
   * can use this to generate SPIR-V types.
   */
  struct DxsoVectorType {
    DxsoScalarType    ctype;
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
  struct DxsoArrayType {
    DxsoScalarType    ctype;
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
  struct DxsoRegisterInfo {
    DxsoArrayType     type;
    spv::StorageClass sclass;
  };
  
  
  /**
   * \brief Register value
   * 
   * Stores a vector type and a SPIR-V ID that
   * represents an intermediate value. This is
   * used to track the type of such values.
   */
  struct DxsoRegisterValue {
    DxsoVectorType    type;
    uint32_t          id;
  };

  
  /**
   * \brief Register pointer
   * 
   * Stores a vector type and a SPIR-V ID that
   * represents a pointer to such a vector. This
   * can be used to load registers conveniently.
   */
  struct DxsoRegisterPointer {
    DxsoVectorType    type;
    uint32_t          id   = 0;
  };

  /**
   * \brief Sampler info
   *
   * Stores a vector type and a SPIR-V ID that
   * represents a pointer to such a vector. This
   * can be used to load registers conveniently.
   */
  struct DxsoSamplerInfo {
    uint32_t dimensions = 0;

    uint32_t varId = 0;
    uint32_t typeId = 0;

    uint32_t imageTypeId = 0;
  };

  enum DxsoSamplerType : uint32_t {
    SamplerTypeTexture2D = 0,
    SamplerTypeTexture3D = 1,
    SamplerTypeTextureCube,

    SamplerTypeCount
  };

  inline auto SamplerTypeFromTextureType(DxsoTextureType type) {
    switch (type) {
      default:
      case DxsoTextureType::Texture2D:   return SamplerTypeTexture2D;   break;
      case DxsoTextureType::Texture3D:   return SamplerTypeTexture3D;   break;
      case DxsoTextureType::TextureCube: return SamplerTypeTextureCube; break;
    }
  }

  struct DxsoSampler {
    DxsoSamplerInfo color[SamplerTypeCount];
    DxsoSamplerInfo depth[SamplerTypeCount];

    uint32_t boundConst;

    DxsoTextureType type;
  };

  struct DxsoAnalysisInfo;

  /**
   * \brief Vertex shader-specific structure
   */
  struct DxsoCompilerVsPart {
    uint32_t functionId       = 0;

    ////////////////////
    // Address register
    DxsoRegisterPointer addr;

    //////////////////////////////
    // Rasterizer output registers
    DxsoRegisterPointer oPos;
    DxsoRegisterPointer oPSize;
  };

  /**
   * \brief Pixel shader-specific structure
   */
  struct DxsoCompilerPsPart {
    uint32_t functionId         = 0;
    uint32_t samplerTypeSpec    = 0;
    uint32_t projectionSpec     = 0;
    uint32_t fetch4Spec         = 0;

    //////////////
    // Misc Types
    DxsoRegisterPointer vPos;
    DxsoRegisterPointer vFace;

    ///////////////////
    // Colour Outputs
    std::array<DxsoRegisterPointer, 4> oColor;

    ////////////////
    // Depth output
    DxsoRegisterPointer oDepth;

    ////////////////
    // Shared State
    uint32_t sharedState        = 0;

    uint32_t killState          = 0;
    uint32_t builtinLaneId      = 0;

    uint32_t diffuseColorIn  = 0;
    uint32_t specularColorIn = 0;
  };

  struct DxsoCfgBlockIf {
    uint32_t ztestId;
    uint32_t labelIf;
    uint32_t labelElse;
    uint32_t labelEnd;
    size_t   headerPtr;
  };

  struct DxsoCfgBlockLoop {
    uint32_t labelHeader;
    uint32_t labelBegin;
    uint32_t labelContinue;
    uint32_t labelBreak;
    uint32_t iteratorPtr;

    uint32_t strideVar;
    uint32_t countBackup;
  };

  enum class DxsoCfgBlockType : uint32_t {
    If, Loop
  };

  struct DxsoCfgBlock {
    DxsoCfgBlockType type;
    
    union {
      DxsoCfgBlockIf     b_if;
      DxsoCfgBlockLoop   b_loop;
    };
  };

  using DxsoSrcArray = std::array<DxsoRegisterValue, DxsoMaxOperandCount>;

  class DxsoCompiler {

  public:

    DxsoCompiler(
      const std::string&        fileName,
      const DxsoModuleInfo&     moduleInfo,
      const DxsoProgramInfo&    programInfo,
      const DxsoAnalysisInfo&   analysis,
      const D3D9ConstantLayout& layout);

    /**
     * \brief Processes a single instruction
     * \param [in] ins The instruction
     */
    void processInstruction(
      const DxsoInstructionContext& ctx,
            uint32_t                currentCoissueIdx = 0);

    /**
     * \brief Finalizes the shader
     */
    void finalize();

    /**
     * \brief Compiles the shader
     * \returns The final shader objects
     */
    DxsoPermutations compile();

    const DxsoIsgn& isgn() { return m_isgn; }
    const DxsoIsgn& osgn() { return m_osgn; }

    const DxsoShaderMetaInfo& meta() { return m_meta; }
    const DxsoDefinedConstants& constants() { return m_constants; }
    uint32_t usedSamplers() const { return m_usedSamplers; }
    uint32_t usedRTs() const { return m_usedRTs; }

  private:

    DxsoModuleInfo             m_moduleInfo;
    DxsoProgramInfo            m_programInfo;
    const DxsoAnalysisInfo*    m_analysis;
    const D3D9ConstantLayout*  m_layout;

    DxsoShaderMetaInfo         m_meta;
    DxsoDefinedConstants       m_constants;

    SpirvModule                m_module;

    uint32_t                   m_boolSpecConstant;
    uint32_t                   m_depthSpecConstant;

    ///////////////////////////////////////////////////////
    // Resource slot description for the shader. This will
    // be used to map D3D9 bindings to DXVK bindings.
    std::vector<DxvkResourceSlot> m_resourceSlots;

    ////////////////////////////////////////////////
    // Temporary r# vector registers with immediate
    // indexing, and x# vector array registers.
    std::array<
      DxsoRegisterPointer,
      DxsoMaxTempRegs> m_rRegs;

    ////////////////////////////////////////////////
    // Predicate registers
    std::array<
      DxsoRegisterPointer,
      1> m_pRegs;

    //////////////////////////////////////////////////////////////////
    // Array of input values. Since v# and o# registers are indexable
    // in DXSO, we need to copy them into an array first.
    uint32_t m_vArray = 0;
    uint32_t m_oArray = 0;

    ////////////////////////////////
    // Input and output signatures
    DxsoIsgn m_isgn;
    DxsoIsgn m_osgn;

    ////////////////////////////////////
    // Ptr to the constant buffer array
    uint32_t m_cBuffer;

    ////////////////////////////////////////
    // Constant buffer deffed mappings
    std::array<uint32_t, caps::MaxFloatConstantsSoftware> m_cFloat;
    std::array<uint32_t, caps::MaxOtherConstantsSoftware> m_cInt;
    std::array<uint32_t, caps::MaxOtherConstantsSoftware> m_cBool;

    //////////////////////
    // Loop counter
    DxsoRegisterPointer m_loopCounter;

    ///////////////////////////////////
    // Working tex/coord registers (PS)
    std::array<
      DxsoRegisterPointer,
      DxsoMaxTextureRegs> m_tRegs;

    ///////////////////////////////////////////////
    // Control flow information. Stores labels for
    // currently active if-else blocks and loops.
    std::vector<DxsoCfgBlock> m_controlFlowBlocks;

    //////////////////////////////////////////////
    // Function state tracking. Required in order
    // to properly end functions in some cases.
    bool m_insideFunction = false;

    ////////////
    // Samplers
    std::array<DxsoSampler, 17> m_samplers;

    ////////////////////////////////////////////
    // What io regswe need to
    // NOT generate semantics for
    uint16_t m_explicitInputs  = 0;
    uint16_t m_explicitOutputs = 0;

    ///////////////////////////////////////////////////
    // Entry point description - we'll need to declare
    // the function ID and all input/output variables.
    std::vector<uint32_t> m_entryPointInterfaces;
    uint32_t m_entryPointId = 0;

    ////////////////////////////////////////////
    // Inter-stage shader interface slots. Also
    // covers vertex input and fragment output.
    DxvkInterfaceSlots m_interfaceSlots;

    ///////////////////////////////////
    // Shader-specific data structures
    DxsoCompilerVsPart m_vs;
    DxsoCompilerPsPart m_ps;

    DxsoRegisterPointer m_fog;

    //////////////////////////////////////////
    // Bit masks containing used samplers
    // and render targets for hazard tracking
    uint32_t m_usedSamplers;
    uint32_t m_usedRTs;

    uint32_t m_rsBlock = 0;
    uint32_t m_mainFuncLabel = 0;

    //////////////////////////////////////
    // Common function definition methods
    void emitInit();

    //////////////////////
    // Common shader dcls
    void emitDclConstantBuffer();

    void emitDclInputArray();
    void emitDclOutputArray();

    /////////////////////////////////
    // Shader initialization methods
    void emitVsInit();

    void emitPsSharedConstants();
    void emitPsInit();

    void emitFunctionBegin(
            uint32_t                entryPoint,
            uint32_t                returnType,
            uint32_t                funcType);

    void emitFunctionEnd();

    uint32_t emitFunctionLabel();

    void emitMainFunctionBegin();

    ///////////////////////////////
    // Variable definition methods
    uint32_t emitNewVariable(
      const DxsoRegisterInfo& info);

    uint32_t emitNewVariableDefault(
      const DxsoRegisterInfo& info,
            uint32_t          value);
    
    uint32_t emitNewBuiltinVariable(
      const DxsoRegisterInfo& info,
            spv::BuiltIn      builtIn,
      const char*             name,
            uint32_t          value);

    DxsoCfgBlock* cfgFindBlock(
      const std::initializer_list<DxsoCfgBlockType>& types);

    void emitDclInterface(
            bool         input,
            uint32_t     regNumber,
            DxsoSemantic semantic,
            DxsoRegMask  mask,
            bool         centroid);

    void emitDclSampler(
            uint32_t        idx,
            DxsoTextureType type);

    bool defineInput(uint32_t idx) {
      bool alreadyDefined = m_interfaceSlots.inputSlots & 1u << idx;
      m_interfaceSlots.inputSlots |= 1u << idx;
      return alreadyDefined;
    }

    bool defineOutput(uint32_t idx) {
      bool alreadyDefined = m_interfaceSlots.outputSlots & 1u << idx;
      m_interfaceSlots.outputSlots |= 1u << idx;
      return alreadyDefined;
    }

    uint32_t emitArrayIndex(
            uint32_t          idx,
      const DxsoBaseRegister* relative);

    DxsoRegisterPointer emitInputPtr(
            bool              texture,
      const DxsoBaseRegister& reg,
      const DxsoBaseRegister* relative);

    DxsoRegisterPointer emitRegisterPtr(
      const char*             name,
            DxsoScalarType    ctype,
            uint32_t          ccount,
            uint32_t          defaultVal,
            spv::StorageClass storageClass = spv::StorageClassPrivate,
            spv::BuiltIn      builtIn      = spv::BuiltInMax);

    DxsoRegisterValue emitLoadConstant(
      const DxsoBaseRegister& reg,
      const DxsoBaseRegister* relative);

    DxsoRegisterPointer emitOutputPtr(
            bool              texcrdOut,
      const DxsoBaseRegister& reg,
      const DxsoBaseRegister* relative);

    DxsoRegisterPointer emitGetOperandPtr(
      const DxsoBaseRegister& reg,
      const DxsoBaseRegister* relative);

    DxsoRegisterPointer emitGetOperandPtr(
      const DxsoRegister& reg) {
      return this->emitGetOperandPtr(
        reg,
        reg.hasRelative ? &reg.relative : nullptr);
    }

    uint32_t emitBoolComparison(DxsoVectorType type, DxsoComparison cmp, uint32_t a, uint32_t b);

    DxsoRegisterValue emitValueLoad(
            DxsoRegisterPointer ptr);

    void emitDstStore(
            DxsoRegisterPointer     ptr,
            DxsoRegisterValue       value,
            DxsoRegMask             writeMask,
            bool                    saturate,
            DxsoRegisterValue       predicate,
            int8_t                  shift,
            DxsoRegisterId          regId) {
      if (regId.type == DxsoRegisterType::RasterizerOut && regId.num == RasterOutFog)
        saturate = true;

      if (value.type.ctype == DxsoScalarType::Float32) {
        const uint32_t typeId = getVectorTypeId(value.type);

        // There doesn't seem to be a nice float bitshift method for float vectors
        // in Spirv that I can see... Resorting to multiplication.
        if (shift != 0) {
          float shiftAmount = shift < 0
            ? 1.0f / (1 << -shift)
            : float(1 << shift);

          uint32_t shiftConst = m_module.constf32(shiftAmount);

          if (value.type.ccount == 1)
            value.id = m_module.opFMul(typeId, value.id, shiftConst);
          else
            value.id = m_module.opVectorTimesScalar(typeId, value.id, shiftConst);
        }

        // Saturating only makes sense on floats
        if (saturate) {
          value.id = m_module.opNClamp(
            typeId, value.id,
            m_module.constfReplicant(0.0f, value.type.ccount),
            m_module.constfReplicant(1.0f, value.type.ccount));
        }
      }

      this->emitValueStore(ptr, value, writeMask, predicate);
    }

    DxsoRegisterValue applyPredicate(DxsoRegisterValue pred, DxsoRegisterValue dst, DxsoRegisterValue src);

    void emitValueStore(
            DxsoRegisterPointer     ptr,
            DxsoRegisterValue       value,
            DxsoRegMask             writeMask,
            DxsoRegisterValue       predicate);

    DxsoRegisterValue emitClampBoundReplicant(
            DxsoRegisterValue       srcValue,
            float                   lb,
            float                   ub);

    DxsoRegisterValue emitSaturate(
            DxsoRegisterValue       srcValue);

    DxsoRegisterValue emitDot(
            DxsoRegisterValue       a,
            DxsoRegisterValue       b);

    DxsoRegisterValue emitRegisterInsert(
            DxsoRegisterValue       dstValue,
            DxsoRegisterValue       srcValue,
            DxsoRegMask             srcMask);

    DxsoRegisterValue emitRegisterLoadRaw(
      const DxsoBaseRegister& reg,
      const DxsoBaseRegister* relative);

    DxsoRegisterValue emitRegisterExtend(
            DxsoRegisterValue       value,
            uint32_t size);

    DxsoRegisterValue emitSrcOperandPreSwizzleModifiers(
            DxsoRegisterValue       value,
            DxsoRegModifier         modifier);

    DxsoRegisterValue emitSrcOperandPostSwizzleModifiers(
            DxsoRegisterValue       value,
            DxsoRegModifier         modifier);

    DxsoRegisterValue emitRegisterSwizzle(
            DxsoRegisterValue       value,
            DxsoRegSwizzle          swizzle,
            DxsoRegMask             writeMask);

    DxsoRegisterValue emitRegisterLoad(
      const DxsoBaseRegister& reg,
            DxsoRegMask       writeMask,
      const DxsoBaseRegister* relative);

    DxsoRegisterValue emitRegisterLoad(
      const DxsoRegister& reg,
            DxsoRegMask   writeMask) {
      return this->emitRegisterLoad(
        reg, writeMask,
        reg.hasRelative ? &reg.relative : nullptr);
    }

    DxsoRegisterValue emitPredicateLoad(const DxsoInstructionContext& ctx) {
      if (!ctx.instruction.predicated)
        return DxsoRegisterValue();

      return emitRegisterLoad(ctx.pred, IdentityWriteMask);
    }

    DxsoRegisterValue emitRegisterLoadTexcoord(
      const DxsoRegister& reg,
            DxsoRegMask   writeMask) {
      DxsoRegister lookup = reg;
      if (reg.id.type == DxsoRegisterType::Texture)
        lookup.id.type = DxsoRegisterType::PixelTexcoord;

      return this->emitRegisterLoad(lookup, writeMask);
    }

    Rc<DxvkShader> compileShader();

    ///////////////////////////////
    // Handle shader ops
    void emitDcl(const DxsoInstructionContext& ctx);

    void emitDef(const DxsoInstructionContext& ctx);
    void emitDefF(const DxsoInstructionContext& ctx);
    void emitDefI(const DxsoInstructionContext& ctx);
    void emitDefB(const DxsoInstructionContext& ctx);

    bool isScalarRegister(DxsoRegisterId id);

    void emitMov(const DxsoInstructionContext& ctx);
    void emitPredicateOp(const DxsoInstructionContext& ctx);
    void emitVectorAlu(const DxsoInstructionContext& ctx);
    void emitMatrixAlu(const DxsoInstructionContext& ctx);

    void emitControlFlowGenericLoop(
            bool     count,
            uint32_t initialVar,
            uint32_t strideVar,
            uint32_t iterationCountVar);

    void emitControlFlowGenericLoopEnd();

    void emitControlFlowRep(const DxsoInstructionContext& ctx);
    void emitControlFlowEndRep(const DxsoInstructionContext& ctx);

    void emitControlFlowLoop(const DxsoInstructionContext& ctx);
    void emitControlFlowEndLoop(const DxsoInstructionContext& ctx);

    void emitControlFlowBreak(const DxsoInstructionContext& ctx);
    void emitControlFlowBreakC(const DxsoInstructionContext& ctx);

    void emitControlFlowIf(const DxsoInstructionContext& ctx);
    void emitControlFlowElse(const DxsoInstructionContext& ctx);
    void emitControlFlowEndIf(const DxsoInstructionContext& ctx);

    void emitTexCoord(const DxsoInstructionContext& ctx);
    void emitTextureSample(const DxsoInstructionContext& ctx);
    void emitTextureKill(const DxsoInstructionContext& ctx);
    void emitTextureDepth(const DxsoInstructionContext& ctx);

    uint32_t emitSample(
            bool                    projected,
            uint32_t                resultType,
            DxsoSamplerInfo&        samplerInfo,
            DxsoRegisterValue       coordinates,
            uint32_t                reference,
            uint32_t                fetch4,
      const SpirvImageOperands&     operands);

    ///////////////////////////////
    // Shader finalization methods
    void emitInputSetup();

    void emitVsClipping();
    void setupRenderStateInfo();
    void emitFog();
    void emitPsProcessing();
    void emitOutputDepthClamp();

    void emitLinkerOutputSetup();

    void emitVsFinalize();
    void emitPsFinalize();

    ///////////////////////////
    // Type definition methods
    uint32_t getScalarTypeId(
            DxsoScalarType type);
    
    uint32_t getVectorTypeId(
      const DxsoVectorType& type);
    
    uint32_t getArrayTypeId(
      const DxsoArrayType& type);
    
    uint32_t getPointerTypeId(
      const DxsoRegisterInfo& type);

  };

}