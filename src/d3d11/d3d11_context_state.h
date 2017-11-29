#pragma once

#include <array>

#include "d3d11_interfaces.h"

namespace dxvk {
  
  /**
   * \brief Context state
   */
  struct D3D11ContextState {
    std::array<
      Com<ID3D11RenderTargetViewPrivate>,
      D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> omRenderTargetViews;
  };
  
}