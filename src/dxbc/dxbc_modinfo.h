#pragma once

#include "dxbc_options.h"

namespace dxvk {

  /**
   * \brief Tessellation info
   * 
   * Stores the maximum tessellation factor
   * to export from tessellation shaders.
   */
  struct DxbcTessInfo {
    float maxTessFactor;
  };


  /**
   * \brief Shader module info
   * 
   * Stores information which may affect shader compilation.
   * This data can be supplied by the client API implementation.
   */
  struct DxbcModuleInfo {
    DxbcOptions   options;
    DxbcTessInfo* tess;
  };

}