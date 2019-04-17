#include "dxso_reader.h"

#include <cstring>

namespace dxvk {

  DxbcTag DxsoReader::readTag() {
    DxbcTag tag;
    this->read(&tag, 4);
    return tag;
  }

  void DxsoReader::read(void* dst, size_t n) {
    std::memcpy(dst, m_data + m_pos, n);
    m_pos += n;
  }

  void DxsoReader::skip(size_t n) {
    m_pos += n;
  }

  void DxsoReader::store(std::ostream && stream, size_t size) const {
    stream.write(m_data, size);
  }

}