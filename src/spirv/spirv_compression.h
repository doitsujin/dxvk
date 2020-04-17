#pragma once

#include <vector>

#include "spirv_code_buffer.h"

#include "../util/util_raw_vector.h"

namespace dxvk {

  /**
   * \brief Compressed SPIR-V code buffer
   *
   * Implements a fast in-memory compression
   * to keep memory footprint low.
   */
  class SpirvCompressedBuffer : public SpirvWriter<SpirvCompressedBuffer> {
    static constexpr size_t not_inserting = ~(size_t)0;

  public:

    SpirvCompressedBuffer();

    SpirvCompressedBuffer(const SpirvCodeBuffer&  code);

    SpirvCompressedBuffer(SpirvCompressedBuffer&& other) = default;
    
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
    size_t size() const { return m_code.size() + m_insert.size(); }

    /**
     * \brief Appends an 32-bit word to the buffer
     * \param [in] word The word to append
     */
    void putWord(uint32_t word) {
      size_t size = m_code.size();
      m_code.resize(size + 5);

      uint8_t *dst = m_code.data() + size;
      *dst++ = word & 0x7f;
      while (word >>= 7) *dst++ = (word & 0x7f) | 0x80;
      m_code.resize(dst - m_code.data());

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

    /**
     * \brief Retrieves current insertion pointer
     *
     * Sometimes it may be necessay to insert code into the
     * middle of the stream rather than appending it. This
     * retrieves the current function pointer. Note that the
     * pointer will become invalid if any code is inserted
     * before the current pointer location.
     * \returns Current instruction pointr
     */
    size_t getInsertionPtr() const {
      return m_code.size();
    }

    /**
     * \brief Sets insertion pointer to a specific value
     *
     * Sets the insertion pointer to a value that was
     * previously retrieved by \ref getInsertionPtr.
     * \returns Current instruction pointr
     */
    void beginInsertion(size_t ptr) {
      std::swap(m_code, m_insert);
      m_ptr = ptr;
    }

    /**
     * \brief Sets insertion pointer to the end
     *
     * After this call, new instructions will be
     * appended to the stream. In other words,
     * this will restore default behaviour.
     */
    void endInsertion() {
      std::swap(m_code, m_insert);
      m_code.insert(m_code.begin() + m_ptr,
                    m_insert.begin(),
                    m_insert.end());
      m_insert.clear();
      m_ptr = not_inserting;
    }

    SpirvCodeBuffer decompress() const;

  private:

    uint32_t            m_dwords;
    raw_vector<uint8_t> m_code;
    raw_vector<uint8_t> m_insert;
    size_t m_ptr = not_inserting;

  };

}
