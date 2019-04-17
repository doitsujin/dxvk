#pragma once

#include "dxso_decoder.h"
#include "dxso_header.h"
#include "dxso_modinfo.h"

#include "../spirv/spirv_module.h"

namespace dxvk {

  struct DxsoSpirvRegister {
    DxsoRegisterId    regId = DxsoRegisterId(DxsoRegisterType::Temp, 0);

    uint32_t          ptrId = 0;
  };

  struct DxsoAnalysisInfo;

  /**
   * \brief Vertex shader-specific structure
   */
  struct DxsoCompilerVsPart {
    uint32_t functionId = 0;

    uint32_t builtinVertexId = 0;
    uint32_t builtinInstanceId = 0;
    uint32_t builtinBaseVertex = 0;
    uint32_t builtinBaseInstance = 0;
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

  /**
   * \brief Pixel shader-specific structure
   */
  struct DxsoCompilerPsPart {
    uint32_t functionId = 0;

    uint32_t discardPtr = 0;
  };

  class DxsoCompiler {

  public:

    static constexpr uint32_t InvalidInputSlot = UINT32_MAX;
    static constexpr uint32_t InvalidOutputSlot = UINT32_MAX;

    DxsoCompiler(
      const std::string&      fileName,
      const DxsoModuleInfo&   moduleInfo,
      const DxsoProgramInfo&  programInfo,
      const DxsoAnalysisInfo& analysis);

    /**
     * \brief Processes a single instruction
     * \param [in] ins The instruction
     */
    void processInstruction(
      const DxsoInstructionContext& ctx);

    /**
     * \brief Finalizes the shader
     * \returns The final shader object
     */
    Rc<DxvkShader> finalize();

    const std::array<DxsoDeclaration, 16>& getDeclarations() const {
      return m_vDecls;
    }

  private:

    DxsoModuleInfo             m_moduleInfo;
    DxsoProgramInfo            m_programInfo;
    const DxsoAnalysisInfo*    m_analysis;

    SpirvModule         m_module;

    ///////////////////////////////////////////////////////
    // Resource slot description for the shader. This will
    // be used to map D3D11 bindings to DXVK bindings.
    std::vector<DxvkResourceSlot> m_resourceSlots;

    std::vector<DxsoSpirvRegister> m_regs;

    uint32_t m_constantBufferVarId = 0;

    ///////////////////////////////////////////////////
    // Entry point description - we'll need to declare
    // the function ID and all input/output variables.
    std::vector<uint32_t> m_entryPointInterfaces;
    uint32_t m_entryPointId = 0;

    ///////////////////////////////////////////////////////////
    // v# and o# register definitions
    std::array<DxsoDeclaration, 16> m_vDecls;
    std::array<DxsoDeclaration, 16> m_oDecls;

    ////////////////////////////////////////////
    // Inter-stage shader interface slots. Also
    // covers vertex input and fragment output.
    DxvkInterfaceSlots m_interfaceSlots;

    //////////////////////////////////////////////
    // Function state tracking. Required in order
    // to properly end functions in some cases.
    bool m_insideFunction = false;

    ///////////////////////////////////
    // Shader-specific data structures
    DxsoCompilerVsPart m_vs;
    DxsoCompilerPsPart m_ps;

    uint32_t m_cBuffer = 0;

    struct DxsoSamplerDesc {
      DxsoTextureType type;

      uint32_t imageTypeId;
      uint32_t imagePtrId;
    };

    std::array<DxsoSamplerDesc, 17> m_samplers;

    /////////////////////////////////////////////////////
    // Shader interface and metadata declaration methods
    void emitInit();
    void emitVsInit();
    void emitPsInit();

    void emitVsFinalize();
    void emitPsFinalize();
    
    void emitVsClipping();
    void emitPsProcessing();

    void emitOutputDepthClamp();

    void emitDclConstantBuffer();

    void emitFunctionBegin(
        uint32_t                entryPoint,
        uint32_t                returnType,
        uint32_t                funcType);
    
    void emitFunctionEnd();
    
    void emitFunctionLabel();
    
    void emitMainFunctionBegin();

    uint32_t emitNewVariable(DxsoRegisterType regType, uint32_t value = 0);

    bool isVectorReg(DxsoRegisterType type);

