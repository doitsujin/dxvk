#pragma once

#include "../spirv_code_buffer.h"

#include "spirv_gen_id.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V type set
   * 
   * Stores SPIR-V type definition so that
   * each type will only be declared once.
   */
  class SpirvTypeInfo {
    
  public:
    
    SpirvTypeInfo();
    ~SpirvTypeInfo();
    
    SpirvCodeBuffer code() const;
    
    uint32_t typeVoid(
            SpirvIdCounter&     ids);
    
    uint32_t typeBool(
            SpirvIdCounter&     ids);
    
    uint32_t typeInt(
            SpirvIdCounter&     ids,
            uint32_t            width,
            uint32_t            isSigned);
    
    uint32_t typeFloat(
            SpirvIdCounter&     ids,
            uint32_t            width);
    
    uint32_t typeVector(
            SpirvIdCounter&     ids,
            uint32_t            componentType,
            uint32_t            componentCount);
    
    uint32_t typeMatrix(
            SpirvIdCounter&     ids,
            uint32_t            colType,
            uint32_t            colCount);
    
    uint32_t typeArray(
            SpirvIdCounter&     ids,
            uint32_t            elementType,
            uint32_t            elementCount);
    
    uint32_t typeRuntimeArray(
            SpirvIdCounter&     ids,
            uint32_t            elementType);
    
    uint32_t typePointer(
            SpirvIdCounter&     ids,
            spv::StorageClass   storageClass,
            uint32_t            type);
    
    uint32_t typeFunction(
            SpirvIdCounter&     ids,
            uint32_t            returnType,
            uint32_t            argCount,
      const uint32_t*           argTypes);
    
    uint32_t typeStruct(
            SpirvIdCounter&     ids,
            uint32_t            memberCount,
      const uint32_t*           memberTypes);
    
  private:
    
    SpirvCodeBuffer m_code;
    
    uint32_t getTypeId(
            SpirvIdCounter&     ids,
            spv::Op             op,
            uint32_t            argCount,
      const uint32_t*           args);
    
  };
  
}