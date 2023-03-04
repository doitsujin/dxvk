#pragma once

#include "d3d8_include.h"

namespace dxvk {
  inline constexpr bool isDXT(D3DFORMAT fmt) {
    return fmt == D3DFMT_DXT1
        || fmt == D3DFMT_DXT2
        || fmt == D3DFMT_DXT3
        || fmt == D3DFMT_DXT4
        || fmt == D3DFMT_DXT5;
  }

  inline constexpr bool isDXT(d3d9::D3DFORMAT fmt) {
    return isDXT(D3DFORMAT(fmt));
  }

  // Get bytes per pixel (or 4x4 block for DXT)
  inline constexpr UINT getFormatStride(D3DFORMAT fmt) {
    switch (fmt) {
      default:
      case D3DFMT_UNKNOWN:
        return 0;
      case D3DFMT_R3G3B2:
      case D3DFMT_A8:
      case D3DFMT_P8:
      case D3DFMT_L8:
      case D3DFMT_A4L4:
        return 1;
      case D3DFMT_R5G6B5:
      case D3DFMT_X1R5G5B5:
      case D3DFMT_A1R5G5B5:
      case D3DFMT_A4R4G4B4:
      case D3DFMT_A8R3G3B2:
      case D3DFMT_X4R4G4B4:
      case D3DFMT_A8P8:
      case D3DFMT_A8L8:
      case D3DFMT_V8U8:
      case D3DFMT_L6V5U5:
      case D3DFMT_D16_LOCKABLE:
      case D3DFMT_D15S1:
      case D3DFMT_D16:
      case D3DFMT_UYVY:
      case D3DFMT_YUY2:
        return 2;
      case D3DFMT_R8G8B8:
        return 3;
      case D3DFMT_A8R8G8B8:
      case D3DFMT_X8R8G8B8:
      case D3DFMT_A2B10G10R10:
      //case D3DFMT_A8B8G8R8:
      //case D3DFMT_X8B8G8R8:
      case D3DFMT_G16R16:
      case D3DFMT_X8L8V8U8:
      case D3DFMT_Q8W8V8U8:
      case D3DFMT_V16U16:
      case D3DFMT_W11V11U10:
      case D3DFMT_A2W10V10U10:
      case D3DFMT_D32:
      case D3DFMT_D24S8:
      case D3DFMT_D24X8:
      case D3DFMT_D24X4S4:
        return 4;
      case D3DFMT_DXT1:
        return 8;
      case D3DFMT_DXT2:
      case D3DFMT_DXT3:
      case D3DFMT_DXT4:
      case D3DFMT_DXT5:
        return 16;
    }
  }

  inline constexpr UINT getSurfaceSize(D3DFORMAT Format, UINT Width, UINT Height) {
    if (isDXT(Format)) {
      Width = ((Width + 3) >> 2);
      Height = ((Height + 3) >> 2);
    }
    return Width * Height * getFormatStride(Format);
  }

}