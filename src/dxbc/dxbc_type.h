#pragma once

#include "dxbc_include.h"

namespace dxvk {
  
  /**
   * \brief Scalar value type
   * 
   * Enumerates possible register component
   * types. Scalar types are represented as
   * a one-component vector type.
   */
  enum class DxbcScalarType {
    Uint32    = 0,
    Uint64    = 1,
    Sint32    = 2,
    Sint64    = 3,
    Float32   = 4,
    Float64   = 5,
  };
  
  
  /**
   * \brief Vector value type
   * 
   * Vector type definition that stores the scalar
   * component type and the number of components.
   */
  struct DxbcValueType {
    DxbcValueType() { }
    DxbcValueType(DxbcScalarType s, uint32_t c)
    : componentType(s), componentCount(c) { }
    
    DxbcScalarType componentType  = DxbcScalarType::Uint32;
    uint32_t       componentCount = 0;
  };
  
  
  /**
   * \brief Value
   * 
   * Stores the type and SPIR-V ID of an expression
   * result that can be used as an operand value.
   */
  struct DxbcValue {
    DxbcValue() { }
    DxbcValue(
      DxbcValueType p_type,
      uint32_t      p_typeId,
      uint32_t      p_valueId)
    : type    (p_type),
      typeId  (p_typeId),
      valueId (p_valueId) { }
    
    DxbcValueType type;
    uint32_t      typeId  = 0;
    uint32_t      valueId = 0;
  };
  
  
  /**
   * \brief Pointer type
   * 
   * Stores the type of data that the pointer will
   * point to, as well as the storage class of the
   * SPIR-V object.
   */
  struct DxbcPointerType {
    DxbcPointerType() { }
    DxbcPointerType(
      DxbcValueType     p_valueType,
      spv::StorageClass p_storageClass)
    : valueType   (p_valueType),
      storageClass(p_storageClass) { }
    
    DxbcValueType     valueType;
    spv::StorageClass storageClass = spv::StorageClassGeneric;
  };
  
  
  /**
   * \brief Pointer
   * 
   * Stores the SPIR-V ID of a pointer value and
   * the type of the pointer, including its storage
   * class. Can be used as a memory operand.
   */
  struct DxbcPointer {
    DxbcPointer() { }
    DxbcPointer(
      DxbcPointerType   p_type,
      uint32_t          p_typeId,
      uint32_t          p_valueId)
    : type    (p_type),
      typeId  (p_typeId),
      valueId (p_valueId) { }
    
    DxbcPointerType type;
    uint32_t        typeId  = 0;
    uint32_t        valueId = 0;
  };
  
}