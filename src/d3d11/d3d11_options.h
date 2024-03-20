#pragma once

#include "../util/config/config.h"

#include "../dxgi/dxgi_options.h"

#include "../dxvk/dxvk_device.h"

#include "d3d11_include.h"

namespace dxvk {

  struct D3D11Options {
    D3D11Options(const Config& config);

    /// Enables speed hack for mapping on deferred contexts
    ///
    /// This can substantially speed up some games, but may
    /// cause issues if the game submits command lists more
    /// than once.
    bool dcSingleUseMode;

    /// Zero-initialize workgroup memory
    ///
    /// Workargound for games that don't initialize
    /// TGSM in compute shaders before reading it.
    bool zeroInitWorkgroupMemory;

    /// Force thread-group shared memory accesses to be volatile
    ///
    /// Workaround for compute shaders that read and
    /// write from the same shared memory location
    /// without explicit synchronization.
    bool forceVolatileTgsmAccess;

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

    /// Mipmap LOD bias
    ///
    /// Enforces the given LOD bias for all samplers.
    float samplerLodBias;

    /// Clamps negative LOD bias
    bool clampNegativeLodBias;

    /// Declare vertex positions in shaders as invariant
    bool invariantPosition;

    /// Enable float control bits
    bool floatControls;

    /// Back buffer count for the Vulkan swap chain.
    /// Overrides DXGI_SWAP_CHAIN_DESC::BufferCount.
    int32_t numBackBuffers;

    /// Override maximum frame latency if the app specifies
    /// a higher value. May help with frame timing issues.
    int32_t maxFrameLatency;

    /// Limit frame rate
    int32_t maxFrameRate;

    /// Limit discardable resource size
    VkDeviceSize maxImplicitDiscardSize;

    /// Limit size of buffer-mapped images
    VkDeviceSize maxDynamicImageBufferSize;

    /// Defer surface creation until first present call. This
    /// fixes issues with games that create multiple swap chains
    /// for a single window that may interfere with each other.
    bool deferSurfaceCreation;

    /// Enables sample rate shading by interpolating fragment shader
    /// inputs at the sample location rather than pixel center,
    /// unless otherwise specified by the application.
    bool forceSampleRateShading;

    /// Forces the sample count of all textures to be 1, and
    /// performs the required shader and resolve fixups.
    bool disableMsaa;

    /// Dynamic resources with the given bind flags will be allocated
    /// in cached system memory. Enabled automatically when recording
    /// an api trace.
    uint32_t cachedDynamicResources;

    /// Always lock immediate context on every API call. May be
    /// useful for debugging purposes or when applications have
    /// race conditions.
    bool enableContextLock;

    /// Whether to expose the driver command list feature. Enabled by
    /// default and generally beneficial, but some games may assume that
    /// this is not supported when running on an AMD GPU.
    bool exposeDriverCommandLists;

    /// Shader dump path
    std::string shaderDumpPath;
  };
  
}