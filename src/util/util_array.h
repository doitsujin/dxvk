#pragma once

#include <array>
#include <utility>

namespace dxvk {

  template <typename V, typename... T>
  constexpr std::array<V, sizeof...(T)> array_of(T&&... t) {
    return {{ std::forward<T>(t)... }};
  }

}
