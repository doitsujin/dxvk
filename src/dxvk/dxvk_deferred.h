#pragma once

#include <unordered_set>

#include "dxvk_lifetime.h"
#include "dxvk_recorder.h"

namespace dxvk {
  
  /**
   * \brief DXVK deferred command list
   * 
   * Buffers Vulkan commands so that they can be recorded
   * into an actual Vulkan command buffer later. This is
   * used to implement D3D11 Deferred Contexts, which do
   * not map particularly well to Vulkan's command buffers.
   */
  class DxvkDeferredCommands : public DxvkRecorder {
    
  public:
    
    DxvkDeferredCommands();
    ~DxvkDeferredCommands();
    
  };
  
}