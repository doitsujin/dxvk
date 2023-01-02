#pragma once

#include "../../vulkan/vulkan_loader.h"
#include <GLFW/glfw3.h>

#include "../wsi_monitor.h"

namespace dxvk::wsi {

  /**
    * \brief Impl-dependent state
    */
  struct DxvkWindowState {
  };
  
  inline bool isDisplayValid(int32_t displayId) {
    int32_t displayCount = 0;
    glfwGetMonitors(&displayCount);

    return displayId < displayCount && displayId >= 0;
  }

}