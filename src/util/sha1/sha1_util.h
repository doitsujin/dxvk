#pragma once

#include <array>
#include <cstring>
#include <string>

namespace dxvk {
  
  using Sha1Digest = std::array<uint8_t, 20>;
  
  class Sha1Hash {
    
  public:
    
    Sha1Hash() { }
    Sha1Hash(const Sha1Digest& digest)
    : m_digest(digest) { }
    
    std::string toString() const;
    
    const uint8_t* digest() const {
      return m_digest.data();
    }
    
    bool operator == (const Sha1Hash& other) const {
      return !std::memcmp(
        this->m_digest.data(),
        other.m_digest.data(),
        other.m_digest.size());
    }
    
    static Sha1Hash compute(
      const uint8_t*  data,
            size_t    size);
    
  private:
    
    Sha1Digest m_digest;
    
  };
  
}