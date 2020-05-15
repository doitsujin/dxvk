#pragma once

#include "spirv_instruction.h"

namespace dxvk {
  
  /**
   * \brief SPIR-V code buffer
   * 
   * Helper class for generating SPIR-V shaders.
   * Stores arbitrary SPIR-V instructions in a
   * format that can be read by Vulkan drivers.
   */
  template<class SpirvBuffer>
  class SpirvWriter {
    
  public:
    
    /**
     * \brief Appends an 32-bit word to the buffer
     * \param [in] word The word to append
     */
    void putWord(uint32_t word) {
      static_cast<SpirvBuffer*>(this)->putWord(word);
    }
    
    /**
     * \brief Appends an instruction word to the buffer
     * 
     * Adds a single word containing both the word count
     * and the op code number for a single instruction.
     * \param [in] opCode Operand code
     * \param [in] wordCount Number of words
     */
    void putIns(spv::Op opCode, uint16_t wordCount) {
      this->putWord(
          (static_cast<uint32_t>(opCode)    <<  0)
        | (static_cast<uint32_t>(wordCount) << 16));
    }
    
    /**
     * \brief Appends a 32-bit integer to the buffer
     * \param [in] value The number to add
     */
    void putInt32(uint32_t word) {
      this->putWord(word);
    }
    
    /**
     * \brief Appends a 64-bit integer to the buffer
     * 
     * A 64-bit integer will take up two 32-bit words.
     * \param [in] value 64-bit value to add
     */
    void putInt64(uint64_t value) {
      this->putWord(value >>  0);
      this->putWord(value >> 32);
    }
    
    /**
     * \brief Appends a 32-bit float to the buffer
     * \param [in] value The number to add
     */
    void putFloat32(float value) {
      this->putInt32(bit::cast<uint32_t>(value));
    }
    
    /**
     * \brief Appends a 64-bit float to the buffer
     * \param [in] value The number to add
     */
    void putFloat64(double value) {
      this->putInt64(bit::cast<uint64_t>(value));
    }
    
    /**
     * \brief Appends a literal string to the buffer
     * \param [in] str String to append to the buffer
     */
    void putStr(const char* str) {
      uint32_t word = 0;
      uint32_t nbit = 0;
      
      for (uint32_t i = 0; str[i] != '\0'; str++) {
        word |= (static_cast<uint32_t>(str[i]) & 0xFF) << nbit;
        
        if ((nbit += 8) == 32) {
          this->putWord(word);
          word = 0;
          nbit = 0;
        }
      }
      
      // Commit current word
      this->putWord(word);
    }
    
    /**
     * \brief Adds the header to the buffer
     *
     * \param [in] version SPIR-V version
     * \param [in] boundIds Number of bound IDs
     */
    void putHeader(uint32_t version, uint32_t boundIds) {
      this->putWord(spv::MagicNumber);
      this->putWord(version);
      this->putWord(0); // Generator
      this->putWord(boundIds);
      this->putWord(0); // Schema
    }
    
    /**
     * \brief Computes length of a literal string
     * 
     * \param [in] str The string to check
     * \returns Number of words consumed by a string
     */
    uint32_t strLen(const char* str)  {
      // Null-termination plus padding
      return (std::strlen(str) + 4) / 4;
    }

  };
  
}
