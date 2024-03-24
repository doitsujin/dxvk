#pragma once

#include "../wsi_monitor.h"

namespace dxvk::wsi {

  /**
    * \brief Impl-dependent state
    */
  struct DxvkWindowState {
  };

  inline bool isDisplayValid(int32_t displayId) {
    return false;
  }

}
