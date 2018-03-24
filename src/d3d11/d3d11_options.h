#pragma once

#include "d3d11_include.h"

namespace dxvk {
  
  enum class D3D11Option : uint64_t {
    /**
     * \brief Handle D3D11_MAP_FLAG_DO_NOT_WAIT properly
     * 
     * This can offer substantial speedups, but some games
     * (The Witcher 3, Elder Scrolls Online, possibly others)
     * seem to make incorrect assumptions about when a map
     * operation succeeds when that flag is set.
     */
    AllowMapFlagNoWait = 0,
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