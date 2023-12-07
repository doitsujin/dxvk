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

  #define GLFW_PROC(ret, name, params) \
    typedef ret (*pfn_##name) params;
  #include "wsi_platform_glfw_funcs.h"

  struct WsiLibrary {
    WsiLibrary(HMODULE dll);
    static WsiLibrary *get();
    HMODULE libglfw;

    #define GLFW_PROC(ret, name, params) \
      pfn_##name name;
    #include "wsi_platform_glfw_funcs.h"
  };

  void init();
  void quit();
  
  inline bool isDisplayValid(int32_t displayId) {
    int32_t displayCount = 0;
    WsiLibrary::get()->glfwGetMonitors(&displayCount);

    return displayId < displayCount && displayId >= 0;
  }

}
