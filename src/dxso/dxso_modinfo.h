#pragma once

#include "dxso_options.h"

namespace dxvk {

  /**
   * \brief Shader module info
   *
   * Stores information which may affect shader compilation.
   * This data can be supplied by the client API implementation.
   */
  struct DxsoModuleInfo {
    DxsoOptions options;
  };

}