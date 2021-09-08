#pragma once

#include "../util/config/config.h"

#include "../dxgi/dxgi_options.h"

#include "../dxvk/dxvk_device.h"

#include "d3d11_include.h"

namespace dxvk {
  
  struct D3D11Options {
    D3D11Options(const Config& config, const Rc<DxvkDevice>& device);

    /// Enables speed hack for mapping on deferred contexts
    ///
    /// This can substantially speed up some games, but may
    /// cause issues if the game submits command lists more
    /// than once.
    bool dcSingleUseMode;

    /// Enables workaround to replace NaN render target
    /// outputs with zero
    bool enableRtOutputNanFixup;

    /// Enables out-of-bounds access check for constant
    /// buffers. Workaround for a few broken games that
    /// access random data inside their shaders.
    bool constantBufferRangeCheck;

    /// Zero-initialize workgroup memory
    ///
    /// Workargound for games that don't initialize
    /// TGSM in compute shaders before reading it.
    bool zeroInitWorkgroupMemory;

    /// Force thread-group shared memory barriers
    ///
    /// Workaround for compute shaders that read and
    /// write from the same shared memory location
    /// without explicit synchronization.
    bool forceTgsmBarriers;

    /// Use relaxed memory barriers
    ///
    /// May improve performance in some games,
    /// but might also cause rendering issues.
    bool relaxedBarriers;

    /// Ignore graphics barriers
    ///
    /// May improve performance in some games,
    /// but might also cause rendering issues.
    bool ignoreGraphicsBarriers;

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
    
    /// Declare vertex positions in shaders as invariant
    bool invariantPosition;

    /// Enable float control bits
    bool floatControls;

    /// Back buffer count for the Vulkan swap chain.
    /// Overrides DXGI_SWAP_CHAIN_DESC::BufferCount.
    int32_t numBackBuffers;

    /// Sync interval. Overrides the value
    /// passed to IDXGISwapChain::Present.
    int32_t syncInterval;

    /// Tear-free mode if vsync is disabled
    /// Tearing mode if vsync is enabled
    Tristate tearFree;

    /// Override maximum frame latency if the app specifies
    /// a higher value. May help with frame timing issues.
    int32_t maxFrameLatency;

    /// Limit frame rate
    int32_t maxFrameRate;

    /// Defer surface creation until first present call. This
    /// fixes issues with games that create multiple swap chains
    /// for a single window that may interfere with each other.
    bool deferSurfaceCreation;

    /// Forces the sample count of all textures to be 1, and
    /// performs the required shader and resolve fixups.
    bool disableMsaa;

    /// Apitrace mode: Maps all buffers in cached memory.
    /// Enabled automatically if dxgitrace.dll is attached.
    bool apitraceMode;
  };
  
}