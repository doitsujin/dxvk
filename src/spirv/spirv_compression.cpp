#include "spirv_compression.h"

namespace dxvk {

  SpirvCompressedBuffer::SpirvCompressedBuffer()
  : m_dwords(0) {

  }


  SpirvCompressedBuffer::SpirvCompressedBuffer(
    const SpirvCodeBuffer& code)
  : m_dwords(code.dwords()) {
    size_t dwords = m_dwords;
    if (dwords == 0)
      return;

    const uint32_t *src = code.data();
    const uint32_t *end = src + dwords;
    do {
      uint32_t word = *src++;
      m_code.push_back(word & 0x7f);
      while (word >>= 7)
        m_code.push_back((word & 0x7f) | 0x80);
    } while (src != end);

    m_code.shrink_to_fit();
  }

    
  SpirvCompressedBuffer::~SpirvCompressedBuffer() {

  }


  SpirvCodeBuffer SpirvCompressedBuffer::decompress() const {
    SpirvCodeBuffer code(m_dwords);
    if (m_dwords == 0)
      return code;

    uint32_t *dst = code.data();
    const uint8_t *src = m_code.data();
    const uint8_t *end = src + m_code.size();
    do {
      *dst = *src++;
      if (src == end || !(*src & 0x80)) continue;
      *dst |= (*src++ & 0x7f) << 7;
      if (src == end || !(*src & 0x80)) continue;
      *dst |= (*src++ & 0x7f) << 14;
      if (src == end || !(*src & 0x80)) continue;
      *dst |= (*src++ & 0x7f) << 21;
      if (src == end || !(*src & 0x80)) continue;
      *dst |= (*src++ & 0x7f) << 28;
    } while (++dst, src != end);

    return code;
  }

}
