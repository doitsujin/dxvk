#include "dxso_reader.h"

#include <cstring>

namespace dxvk {

  DxbcTag DxsoReader::readTag() {
    DxbcTag tag;
    this->read(&tag, 4);
    return tag;
  }

  void DxsoReader::read(void* dst, size_t n) {
    if (m_pos + n > m_size)
      throw DxvkError("DxsoReader::read: Unexpected end of file");
    std::memcpy(dst, m_data + m_pos, n);
    m_pos += n;
  }

  void DxsoReader::skip(size_t n) {
    if (m_pos + n > m_size)
      throw DxvkError("DxsoReader::skip: Unexpected end of file");
    m_pos += n;
  }

  DxsoReader DxsoReader::clone(size_t pos) const {
    if (pos > m_size)
      throw DxvkError("DxsoReader::clone: Invalid offset");
    return DxsoReader(m_data + pos, m_size - pos);
  }


  DxsoReader DxsoReader::resize(size_t size) const {
    if (size > m_size)
      throw DxvkError("DxsoReader::resize: Invalid size");
    return DxsoReader(m_data, size, m_pos);
  }


  void DxsoReader::store(std::ostream && stream) const {
    stream.write(m_data, m_size);
  }

}