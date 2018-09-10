#pragma once

#include "../util/config/config.h"

#include "d3d11_include.h"

namespace dxvk {
  
  struct D3D11Options {
    D3D11Options(const Config& config);
    /// Handle D3D11_MAP_FLAG_DO_NOT_WAIT properly.
    /// 
    /// This can offer substantial speedups, but some games
    /// (The Witcher 3, Elder Scrolls Online, possibly others)
    /// seem to make incorrect assumptions about when a map
    /// operation succeeds when that flag is set.
    bool allowMapFlagNoWait;

    /// Fakes stream output support.
    /// 
    /// Temporary hack that fixes issues in some games
    /// which technically need stream output but work
    /// well enough without it. Will be removed once
    /// Stream Output is properly supported in DXVK.
    bool fakeStreamOutSupport;

    /// Maximum tessellation factor.
    ///
    /// Limits tessellation factors in tessellation
    /// control shaders. Values from 8 to 64 are
    /// supported, other values will be ignored.
    int32_t maxTessFactor;

    /// Anisotropic filter override
    ///
    /// Enforces anisotropic filtering with the
    /// given anisotropy value for all samplers.
    int32_t samplerAnisotropy;
  };
  
}