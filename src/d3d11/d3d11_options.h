#pragma once

#include "../util/config/config.h"

#include "../dxgi/dxgi_options.h"

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

    /// Enables speed hack for mapping on deferred contexts
    ///
    /// This can substantially speed up some games, but may
    /// cause issues if the game submits command lists more
    /// than once.
    bool dcSingleUseMode;

    /// Fakes stream output support.
    /// 
    /// Temporary hack that fixes issues in some games
    /// which technically need stream output but work
    /// well enough without it. Will be removed once
    /// Stream Output is properly supported in DXVK.
    bool fakeStreamOutSupport;

    /// Zero-initialize workgroup memory
    ///
    /// Workargound for games that don't initialize
    /// TGSM in compute shaders before reading it.
    bool zeroInitWorkgroupMemory;

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
    
    /// Back buffer count for the Vulkan swap chain.
    /// Overrides DXGI_SWAP_CHAIN_DESC::BufferCount.
    int32_t numBackBuffers;

    /// Sync interval. Overrides the value
    /// passed to IDXGISwapChain::Present.
    int32_t syncInterval;

    /// Override maximum frame latency if the app specifies
    /// a higher value. May help with frame timing issues.
    int32_t maxFrameLatency;

    /// Defer surface creation until first present call. This
    /// fixes issues with games that create multiple swap chains
    /// for a single window that may interfere with each other.
    bool deferSurfaceCreation;
  };
  
}