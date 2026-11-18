#pragma once

#include <stdint.h>

struct wl_display;
struct wl_surface;

typedef struct dxvk_wayland_window {
  struct wl_display* display;
  struct wl_surface* surface;
  uint32_t           width;
  uint32_t           height;
  uint32_t           minimized;
} dxvk_wayland_window;
