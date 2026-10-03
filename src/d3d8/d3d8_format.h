#pragma once

#include "d3d8_include.h"

namespace dxvk {

  inline bool IsDXTFormat(D3DFORMAT fmt) {
    switch (fmt) {
      case D3DFMT_DXT1:
      case D3DFMT_DXT2:
      case D3DFMT_DXT3:
      case D3DFMT_DXT4:
      case D3DFMT_DXT5:
        return true;
      default:
        return false;
    }
  }

  inline bool IsDepthStencilFormat(D3DFORMAT fmt) {
    switch (fmt) {
      case D3DFMT_D16_LOCKABLE:
      case D3DFMT_D16:
      case D3DFMT_D32:
      case D3DFMT_D15S1:
      case D3DFMT_D24X4S4:
      case D3DFMT_D24S8:
      case D3DFMT_D24X8:
        return true;
      default:
        return false;
    }
  }

  // The D3D8 documentation states: Render target formats are restricted to
  // D3DFMT_X1R5G5B5, D3DFMT_R5G6B5, D3DFMT_X8R8G8B8, and D3DFMT_A8R8G8B8.
  // This limited RT format support is confirmed by age-accurate drivers.
  inline bool IsRenderTargetFormat(D3DFORMAT fmt) {
    // NULL format support was later added to D3D9 and is also advertised
    // in D3D8, though it's unlikely to ever have been used in practice.
    if (unlikely(fmt == static_cast<D3DFORMAT>(MAKEFOURCC('N', 'U', 'L', 'L'))))
      return true;

    switch (fmt) {
      case D3DFMT_X1R5G5B5:
      case D3DFMT_R5G6B5:
      case D3DFMT_X8R8G8B8:
      case D3DFMT_A8R8G8B8:
        return true;
      default:
        return false;
    }
  }

  // Some games will exhaustively query all formats in the 0-100 range,
  // so filter out some known formats which are exclusive to D3D9.
  inline bool IsD3D9ExclusiveFormat(D3DFORMAT fmt) {
    d3d9::D3DFORMAT fmt9 = static_cast<d3d9::D3DFORMAT>(fmt);

    // Get some very unlikely FOURCCs out of the way first
    if (unlikely(fmt9 == static_cast<d3d9::D3DFORMAT>(MAKEFOURCC('D', 'F', '1', '6'))
              || fmt9 == static_cast<d3d9::D3DFORMAT>(MAKEFOURCC('D', 'F', '2', '4'))
              || fmt9 == static_cast<d3d9::D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'))))
      return true;

    switch (fmt9) {
      case d3d9::D3DFMT_A8B8G8R8:            //32
      case d3d9::D3DFMT_X8B8G8R8:            //33
      case d3d9::D3DFMT_A2R10G10B10:         //35
      case d3d9::D3DFMT_A16B16G16R16:        //36
      case d3d9::D3DFMT_L16:                 //81
      case d3d9::D3DFMT_D32F_LOCKABLE:       //82
      case d3d9::D3DFMT_D24FS8:              //83
      case d3d9::D3DFMT_D32_LOCKABLE:        //84
      case d3d9::D3DFMT_S8_LOCKABLE:         //85
      case d3d9::D3DFMT_Q16W16V16U16:        //110
      case d3d9::D3DFMT_R16F:                //111
      case d3d9::D3DFMT_G16R16F:             //112
      case d3d9::D3DFMT_A16B16G16R16F:       //113
      case d3d9::D3DFMT_R32F:                //114
      case d3d9::D3DFMT_G32R32F:             //115
      case d3d9::D3DFMT_A32B32G32R32F:       //116
      case d3d9::D3DFMT_CxV8U8:              //117
      case d3d9::D3DFMT_A1:                  //118
      case d3d9::D3DFMT_A2B10G10R10_XR_BIAS: //119
        return true;
      default:
        return false;
    }
  }

  // Get bytes per pixel (or 4x4 block for DXT)
  inline UINT GetFormatStride(D3DFORMAT fmt) {
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

  inline UINT GetSurfaceSize(D3DFORMAT Format, UINT Width, UINT Height) {
    if (IsDXTFormat(Format)) {
      Width  = ((Width  + 3) >> 2);
      Height = ((Height + 3) >> 2);
    }
    return Width * Height * GetFormatStride(Format);
  }

}