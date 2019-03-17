#pragma once

#include "dxso_decoder.h"
#include "dxso_header.h"
#include "dxso_modinfo.h"

#include "../spirv/spirv_module.h"

namespace dxvk {

  using SpirvId = uint32_t;

  struct DxsoSpirvRegister {
    DxsoRegisterId  regId;
    SpirvId         varId;
  };

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

  /**
   * \brief Pixel shader-specific structure
   */
  struct DxsoCompilerPsPart {
    uint32_t functionId = 0;

    uint32_t builtinFragCoord = 0;
    uint32_t builtinDepth = 0;
    uint32_t builtinIsFrontFace = 0;
    uint32_t builtinSampleId = 0;
    uint32_t builtinSampleMaskIn = 0;
    uint32_t builtinSampleMaskOut = 0;
    uint32_t builtinLayer = 0;
    uint32_t builtinViewportId = 0;

    uint32_t invocationMask = 0;
    uint32_t killState = 0;
  };

  class DxsoCompiler {

  public:

    static constexpr uint32_t InvalidInputSlot = UINT32_MAX;
    static constexpr uint32_t InvalidOutputSlot = UINT32_MAX;

    DxsoCompiler(
      const std::string&     fileName,
      const DxsoModuleInfo&  moduleInfo,
      const DxsoProgramInfo& programInfo);

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

    const uint32_t getDeclCount() const {
      return m_nextInputSlot;
    }

  private:

    DxsoModuleInfo      m_moduleInfo;
    DxsoProgramInfo     m_programInfo;
    SpirvModule         m_module;

    ///////////////////////////////////////////////////////
    // Resource slot description for the shader. This will
    // be used to map D3D11 bindings to DXVK bindings.
    std::vector<DxvkResourceSlot> m_resourceSlots;

    std::vector<DxsoSpirvRegister> m_regs;


    ///////////////////////////////////////////////////
    // Entry point description - we'll need to declare
    // the function ID and all input/output variables.
    std::vector<uint32_t> m_entryPointInterfaces;
    uint32_t m_entryPointId = 0;

    ////////////////////////////////////////////////////
    // Per-vertex input and output blocks. Depending on
    // the shader stage, these may be declared as arrays.
    uint32_t m_perVertexIn = 0;
    uint32_t m_perVertexOut = 0;

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

    /////////////////////////////////////////////////////
    // Shader interface and metadata declaration methods
    void emitInit();
    void emitVsInit();
    void emitPsInit();

    void emitVsFinalize();
    void emitPsFinalize();

    void emitOutputDepthClamp();

    void emitFunctionBegin(
        uint32_t                entryPoint,
        uint32_t                returnType,
        uint32_t                funcType);
    
    void emitFunctionEnd();
    
    void emitFunctionLabel();
    
    void emitMainFunctionBegin();

    uint32_t getPerVertexBlockId();

    uint32_t emitNewVariable(DxsoRegisterType regType, spv::StorageClass storageClass);

    uint32_t emitRegisterLoad(const DxsoRegister& reg);

    uint32_t emitRegisterSwizzle(uint32_t typeId, uint32_t varId, DxsoRegSwizzle swizzle);

    uint32_t emitSrcOperandModifier(uint32_t typeId, uint32_t varId, DxsoRegModifier modifier);

    uint32_t emitDstOperandModifier(uint32_t typeId, uint32_t varId, bool saturate, bool partialPrecision);

    uint32_t emitWriteMask(uint32_t typeId, uint32_t dst, uint32_t src, DxsoRegMask writeMask);

    void emitVectorAlu(const DxsoInstructionContext& ctx);

    void emitDcl(const DxsoInstructionContext& ctx);
    void emitDef(const DxsoInstructionContext& ctx);


    ///////////////////////////////////
    // Health Warning: Can cause m_regs to be
    // realloced. Don't call me unless you accept this fact.
    DxsoSpirvRegister& getSpirvRegister(const DxsoRegister& reg);
    uint32_t spvId(const DxsoRegister& reg) {
      return getSpirvRegister(reg).varId;
    }
    DxsoSpirvRegister& mapSpirvRegister(const DxsoRegister& reg, const DxsoDeclaration* optionalPremadeDecl);

    uint32_t getTypeId(DxsoRegisterType regType);
    uint32_t spvType(const DxsoRegister& reg) {
      return getTypeId(reg.registerId().type());
    }
    uint32_t getPointerTypeId(DxsoRegisterType regType, spv::StorageClass storageClass) {
      return m_module.defPointerType(
        this->getTypeId(regType),
        storageClass);
    }

    ///////////////////////////////////////////
    // Reads decls and generates an input slot.
    uint32_t allocateInputSlot();
    uint32_t allocateOutputSlot();

    uint32_t m_nextInputSlot = 0;
    uint32_t m_nextOutputSlot = 0;

    uint32_t getTypeId(DxsoSpirvRegister& reg) {
      return getTypeId(reg.regId.type());
    }

  };

}