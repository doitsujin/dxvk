#pragma once

#include <cstdint>

namespace dxvk::hud {
  
  struct HudGlyph {
    uint32_t codePoint;
    int32_t  x;
    int32_t  y;
    int32_t  w;
    int32_t  h;
    int32_t  originX;
    int32_t  originY;
  };

  struct HudFont {
    int32_t  size;
    uint32_t width;
    uint32_t height;
    uint32_t falloff;
    uint32_t advance;
    uint32_t charCount;
    
    const HudGlyph* glyphs;
    const uint8_t* texture;
  };
  
  extern const HudFont g_hudFont;
  
}