#pragma once

#include "dxvk_framebuffer.h"
#include "dxvk_shader.h"

namespace dxvk {
  
  enum class DxvkFbStateFlags : uint32_t {
    InsideRenderPass = 0,
  };
  
  struct DxvkFramebufferState {
    Rc<DxvkFramebuffer>     framebuffer;
    Flags<DxvkFbStateFlags> flags;
  };
  
  struct DxvkContextState {
    DxvkFramebufferState fb; ///< Framebuffer and render pass
  };
  
}