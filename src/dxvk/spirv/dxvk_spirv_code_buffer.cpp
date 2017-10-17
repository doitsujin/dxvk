#include <array>
#include <cstring>

#include "dxvk_spirv_code_buffer.h"

namespace dxvk {
  
  DxvkSpirvCodeBuffer:: DxvkSpirvCodeBuffer() { }
  DxvkSpirvCodeBuffer::~DxvkSpirvCodeBuffer() { }
  
  
  DxvkSpirvCodeBuffer::DxvkSpirvCodeBuffer(std::istream&& stream) {
    stream.ignore(std::numeric_limits<std::streamsize>::max());
    std::streamsize length = stream.gcount();
    stream.clear();
    stream.seekg(0, std::ios_base::beg);
    
    std::vector<char> buffer(length);
    stream.read(buffer.data(), length);
    buffer.resize(stream.gcount());
    
    m_code.resize(buffer.size() / sizeof(uint32_t));
    std::memcpy(reinterpret_cast<char*>(m_code.data()),
      buffer.data(), m_code.size() * sizeof(uint32_t));
  }
  
  
  void DxvkSpirvCodeBuffer::append(const DxvkSpirvCodeBuffer& other) {
    if (other.size() != 0) {
      const size_t size = m_code.size();
      m_code.resize(size + other.m_code.size());
      
            uint32_t* dst = this->m_code.data();
      const uint32_t* src = other.m_code.data();
      
      std::memcpy(dst + size, src, other.size());
    }
  }
  
  
  void DxvkSpirvCodeBuffer::putWord(uint32_t word) {
    m_code.push_back(word);
  }
  
  
  void DxvkSpirvCodeBuffer::putIns(spv::Op opCode, uint16_t wordCount) {
    this->putWord(
        (static_cast<uint32_t>(opCode)    <<  0)
      | (static_cast<uint32_t>(wordCount) << 16));
  }
  
  
  void DxvkSpirvCodeBuffer::putInt32(uint32_t word) {
    this->putWord(word);
  }
  
  
  void DxvkSpirvCodeBuffer::putInt64(uint64_t value) {
    this->putWord(value >>  0);
    this->putWord(value >> 32);
  }
  
  
  void DxvkSpirvCodeBuffer::putFloat32(float value) {
    uint32_t tmp;
    static_assert(sizeof(tmp) == sizeof(value));
    std::memcpy(&tmp, &value, sizeof(value));
    this->putInt32(tmp);
  }
  
  
  void DxvkSpirvCodeBuffer::putFloat64(double value) {
    uint64_t tmp;
    static_assert(sizeof(tmp) == sizeof(value));
    std::memcpy(&tmp, &value, sizeof(value));
    this->putInt64(tmp);
  }
  
  
  void DxvkSpirvCodeBuffer::putStr(const char* str) {
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
  
  
  void DxvkSpirvCodeBuffer::putHeader(uint32_t boundIds) {
    this->putWord(spv::MagicNumber);
    this->putWord(spv::Version);
    this->putWord(0); // Generator
    this->putWord(boundIds);
    this->putWord(0); // Schema
  }
  
  
  uint32_t DxvkSpirvCodeBuffer::strLen(const char* str) {
    // Null-termination plus padding
    return (std::strlen(str) + 4) / 4;
  }
  
  
  void DxvkSpirvCodeBuffer::store(std::ostream&& stream) const {
    stream.write(
      reinterpret_cast<const char*>(m_code.data()),
      sizeof(uint32_t) * m_code.size());
  }
  
}