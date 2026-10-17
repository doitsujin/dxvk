#pragma once

#include <stdint.h>

enum dxvk_external_window_type {
  DXVK_EXTERNAL_WINDOW_XLIB    = 1,
  DXVK_EXTERNAL_WINDOW_XCB     = 2,
  DXVK_EXTERNAL_WINDOW_WAYLAND = 3,
};

typedef struct dxvk_external_window {
  uint32_t  type;       // dxvk_external_window_type
  void*     display;
  uintptr_t window;
  uint32_t  width;
  uint32_t  height;
  uint32_t  minimized;
} dxvk_external_window;
