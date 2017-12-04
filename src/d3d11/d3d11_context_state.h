#pragma once

#include <array>

#include "d3d11_interfaces.h"
#include "d3d11_view.h"

namespace dxvk {
  
  struct D3D11ContextStateOM {
    Rc<DxvkFramebuffer> framebuffer;
  };
  
  /**
   * \brief Context state
   */
  struct D3D11ContextState {
    std::array<
      Com<D3D11RenderTargetView>,
      D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> omRenderTargetViews;
    D3D11ContextStateOM om;
  };
  
}