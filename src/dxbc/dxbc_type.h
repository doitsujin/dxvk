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
    : DxbcValueType(s, c, 0) { }
    DxbcValueType(DxbcScalarType s, uint32_t c, uint32_t e)
    : componentType(s), componentCount(c), elementCount(e) { }
    
    DxbcScalarType componentType  = DxbcScalarType::Uint32;
    uint32_t       componentCount = 0;
    uint32_t       elementCount   = 0;
  };
  
  
  /**
   * \brief Value
   * 
   * Stores the type and SPIR-V ID of an expression
   * result that can be used as an operand value.
   */
  struct DxbcValue {
    DxbcValueType type;
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
    DxbcPointerType type;
    uint32_t        valueId = 0;
  };
  
  
  /**
   * \brief Component mask
   */
  class DxbcComponentMask {
    
  public:
    
    DxbcComponentMask() { }
    DxbcComponentMask(uint32_t mask)
    : m_mask(mask) { }
    DxbcComponentMask(bool x, bool y, bool z, bool w)
    : m_mask((x ? 1 : 0) | (y ? 2 : 0) | (z ? 4 : 0) | (w ? 8 : 0)) { }
    
    void set(uint32_t id) { m_mask |=  bit(id); }
    void clr(uint32_t id) { m_mask &= ~bit(id); }
    
    bool test(uint32_t id) const {
      return !!(m_mask & bit(id));
    }
    
    uint32_t componentCount() const {
      return bit::popcnt(m_mask);
    }
    
    uint32_t firstComponent() const {
      return bit::tzcnt(m_mask);
    }
    
    DxbcComponentMask operator ~ () const { return (~m_mask) & 0xF; }
    
    DxbcComponentMask operator & (const DxbcComponentMask& other) const { return m_mask & other.m_mask; }
    DxbcComponentMask operator | (const DxbcComponentMask& other) const { return m_mask | other.m_mask; }
    
    bool operator == (const DxbcComponentMask& other) const { return m_mask == other.m_mask; }
    bool operator != (const DxbcComponentMask& other) const { return m_mask != other.m_mask; }
    
    operator bool () const {
      return m_mask != 0;
    }
    
  private:
    
    uint32_t m_mask = 0;
    
    uint32_t bit(uint32_t id) const {
      return 1u << id;
    }
    
  };
  
  /**
   * \brief Component swizzle
   */
  class DxbcComponentSwizzle {
    
  public:
    
    DxbcComponentSwizzle()
    : DxbcComponentSwizzle(0, 1, 2, 3) { }
    DxbcComponentSwizzle(uint32_t x, uint32_t y, uint32_t z, uint32_t w)
    : m_components {{ x, y, z, w }} { }
    
    uint32_t  operator [] (uint32_t id) const { return m_components.at(id); }
    uint32_t& operator [] (uint32_t id)       { return m_components.at(id); }
    
    const uint32_t* operator & () const {
      return m_components.data();
    }
    
    DxbcComponentSwizzle extract(DxbcComponentMask mask) const;
    
  private:
    
    std::array<uint32_t, 4> m_components;
    
  };
  
}