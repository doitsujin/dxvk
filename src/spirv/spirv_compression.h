#pragma once

#include <vector>

#include "spirv_code_buffer.h"

namespace dxvk {

  /**
   * \brief Compressed SPIR-V code buffer
   *
   * Implements a fast in-memory compression
   * to keep memory footprint low.
   */
  class SpirvCompressedBuffer : public SpirvWriter<SpirvCompressedBuffer> {

  public:

    SpirvCompressedBuffer();

    SpirvCompressedBuffer(const SpirvCodeBuffer& code);
    
    ~SpirvCompressedBuffer();
    
    void shrink() { m_code.shrink_to_fit(); }

    /**
     * \brief Code size, in dwords
     * \returns Code size, in dwords
     */
    uint32_t dwords() const { return m_dwords; }

    /**
     * \brief Code size, in bytes
     * \returns Code size, in bytes
     */
    size_t size() const { return m_code.size(); }

    /**
     * \brief Appends an 32-bit word to the buffer
     * \param [in] word The word to append
     */
    void putWord(uint32_t word) {
      m_code.push_back(word & 0x7f);
      while (word >>= 7)
        m_code.push_back((word & 0x7f) | 0x80);
      m_dwords += 1;
    }

    /**
     * \brief Merges two code buffers
     *
     * This is useful to generate declarations or
     * the SPIR-V header at the same time as the
     * code when doing so in advance is impossible.
     * \param [in] other Code buffer to append
     */
    void append(const SpirvCompressedBuffer& other) {
      m_code.insert(m_code.end(),
                    other.m_code.begin(),
                    other.m_code.end());
      m_dwords += other.dwords();
    }

    SpirvCodeBuffer decompress() const;

  private:

    uint32_t             m_dwords;
    std::vector<uint8_t> m_code;

  };

}
