#pragma once

#include "../dxbc_chunk_isgn.h"
#include "../dxbc_common.h"
#include "../dxbc_decoder.h"
#include "../dxbc_type.h"
#include "../dxbc_util.h"

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
   * Stores a sampler variable.
   */
  struct DxbcSampler {
    uint32_t varId = 0;
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
    
    DxbcCodeGen(DxbcProgramType shaderStage);
    
    virtual ~DxbcCodeGen();
    
    void dclTemps(uint32_t n);
    
    void dclConstantBuffer(
            uint32_t              bufferId,
            uint32_t              elementCount);
    
    void dclSampler(
            uint32_t              samplerId);
    
    DxbcValue defConstScalar(uint32_t v);
    
    DxbcValue defConstVector(
            uint32_t x, uint32_t y,
            uint32_t z, uint32_t w);
    
    void fnReturn();
    
    DxbcPointer ptrTempReg(
            uint32_t              regId);
    
    DxbcPointer ptrConstantBuffer(
            uint32_t              regId,
      const DxbcValue&            index);
    
    DxbcValue opAbs(
      const DxbcValue&            src);
    
    DxbcValue opAdd(
      const DxbcValue&            a,
      const DxbcValue&            b);
    
    DxbcValue opMul(
      const DxbcValue&            a,
      const DxbcValue&            b);
    
    DxbcValue opDot(
      const DxbcValue&            a,
      const DxbcValue&            b);
    
    DxbcValue opNeg(
      const DxbcValue&            src);
    
    DxbcValue opSaturate(
      const DxbcValue&            src);
    
    DxbcValue regCast(
      const DxbcValue&            src,
      const DxbcValueType&        type);
    
    DxbcValue regExtract(
      const DxbcValue&            src,
            DxbcComponentMask     mask);
    
    DxbcValue regSwizzle(
      const DxbcValue&            src,
      const DxbcComponentSwizzle& swizzle,
            DxbcComponentMask     mask);
    
    DxbcValue regInsert(
      const DxbcValue&            dst,
      const DxbcValue&            src,
            DxbcComponentMask     mask);
    
    DxbcValue regLoad(
      const DxbcPointer&          ptr);
    
    void regStore(
      const DxbcPointer&          ptr,
      const DxbcValue&            val,
            DxbcComponentMask     mask);
    
    virtual void dclInterfaceVar(
            DxbcOperandType       regType,
            uint32_t              regId,
            uint32_t              regDim,
            DxbcComponentMask     regMask,
            DxbcSystemValue       sv,
            DxbcInterpolationMode im) = 0;
    
    virtual DxbcPointer ptrInterfaceVar(
            DxbcOperandType       regType,
            uint32_t              regId) = 0;
    
    virtual DxbcPointer ptrInterfaceVarIndexed(
            DxbcOperandType       regType,
            uint32_t              regId,
      const DxbcValue&            index) = 0;
    
    virtual Rc<DxvkShader> finalize() = 0;
    
    static Rc<DxbcCodeGen> create(
      const DxbcProgramVersion& version,
      const Rc<DxbcIsgn>&       isgn,
      const Rc<DxbcIsgn>&       osgn);
    
  protected:
    
    constexpr static uint32_t PerVertex_Position  = 0;
    constexpr static uint32_t PerVertex_PointSize = 1;
    constexpr static uint32_t PerVertex_CullDist  = 2;
    constexpr static uint32_t PerVertex_ClipDist  = 3;
    
    const DxbcProgramType m_shaderStage;
    
    SpirvModule m_module;
    
    std::vector<uint32_t> m_entryPointInterfaces;
    uint32_t              m_entryPointId = 0;
    
    std::vector<DxbcPointer> m_rRegs;
    
    std::array<DxbcConstantBuffer, 16>  m_constantBuffers;
    std::array<DxbcSampler,        16>  m_samplers;
    
    std::vector<DxvkResourceSlot>       m_resourceSlots;
    
    uint32_t defScalarType(
            DxbcScalarType          type);
    
    uint32_t defValueType(
      const DxbcValueType&          type);
    
    uint32_t defPointerType(
      const DxbcPointerType&        type);
    
    uint32_t defPerVertexBlock();
    
    DxbcPointer defVar(
      const DxbcValueType&          type,
            spv::StorageClass       storageClass);
    
  };
  
}