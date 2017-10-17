#pragma once

#include <initializer_list>

#include "dxbc_include.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V type set
   * 
   * Stores SPIR-V type definition so that
   * each type will only be declared once.
   */
  class DxbcTypeInfo {
    
  public:
    
    DxbcTypeInfo();
    ~DxbcTypeInfo();
    
    DxvkSpirvCodeBuffer code() const;
    
    uint32_t typeVoid(
            DxvkSpirvIdCounter& ids);
    
    uint32_t typeBool(
            DxvkSpirvIdCounter& ids);
    
    uint32_t typeInt(
            DxvkSpirvIdCounter& ids,
            uint32_t            width,
            uint32_t            isSigned);
    
    uint32_t typeFloat(
            DxvkSpirvIdCounter& ids,
            uint32_t            width);
    
    uint32_t typeVector(
            DxvkSpirvIdCounter& ids,
            uint32_t            componentType,
            uint32_t            componentCount);
    
    uint32_t typeMatrix(
            DxvkSpirvIdCounter& ids,
            uint32_t            colType,
            uint32_t            colCount);
    
    uint32_t typeArray(
            DxvkSpirvIdCounter& ids,
            uint32_t            elementType,
            uint32_t            elementCount);
    
    uint32_t typeRuntimeArray(
            DxvkSpirvIdCounter& ids,
            uint32_t            elementType);
    
    uint32_t typePointer(
            DxvkSpirvIdCounter& ids,
            spv::StorageClass   storageClass,
            uint32_t            type);
    
    uint32_t typeFunction(
            DxvkSpirvIdCounter& ids,
            uint32_t            returnType,
            uint32_t            argCount,
      const uint32_t*           argTypes);
    
    uint32_t typeStruct(
            DxvkSpirvIdCounter& ids,
            uint32_t            memberCount,
      const uint32_t*           memberTypes);
    
  private:
    
    DxvkSpirvCodeBuffer m_code;
    
    uint32_t getTypeId(
            DxvkSpirvIdCounter&               ids,
            spv::Op                           op,
            uint32_t                          argCount,
      const uint32_t*                         args);
    
  };
  
}