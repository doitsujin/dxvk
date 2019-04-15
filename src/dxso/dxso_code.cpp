#include "dxso_code.h"

namespace dxvk {

  DxsoCode::DxsoCode(DxsoReader& reader) {
    size_t codeSize = reader.remaining();

    m_code.resize(codeSize);
    reader.read(m_code.data(), codeSize);
  }

  const uint32_t* DxsoCodeSlice::ptrAt(uint32_t id) const {
    if (m_ptr + id >= m_end)
      throw DxvkError("DxsoCodeSlice: End of stream");
    return m_ptr + id;
  }


  uint32_t DxsoCodeSlice::at(uint32_t id) const {
    if (m_ptr + id >= m_end)
      throw DxvkError("DxsoCodeSlice: End of stream");
    return m_ptr[id];
  }


  uint32_t DxsoCodeSlice::read() {
    if (m_ptr >= m_end)
      throw DxvkError("DxsoCodeSlice: End of stream");
    return *(m_ptr++);
  }


  DxsoCodeSlice DxsoCodeSlice::take(uint32_t n) const {
    if (m_ptr + n > m_end)
      throw DxvkError("DxsoCodeSlice: End of stream");
    return DxsoCodeSlice(m_ptr, m_ptr + n);
  }


  DxsoCodeSlice DxsoCodeSlice::skip(uint32_t n) const {
    if (m_ptr + n > m_end)
      throw DxvkError("DxsoCodeSlice: End of stream");
    return DxsoCodeSlice(m_ptr + n, m_end);
  }

}