#include "dxso_code.h"

namespace dxvk {

  DxsoCode::DxsoCode(DxsoReader& reader) {
    m_code =
      reinterpret_cast<const uint32_t*>(reader.currentPtr());
  }

  const uint32_t* DxsoCodeIter::ptrAt(uint32_t id) const {
    return m_ptr + id;
  }


  uint32_t DxsoCodeIter::at(uint32_t id) const {
    return m_ptr[id];
  }


  uint32_t DxsoCodeIter::read() {
    return *(m_ptr++);
  }

  DxsoCodeIter DxsoCodeIter::skip(uint32_t n) const {
    return DxsoCodeIter(m_ptr + n);
  }

}