#pragma once

#include <unordered_set>

#include "dxvk_lifetime.h"
#include "dxvk_recorder.h"

namespace dxvk {
  
  /**
   * \brief DXVK command list
   * 
   * Stores a command buffer that a context can use to record Vulkan
   * commands. The command list shall also reference the resources
   * used by the recorded commands for automatic lifetime tracking.
   * When the command list has completed execution, resources that
   * are no longer used may get destroyed.
   */
  class DxvkDeferredCommands : public DxvkRecorder {
    
  public:
    
    DxvkDeferredCommands();
    ~DxvkDeferredCommands();
    
  };
  
}