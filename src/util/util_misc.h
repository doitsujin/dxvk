#pragma once

namespace dxvk {

  template <typename T, size_t n>
  size_t countof(const T(&)[n]) { return n; }

}