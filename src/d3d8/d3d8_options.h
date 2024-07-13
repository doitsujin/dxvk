#pragma once

#include "d3d8_include.h"
#include "../d3d9/d3d9_bridge.h"
#include "../util/config/config.h"

namespace dxvk {
  struct D3D8Options {

    /// Some games rely on undefined behavior by using undeclared vertex shader inputs.
    /// The simplest way to fix them is to simply modify their vertex shader decl.
    /// 
    /// This option takes a comma-separated list of colon-separated number pairs, where
    /// the first number is a D3DVSDE_REGISTER value, the second is a D3DVSDT_TYPE value.
    ///   e.g. "0:2,3:2,7:1" for float3 position : v0, float3 normal : v3, float2 uv : v7
    std::vector<std::pair<D3DVSDE_REGISTER, D3DVSDT_TYPE>> forceVsDecl;

    /// Specialized drawcall batcher, typically for games that draw a lot of similar
    /// geometry in separate drawcalls (sometimes even one triangle at a time).
    ///
    /// May hurt performance outside of specifc games that benefit from it.
    bool batching = false;

    /// The Lord of the Rings: The Fellowship of the Ring tries to create a P8 texture
    /// in D3DPOOL_MANAGED on Nvidia and Intel, which fails, but has a separate code
    /// path for ATI/AMD that creates it in D3DPOOL_SCRATCH instead, which works.
    ///
    /// The internal logic determining this path doesn't seem to be d3d-related, but
    /// the game works universally if we mimic its own ATI/AMD workaround during P8
    /// texture creation.
    ///
    /// Early Nvidia GPUs, such as the GeForce 4 generation cards, included and exposed
    /// P8 texture support. However, it was no longer advertised with cards in the FX series
    /// and above. Most likely ATI/AMD drivers never supported P8 in the first place.
    bool placeP8InScratch = false;

    /// Rayman 3 relies on D3DLOCK_DISCARD being ignored for everything except D3DUSAGE_DYNAMIC +
    /// D3DUSAGE_WRITEONLY buffers, however this approach incurs a performance penalty.
    ///
    /// Some titles might abuse this early D3D8 quirk, however at some point in its history
    /// it was brought in line with standard D3D9 behavior.
    bool forceLegacyDiscard = false;

    D3D8Options() {}
    D3D8Options(const Config& config) {
      auto forceVsDeclStr     = config.getOption<std::string>("d3d8.forceVsDecl",            "");
      batching                = config.getOption<bool>       ("d3d8.batching",               batching);
      placeP8InScratch        = config.getOption<bool>       ("d3d8.placeP8InScratch",       placeP8InScratch);
      forceLegacyDiscard      = config.getOption<bool>       ("d3d8.forceLegacyDiscard",     forceLegacyDiscard);

      parseVsDecl(forceVsDeclStr);
    }

    void parseVsDecl(const std::string& decl);
  };
}
