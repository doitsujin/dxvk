#pragma once

#include <windows.h>

namespace dxvk::wsi {

  /**
    * \brief Impl-dependent state
    */
  struct DxvkWindowState {
    LONG style   = 0;
    LONG exstyle = 0;
    RECT rect    = { 0, 0, 0, 0 };
  };

}