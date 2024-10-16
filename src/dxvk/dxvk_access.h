#pragma once

#include "../util/util_flags.h"

namespace dxvk {

  /**
   * \brief Resource access flags
   */
  enum class DxvkAccess : uint32_t {
    None    = 0,
    Read    = 1,
    Write   = 2,
  };

  using DxvkAccessFlags = Flags<DxvkAccess>;

}
