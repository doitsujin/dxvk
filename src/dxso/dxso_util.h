#pragma once

#include <cstdint>

namespace dxvk {

  /**
   * \brief Returns the length of d3d9 shader bytecode in bytes.
   */
  size_t DXSOBytecodeLength(const uint32_t* pFunction);

}