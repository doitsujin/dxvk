#include "sha1.h"
#include "sha1_util.h"

namespace dxvk {
  
  std::string Sha1Hash::toString() const {
    static const char nibbles[]
      = { '0', '1', '2', '3', '4', '5', '6', '7',
          '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    
    std::string result;
    result.resize(2 * m_digest.size());
    
    for (uint32_t i = 0; i < m_digest.size(); i++) {
      result.at(2 * i + 0) = nibbles[(m_digest[i] >> 4) & 0xF];
      result.at(2 * i + 1) = nibbles[(m_digest[i] >> 0) & 0xF];
    }
    
    return result;
  }
  
  
  Sha1Hash Sha1Hash::compute(
    const uint8_t*  data,
          size_t    size) {
    Sha1Digest digest;
    
    SHA1_CTX ctx;
    SHA1Init(&ctx);
    SHA1Update(&ctx, data, size);
    SHA1Final(digest.data(), &ctx);
    return Sha1Hash(digest);
  }
  
}