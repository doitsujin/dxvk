#pragma once

#include <array>
#include <cstring>
#include <string>

namespace dxvk {
  
  using Sha1Digest = std::array<uint8_t, 20>;

  struct Sha1Data {
    const void* data;
    size_t      size;
  };
  
  class Sha1Hash {
    
  public:
    
    Sha1Hash() { }
    Sha1Hash(const Sha1Digest& digest)
    : m_digest(digest) { }
    
    std::string toString() const;
    
    uint32_t dword(uint32_t id) const {
      return uint32_t(m_digest[4 * id + 0]) <<  0
           | uint32_t(m_digest[4 * id + 1]) <<  8
           | uint32_t(m_digest[4 * id + 2]) << 16
           | uint32_t(m_digest[4 * id + 3]) << 24;
    }
    
    bool operator == (const Sha1Hash& other) const {
      return !std::memcmp(
        this->m_digest.data(),
        other.m_digest.data(),
        other.m_digest.size());
    }
    
    bool operator != (const Sha1Hash& other) const {
      return !this->operator == (other);
    }
    
    static Sha1Hash compute(
      const void*     data,
            size_t    size);
    
    static Sha1Hash compute(
            size_t    numChunks,
      const Sha1Data* chunks);
    
    template<typename T>
    static Sha1Hash compute(const T& data) {
      return compute(&data, sizeof(T));
    }

  private:
    
    Sha1Digest m_digest;
    
  };
  
}