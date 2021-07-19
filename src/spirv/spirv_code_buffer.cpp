#include <array>
#include <cstring>

#include "spirv_code_buffer.h"

namespace dxvk {
  
  SpirvCodeBuffer:: SpirvCodeBuffer() { }
  SpirvCodeBuffer::~SpirvCodeBuffer() { }
  
  
  SpirvCodeBuffer::SpirvCodeBuffer(uint32_t size) {
    m_code.resize(size);
  }


  SpirvCodeBuffer::SpirvCodeBuffer(uint32_t size, const uint32_t* data) {
    m_code.resize(size);
    std::memcpy(m_code.data(), data, size * sizeof(uint32_t));
  }
  
  
  SpirvCodeBuffer::SpirvCodeBuffer(std::istream& stream) {
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
  
  
  uint32_t SpirvCodeBuffer::allocId() {
    constexpr size_t BoundIdsOffset = 3;

    if (this->dwords() <= BoundIdsOffset)
      return 0;

    // If we are inserting, the buffers are swapped
    if (m_ptr == not_inserting)
      return m_code[BoundIdsOffset]++;
    else
      return m_insert[BoundIdsOffset]++;
  }


  void SpirvCodeBuffer::append(const SpirvCodeBuffer& other) {
    if (other.size() != 0) {
      const size_t size = m_code.size();
      m_code.resize(size + other.m_code.size());
      
            uint32_t* dst = this->m_code.data();
      const uint32_t* src = other.m_code.data();
      
      std::memcpy(dst + size, src, other.size());
    }
  }
  
  
  void SpirvCodeBuffer::erase(size_t size) {
    if (m_ptr == not_inserting)
      return;

    // If we are inserting, the buffers are swapped
    m_insert.erase(
      m_insert.begin() + m_ptr,
      m_insert.begin() + m_ptr + size);
  }


  void SpirvCodeBuffer::store(std::ostream& stream) const {
    stream.write(
      reinterpret_cast<const char*>(m_code.data()),
      sizeof(uint32_t) * m_code.size());
  }
  
}