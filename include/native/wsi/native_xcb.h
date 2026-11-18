#pragma once

#include <stdint.h>

typedef struct xcb_connection_t xcb_connection_t;

typedef struct dxvk_xcb_window {
  xcb_connection_t* connection;
  uint32_t          window;
  uint32_t          width;
  uint32_t          height;
  uint32_t          minimized;
} dxvk_xcb_window;