    uint32_t emitRegisterLoad(const DxsoRegister& reg, uint32_t count = 4);

    uint32_t emitRegisterSwizzle(uint32_t typeId, uint32_t varId, DxsoRegSwizzle swizzle, uint32_t count);

    uint32_t emitVecTrunc(uint32_t typeId, uint32_t varId, uint32_t count);

    uint32_t emitSrcOperandModifier(uint32_t typeId, uint32_t varId, DxsoRegModifier modifier, uint32_t count);

    uint32_t emitDstOperandModifier(uint32_t typeId, uint32_t varId, bool saturate, bool partialPrecision);

    uint32_t emitWriteMask(bool vector, uint32_t typeId, uint32_t dst, uint32_t src, DxsoRegMask writeMask);

    void     emitDebugName(uint32_t varId, DxsoRegisterId id, bool deffed = false);

    uint32_t emitScalarReplicant(uint32_t typeId, uint32_t varId);

    uint32_t emitBoolComparison(DxsoComparison cmp, uint32_t a, uint32_t b);

    DxsoCfgBlock* cfgFindBlock(
      const std::initializer_list<DxsoCfgBlockType>& types);

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

    void emitVectorAlu(const DxsoInstructionContext& ctx);

    uint32_t emitInfinityClamp(uint32_t typeId, uint32_t varId, bool vector = true);

    void emitTextureKill(const DxsoInstructionContext& ctx);
    void emitTextureSample(const DxsoInstructionContext& ctx);

    void emitDcl(const DxsoInstructionContext& ctx);
    void emitDclSampler(uint32_t idx, DxsoTextureType type);

    void emitDef(DxsoOpcode opcode, const DxsoInstructionContext& ctx);
    void emitDefF(const DxsoInstructionContext& ctx);
    void emitDefI(const DxsoInstructionContext& ctx);
    void emitDefB(const DxsoInstructionContext& ctx);


    ///////////////////////////////////
    // Health Warning: Can cause m_regs to be
    // realloced. Don't call me unless you accept this fact.
    DxsoSpirvRegister getSpirvRegister(DxsoRegisterId id, bool centroid, DxsoRegister* relative);
    DxsoSpirvRegister getSpirvRegister(const DxsoRegister& reg);

    uint32_t spvPtr(const DxsoRegister& reg) {
      return getSpirvRegister(reg).ptrId;
    }

    DxsoSpirvRegister mapSpirvRegister(DxsoRegisterId id, bool centroid, DxsoRegister* relative, const DxsoDeclaration* optionalPremadeDecl);

    spv::StorageClass spvStorage(DxsoRegisterType regType);

    DxsoSpirvRegister findBuiltInOutputPtr(DxsoUsage usage, uint32_t index);

    uint32_t spvLoad(DxsoRegisterId regId) {
      return m_module.opLoad(
        spvTypeVar(regId.type()),
        getSpirvRegister(regId, false, nullptr).ptrId);
    }

    uint32_t spvLoad(const DxsoRegister& reg) {
      return m_module.opLoad(
        spvTypeVar(reg.registerId().type()),
        getSpirvRegister(reg).ptrId);
    }

    uint32_t spvTypeVar(DxsoRegisterType regType, uint32_t count = 4);
    uint32_t spvTypeVar(const DxsoRegister& reg, uint32_t count = 4) {
      return spvTypeVar(reg.registerId().type(), count);
    }
    uint32_t spvTypePtr(DxsoRegisterType regType, uint32_t count = 4) {
      return m_module.defPointerType(
        this->spvTypeVar(regType, count),
        this->spvStorage(regType));
    }
    uint32_t spvTypePtr(const DxsoRegister& reg, uint32_t count = 4) {
      return spvTypePtr(
        reg.registerId().type(),
        count);
    }

    ///////////////////////////////////////////////
    // Control flow information. Stores labels for
    // currently active if-else blocks and loops.
    std::vector<DxsoCfgBlock> m_controlFlowBlocks;

    ///////////////////////////////////////////
    // Reads decls and generates an input slot.
    uint32_t allocateSlot(bool input, DxsoRegisterId id, DxsoSemantic semantic);

    std::array<uint32_t, 16> m_oPtrs;

  };

}