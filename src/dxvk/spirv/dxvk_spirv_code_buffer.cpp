#include <array>
#include <cstring>

#include "dxvk_spirv_code_buffer.h"

namespace dxvk {
  
  SpirvCodeBuffer:: SpirvCodeBuffer() { }
  SpirvCodeBuffer::~SpirvCodeBuffer() { }
  
  
  SpirvCodeBuffer::SpirvCodeBuffer(
    std::basic_istream<uint32_t>& stream)
  : m_code(
    std::istreambuf_iterator<uint32_t>(stream),
    std::istreambuf_iterator<uint32_t>()) { }
  
  
  void SpirvCodeBuffer::append(const SpirvCodeBuffer& other) {
    const size_t size = m_code.size();
    m_code.resize(size + other.m_code.size());
    
          uint32_t* dst = this->m_code.data();
    const uint32_t* src = other.m_code.data();
    
    std::memcpy(dst + size, src, sizeof(uint32_t) * size);
  }
  
  
  void SpirvCodeBuffer::putWord(uint32_t word) {
    m_code.push_back(word);
  }
  
  
  void SpirvCodeBuffer::putIns(spv::Op opCode, uint16_t wordCount) {
    this->putWord(
        (static_cast<uint32_t>(opCode)    <<  0)
      | (static_cast<uint32_t>(wordCount) << 16));
  }
  
  
  void SpirvCodeBuffer::putInt32(uint32_t word) {
    this->putWord(word);
  }
  
  
  void SpirvCodeBuffer::putInt64(uint64_t value) {
    this->putWord(value >>  0);
    this->putWord(value >> 32);
  }
  
  
  void SpirvCodeBuffer::putFloat32(float value) {
    uint32_t tmp;
    static_assert(sizeof(tmp) == sizeof(value));
    std::memcpy(&tmp, &value, sizeof(value));
    this->putInt32(tmp);
  }
  
  
  void SpirvCodeBuffer::putFloat64(double value) {
    uint64_t tmp;
    static_assert(sizeof(tmp) == sizeof(value));
    std::memcpy(&tmp, &value, sizeof(value));
    this->putInt64(tmp);
  }
  
  
  void SpirvCodeBuffer::putStr(const char* str) {
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
  
  
  void SpirvCodeBuffer::putHeader(uint32_t boundIds) {
    this->putWord(spv::MagicNumber);
    this->putWord(spv::Version);
    this->putWord(0); // Generator
    this->putWord(boundIds);
    this->putWord(0); // Schema
  }
  
  
  uint32_t SpirvCodeBuffer::strLen(const char* str) {
    // Null-termination plus padding
    return (std::strlen(str) + 4) / 4;
  }
  
  
  void SpirvCodeBuffer::store(std::basic_ostream<uint32_t>& stream) const {
    stream.write(m_code.data(), m_code.size());
  }
  
}