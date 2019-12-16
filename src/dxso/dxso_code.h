#pragma once

#include "dxso_include.h"
#include "dxso_reader.h"

#include <vector>
#include <cstdint>

namespace dxvk {

  /**
   * \brief DXBC code iterator
   * 
   * Convenient pointer wrapper that allows
   * reading the code token stream.
   */
  class DxsoCodeIter {
    
  public:
    
    DxsoCodeIter(
      const uint32_t* ptr)
    : m_ptr(ptr) { }
    
    const uint32_t* ptrAt(uint32_t id) const;
    
    uint32_t at(uint32_t id) const;
    uint32_t read();
    
    DxsoCodeIter skip(uint32_t n) const;
    
  private:
    
    const uint32_t* m_ptr = nullptr;
    
  };

  class DxsoCode {

  public:

    DxsoCode(DxsoReader& reader);

    DxsoCodeIter iter() const {
      return DxsoCodeIter(m_code);
    }

  private:

    const uint32_t* m_code;

  };

}