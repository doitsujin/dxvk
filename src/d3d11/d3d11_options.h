#pragma once

#include "d3d11_include.h"

namespace dxvk {
  
  enum class D3D11Option : uint64_t {
    IgnoreMapFlagNoWait = 0,
  };
  
  using D3D11OptionSet = Flags<D3D11Option>;
  
  /**
   * \brief Retrieves per-app options
   * 
   * \param [in] AppName Executable name
   * \returns D3D11 options
   */
  D3D11OptionSet D3D11GetAppOptions(const std::string& AppName);
  
}