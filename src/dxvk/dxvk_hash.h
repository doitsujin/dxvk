#pragma once

#include <cstddef>

#include "../util/util_env.h"

namespace dxvk {

  struct DxvkEq {
    template<typename T>
    size_t operator () (const T& a, const T& b) const {
      return a.eq(b);
    }
  };

  struct DxvkHash {
    template<typename T>
    size_t operator () (const T& object) const {
      return object.hash();
    }
  };
  
  class DxvkHashState {
    static constexpr size_t Offset = env::is32BitHostPlatform()
      ? size_t(0x811c9dc5u)
      : size_t(0xcbf29ce484222325ull);

    static constexpr size_t Prime = env::is32BitHostPlatform()
      ? size_t(0x01000193u)
      : size_t(0x00000100000001b3ull);
  public:

    void add(size_t hash) {
      m_value ^= hash;
      m_value *= Prime;
    }

    operator size_t () const {
      return m_value;
    }

  private:

    size_t m_value = Offset;

  };

}
