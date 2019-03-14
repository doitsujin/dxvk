#pragma once

#include "dxso_include.h"
#include "dxso_reader.h"

#include <vector>
#include <cstdint>

namespace dxvk {

  /**
   * \brief DXBC code slice
   * 
   * Convenient pointer pair that allows
   * reading the code word stream safely.
   */
  class DxsoCodeSlice {
    
  public:
    
    DxsoCodeSlice(
      const uint32_t* ptr,
      const uint32_t* end)
    : m_ptr(ptr), m_end(end) { }
    
    const uint32_t* ptrAt(uint32_t id) const;
    
    uint32_t at(uint32_t id) const;
    uint32_t read();
    
    DxsoCodeSlice take(uint32_t n) const;
    DxsoCodeSlice skip(uint32_t n) const;
    
    bool atEnd() const {
      return m_ptr == m_end;
    }
    
  private:
    
    const uint32_t* m_ptr = nullptr;
    const uint32_t* m_end = nullptr;
    
  };

  class DxsoCode : public RcObject {

  public:

    DxsoCode(DxsoReader& reader);

    DxsoCodeSlice slice() const {
      return DxsoCodeSlice(m_code.data(),
        m_code.data() + m_code.size());
    }

  private:

    std::vector<uint32_t> m_code;

  };

}