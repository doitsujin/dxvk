#pragma once

#include <d3d9types.h>

namespace dxvk {

  inline void DecodeD3DCOLOR(D3DCOLOR color, float* rgba) {
    // Encoded in D3DCOLOR as argb
    rgba[3] = (float)((color & 0xff000000) >> 24) / 255.0f;
    rgba[0] = (float)((color & 0x00ff0000) >> 16) / 255.0f;
    rgba[1] = (float)((color & 0x0000ff00) >> 8)  / 255.0f;
    rgba[2] = (float)((color & 0x000000ff))       / 255.0f;
  }

}