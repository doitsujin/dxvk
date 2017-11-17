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
     * \brief Defines 32-bit constant
     * 
     * The constant will be declared as a 32-bit
     * unsigned integer. Cast the resulting value
     * to the required type.
     * \param [in] v Constant value
     * \returns The constant value ID
     */
    DxbcValue defConstScalar(uint32_t v);
    
    /**
     * \brief Defines 32-bit constant vector
     * 
     * Defines a four-component vector of 32-bit
     * unsigned integer values. Cast the resulting
     * value to the required type as needed.
     * \param [in] x First vector component
     * \param [in] y Second vector component
     * \param [in] z Third vector component
     * \param [in] w Fourth vector component
     * \returns The constant value ID
     */
    DxbcValue defConstVector(
            uint32_t x, uint32_t y,
            uint32_t z, uint32_t w);
    
    /**
     * \brief Returns from function
     */
    void fnReturn();
    
    /**
     * \brief Retrieves temporary register pointer
     * 
     * Provides access to a temporary register.
     * \param [in] regId Register index
     * \returns Register pointer
     */
    DxbcPointer ptrTempReg(
            uint32_t          regId);
    
    /**
     * \brief Pointer to an interface variable
     * 
     * Provides access to an interface variable.
     * \param [in] regType Register type
     * \param [in] regId Register index
     * \returns Register pointer
     */
    virtual DxbcPointer ptrInterfaceVar(
            DxbcOperandType       regType,
            uint32_t              regId) = 0;
    
    /**
     * \brief Pointer to an interface variable
     * 
     * Provides access to an indexed interface variable.
     * Some shader types may have indexed input or output
     * variables that can be accesswed via an array index.
     * \param [in] regType Register type
     * \param [in] regId Register index
     * \param [in] index Array index
     * \returns Register pointer
     */
    virtual DxbcPointer ptrInterfaceVarIndexed(
            DxbcOperandType       regType,
            uint32_t              regId,
      const DxbcValue&            index) = 0;
    
    DxbcValue opAbs(
      const DxbcValue&            src);
    
    DxbcValue opAdd(
      const DxbcValue&            a,
      const DxbcValue&            b);
    
    DxbcValue opMul(
      const DxbcValue&            a,
      const DxbcValue&            b);
    
    DxbcValue opNeg(
      const DxbcValue&            src);
    
    DxbcValue opSaturate(
      const DxbcValue&            src);
    
    /**
     * \brief Casts register value to another type
     * 
     * Type cast that does not change the bit pattern
     * of the value. This is required as DXBC values
     * are not statically typed, but SPIR-V is.
     * \param [in] src Source value
     * \param [in] type Destination type
     * \returns Resulting register value
     */
    DxbcValue regCast(
      const DxbcValue&            src,
      const DxbcValueType&        type);
    
    /**
     * \brief Extracts vector components
     * 
     * Extracts the given set of components.
     * \param [in] src Source vector
     * \param [in] mask Component mask
     * \returns Resulting register value
     */
    DxbcValue regExtract(
      const DxbcValue&            src,
            DxbcComponentMask     mask);
    
    /**
     * \brief Swizzles a vector register
     * 
     * Swizzles the vector and extracts
     * the given set of vector components.
     * \param [in] src Source vector to swizzle
     * \param [in] swizzle The component swizzle
     * \param [in] mask Components to extract
     * \returns Resulting register value
     */
    DxbcValue regSwizzle(
      const DxbcValue&            src,
      const DxbcComponentSwizzle& swizzle,
            DxbcComponentMask     mask);
    
    /**
     * \brief Writes to parts of a vector register
     * 
     * Note that the source value must have the same
     * number of components as the write mask.
     * \param [in] dst Destination value ID
     * \param [in] src Source value ID
     * \param [in] mask Write mask
     * \returns New destination value ID
     */
    DxbcValue regInsert(
      const DxbcValue&            dst,
      const DxbcValue&            src,
            DxbcComponentMask     mask);
    
    /**
     * \brief Loads register
     * 
     * \param [in] ptr Register pointer
     * \returns The register value ID
     */
    DxbcValue regLoad(
      const DxbcPointer&          ptr);
    
    /**
     * \brief Stores register
     * 
     * \param [in] ptr Register pointer
     * \param [in] val Value ID to store
     * \param [in] mask Write mask
     */
    void regStore(
      const DxbcPointer&          ptr,
      const DxbcValue&            val,
            DxbcComponentMask     mask);
    
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
    
    DxbcPointer defVar(
      const DxbcValueType&          type,
            spv::StorageClass       storageClass);
    
  };
  
}