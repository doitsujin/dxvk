#pragma once

#include "ddraw_include.h"

#include "../util/config/config.h"
#include "../util/util_math.h"

namespace dxvk {

  enum class AlternatePixelCenter {
    Disabled,
    Enabled,
    Legacy
  };

  enum class D3DLegacyPresentGuard {
    Auto,
    Disabled,
    Strict
  };

  enum class FSAAEmulation {
    Disabled,
    Enabled,
    Forced
  };

  struct D3DOptions {

    D3DOptions() {};

    D3DOptions(const Config& config);

    /// Enforces the use of DDSCL_MULTITHREADED
    bool forceMultiThreaded;

    /// Use SWVP mode for all D3D9 devices
    bool forceSWVP;

    /// Use MANAGED vertex buffers instead of DEFAULT vertex buffers
    bool managedVertexBuffers;

    /// Advertise support for R3G3B2
    bool supportR3G3B2;

    /// Advertise support for D16
    bool supportD16;

    /// Advertise support for any 32-bit bitmask depth foramts
    bool support32BitDepth;

    /// Replaces any use of D32 with D24X8
    bool useD24X8forD32;

    /// Replaces any use of D24X8 with D16
    bool useD16forD24X8;

    /// Report any 8-bit display modes as being 16-bit
    bool mask8BitModes;

    /// Always use 0.0f and 1.0f as viewport Z values
    bool viewportZCorrection;

    /// Respect DISCARD only on DYNAMIC + WRITEONLY buffers
    bool forceLegacyDiscard;

    /// Process vertices on the CPU, instead of relaying to D3D9
    bool cpuProcessVertices;

    /// Resize the back buffer size to screen size when needed
    bool backBufferResize;

    /// Blits back to the proxied flippable surface and back again for presentation
    bool forceLegacyPresent;

    /// Explicitly flip the RT swapchain, even if the primary surface is not part of it
    bool forceRTFlip;

    /// Forwards all DC operations to D3D9 surfaces
    bool forceDCForwarding;

    /// Emulate an explicit D3D9 front buffer by uploading its content from DDraw
    bool emulateFrontBuffer;

    /// Ignore any application set gamma ramp
    bool ignoreGammaRamp;

    /// Automatically generate all texture mip maps on the GPU
    bool autoGenMipMaps;

    /// Allow cross-device resource (surfaces/textures) use
    bool deviceResourceSharing;

    /// Masks the color key values based on surface format color depth
    bool colorKeyMasking;

    /// Enumerate with legacy/official implementation device names
    bool legacyDeviceNames;

    /// Expose the D3DDEVCAPS_TEXTURENONLOCALVIDMEM device cap
    bool nonLocalVideoMemory;

    /// Be adamant about keeping all texture backing surfaces alive
    bool robustTextureLifeCycle;

    /// Extends features and relaxes validations to enable apitrace debugging
    bool apitraceMode;

    /// Half-texel correction offset for X/Y vertex position
    AlternatePixelCenter alternatePixelCenter;

    /// By default guards against legacy presents while inside of a scene
    D3DLegacyPresentGuard legacyPresentGuard;

    /// Uses supported MSAA up to 4x to emulate FSAA
    FSAAEmulation emulateFSAA;

  };

}
