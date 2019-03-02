#pragma once

#include "d3d9_caps.h"

#include <array>

namespace dxvk {

  class Direct3DSurface9;

  struct Direct3DState9 {
    Direct3DState9() {
      for (uint32_t i = 0; i < caps::MaxSimultaneousRenderTargets; i++)
        renderTargets[i] = nullptr;

      depthStencil = nullptr;
    }

    std::array<Direct3DSurface9*, caps::MaxSimultaneousRenderTargets> renderTargets;
    Direct3DSurface9* depthStencil;
  };

}