#pragma once

#include <array>

#include "d3d11_state.h"
#include "d3d11_view.h"

namespace dxvk {
  
  struct D3D11ContextStateOM {
    std::array<Com<D3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> renderTargetViews;
    
    Rc<DxvkFramebuffer> framebuffer;
  };
  
  
  struct D3D11ContextStateRS {
    uint32_t numViewports = 0;
    uint32_t numScissors  = 0;
    
    std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports;
    std::array<D3D11_RECT,     D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors;
    
    Com<D3D11RasterizerState> state;
  };
  
  
  /**
   * \brief Context state
   */
  struct D3D11ContextState {
    D3D11ContextStateOM om;
    D3D11ContextStateRS rs;
  };
  
}