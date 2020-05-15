#pragma once

#include <iostream>
#include <utility>
#include <vector>

#include "spirv_instruction.h"
#include "spirv_writer.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V code buffer
   * 
   * Helper class for generating SPIR-V shaders.
   * Stores arbitrary SPIR-V instructions in a
   * format that can be read by Vulkan drivers.
   */
  class SpirvCodeBuffer : public SpirvWriter<SpirvCodeBuffer> {
    static constexpr size_t not_inserting = ~(size_t)0;

  public:
    
    SpirvCodeBuffer();
    explicit SpirvCodeBuffer(uint32_t size);
    SpirvCodeBuffer(uint32_t size, const uint32_t* data);
    SpirvCodeBuffer(std::istream& stream);
    
    template<size_t N>
    SpirvCodeBuffer(const uint32_t (&data)[N])
    : SpirvCodeBuffer(N, data) { }
    
    ~SpirvCodeBuffer();
    
    /**
     * \brief Code data
     * \returns Code data
     */
    const uint32_t* data() const { return m_code.data(); }
          uint32_t* data()       { return m_code.data(); }
    
    /**
     * \brief Code size, in dwords
     * \returns Code size, in dwords
     */
    uint32_t dwords() const {
      return m_code.size() + m_insert.size();
    }
    
    /**
     * \brief Code size, in bytes
     * \returns Code size, in bytes
     */
    size_t size() const {
      return this->dwords() * sizeof(uint32_t);
    }
    
    /**
     * \brief Begin instruction iterator
     * 
     * Points to the first instruction in the instruction
     * block. The header, if any, will be skipped over.
     * \returns Instruction iterator
     */
    SpirvInstructionIterator begin() {
      return SpirvInstructionIterator(
        m_code.data(), 0, m_code.size());
    }
    
    /**
     * \brief End instruction iterator
     * 
     * Points to the end of the instruction block.
     * \returns Instruction iterator
     */
    SpirvInstructionIterator end() {
      return SpirvInstructionIterator(nullptr, 0, 0);
    }
    
    /**
     * \brief Allocates a new ID
     *
     * Returns a new valid ID and increments the
     * maximum ID count stored in the header.
     * \returns The new SPIR-V ID
     */
    uint32_t allocId();
    
    /**
     * \brief Merges two code buffers
     * 
     * This is useful to generate declarations or
     * the SPIR-V header at the same time as the
     * code when doing so in advance is impossible.
     * \param [in] other Code buffer to append
     */
    void append(const SpirvCodeBuffer& other);
    
    /**
     * \brief Appends an 32-bit word to the buffer
     * \param [in] word The word to append
     */
    void putWord(uint32_t word) {
      m_code.push_back(word);
    }

    /**
     * \brief Erases given number of dwords
     *
     * Removes data from the code buffer, starting
     * at the current insertion offset.
     * \param [in] size Number of words to remove
     */
    void erase(size_t size);
    
    /**
     * \brief Stores the SPIR-V module to a stream
     * 
     * The ability to save modules to a file
     * exists mostly for debugging purposes.
     * \param [in] stream Output stream
     */
    void store(std::ostream& stream) const;
    
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
     * \returns Previous instruction pointer
     */
    void endInsertion() {
      std::swap(m_code, m_insert);
      m_code.insert(m_code.begin() + m_ptr,
                    m_insert.begin(),
                    m_insert.end());
      m_insert.clear();
      m_ptr = not_inserting;
    }
    
  private:
    
    std::vector<uint32_t> m_code;
    std::vector<uint32_t> m_insert;
    size_t m_ptr = not_inserting;
    
  };
  
}
