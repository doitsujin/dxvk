#pragma once

#include "../util/config/config.h"
#include "../dxvk/dxvk_device.h"

namespace dxvk {

  enum class D3D9FloatEmulation {
    Disabled,
    Enabled,
    Strict
  };

  struct D3D9Options {

    D3D9Options(const Rc<DxvkDevice>& device, const Config& config);

    /// Override PCI vendor and device IDs reported to the
    /// application. This may make apps think they are running
    /// on a different GPU than they do and behave differently.
    int32_t customVendorId;
    int32_t customDeviceId;
    std::string customDeviceDesc;

    /// Present interval. Overrides the value
    /// in D3DPRESENT_PARAMS used in swapchain present.
    int32_t presentInterval;

    /// Override maximum frame latency if the app specifies
    /// a higher value. May help with frame timing issues.
    int32_t maxFrameLatency;

    /// Limit frame rate
    int32_t maxFrameRate;

    /// Set the max shader model the device can support in the caps.
    uint32_t shaderModel;

    /// Whether or not to set the process as DPI aware in Windows when the API interface is created.
    bool dpiAware;

    /// True:  Copy our constant set into UBO if we are relative indexing ever.
    /// False: Copy our constant set into UBO if we are relative indexing at the start of a defined constant
    /// Why?:  In theory, FXC should never generate code where this would be an issue.
    bool strictConstantCopies;

    /// Whether or not we should care about pow(0, 0) = 1
    bool strictPow;

    /// Whether or not to do a fast path clear if we're close enough to the whole render target.
    bool lenientClear;

    /// Defer surface creation
    bool deferSurfaceCreation;

    /// Anisotropic filter override
    ///
    /// Enforces anisotropic filtering with the
    /// given anisotropy value for all samplers.
    int32_t samplerAnisotropy;

    /// Max available memory override
    ///
    /// Changes the max initial value used in
    /// tracking and GetAvailableTextureMem
    uint32_t maxAvailableMemory;

    /// D3D9 Floating Point Emulation (anything * 0 = 0)
    D3D9FloatEmulation d3d9FloatEmulation;

    /// Support the DF16 & DF24 texture format
    bool supportDFFormats;

    /// Support X4R4G4B4
    bool supportX4R4G4B4;

    /// Support D16_LOCKABLE
    bool supportD16Lockable;

    /// Use D32f for D24
    bool useD32forD24;

    /// Disable D3DFMT_A8 for render targets.
    /// Specifically to work around a game
    /// bug in The Sims 2 that happens on native too!
    bool disableA8RT;

    /// Work around a NV driver quirk
    /// Fixes flickering/z-fighting in some games.
    bool invariantPosition;

    /// Whether or not to respect memory tracking for
    /// failing resource allocation.
    bool memoryTrackTest;

    /// Support VCACHE query
    bool supportVCache;

    /// Forced aspect ratio, disable other modes
    std::string forceAspectRatio;

    /// Always use a spec constant to determine sampler type (instead of just in PS 1.x)
    /// Works around a game bug in Halo CE where it gives cube textures to 2d/volume samplers
    bool forceSamplerTypeSpecConstants;

    /// Forces an MSAA level on the swapchain
    int32_t forceSwapchainMSAA;

    /// Forces sample rate shading
    bool forceSampleRateShading;

    /// Enumerate adapters by displays
    bool enumerateByDisplays;

    /// Cached dynamic buffers: Maps all buffers in cached memory.
    bool cachedDynamicBuffers;

    /// Use device local memory for constant buffers.
    bool deviceLocalConstantBuffers;

    /// Disable direct buffer mapping
    bool allowDirectBufferMapping;

    /// Don't use non seamless cube maps
    bool seamlessCubes;

    /// Mipmap LOD bias
    ///
    /// Enforces the given LOD bias for all samplers.
    float samplerLodBias;

    /// Clamps negative LOD bias
    bool clampNegativeLodBias;

    /// How much virtual memory will be used for textures (in MB).
    int32_t textureMemory;

    /// Shader dump path
    std::string shaderDumpPath;

    /// Enable emulation of device loss when a fullscreen app loses focus
    bool deviceLossOnFocusLoss;

    /// Disable counting losable resources and rejecting calls to Reset() if any are still alive
    bool countLosableResources;

    /// Ensure that for the same D3D commands the output VK commands
    /// don't change between runs. Useful for comparative benchmarking,
    /// can negatively affect performance.
    bool reproducibleCommandStream;

    /// Enable depth texcoord Z (Dref) scaling (D3D8 quirk)
    int32_t drefScaling;

    /// Add an extra front buffer to make GetFrontBufferData() work correctly when the swapchain only has a single buffer
    bool extraFrontbuffer;
  };

}
