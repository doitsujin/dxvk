#pragma once

#include "d3d8_include.h"

namespace dxvk {

  inline bool isDXTFormat(D3DFORMAT fmt) {
    return fmt == D3DFMT_DXT1
        || fmt == D3DFMT_DXT2
        || fmt == D3DFMT_DXT3
        || fmt == D3DFMT_DXT4
        || fmt == D3DFMT_DXT5;
  }

  inline bool isDepthStencilFormat(D3DFORMAT fmt) {
    return fmt == D3DFMT_D16_LOCKABLE
        || fmt == D3DFMT_D16
        || fmt == D3DFMT_D32
        || fmt == D3DFMT_D15S1
        || fmt == D3DFMT_D24X4S4
        || fmt == D3DFMT_D24S8
        || fmt == D3DFMT_D24X8;
  }

  // The d3d8 documentation states: Render target formats are restricted to
  // D3DFMT_X1R5G5B5, D3DFMT_R5G6B5, D3DFMT_X8R8G8B8, and D3DFMT_A8R8G8B8.
  // This limited RT format support is confirmed by age-accurate drivers.
  inline bool isRenderTargetFormat(D3DFORMAT fmt) {
    return fmt == D3DFMT_X1R5G5B5
        || fmt == D3DFMT_R5G6B5
        || fmt == D3DFMT_X8R8G8B8
        || fmt == D3DFMT_A8R8G8B8
        // NULL format support was later added to d3d9 with
        // GeForce 6 series cards, and also advertised in d3d8.
        || fmt == (D3DFORMAT) MAKEFOURCC('N', 'U', 'L', 'L');
  }

  // Some games will exhaustively query all formats in the 0-100 range,
  // so filter out some known formats which are exclusive to d3d9
  inline bool isD3D9ExclusiveFormat(D3DFORMAT fmt) {
    const d3d9::D3DFORMAT d3d9Fmt = d3d9::D3DFORMAT(fmt);

    return d3d9Fmt == d3d9::D3DFMT_A8B8G8R8            //32
        || d3d9Fmt == d3d9::D3DFMT_X8B8G8R8            //33
        || d3d9Fmt == d3d9::D3DFMT_A2R10G10B10         //35
        || d3d9Fmt == d3d9::D3DFMT_A16B16G16R16        //36
        || d3d9Fmt == d3d9::D3DFMT_L16                 //81
        || d3d9Fmt == d3d9::D3DFMT_D32F_LOCKABLE       //82
        || d3d9Fmt == d3d9::D3DFMT_D24FS8              //83
        || d3d9Fmt == d3d9::D3DFMT_D32_LOCKABLE        //84
        || d3d9Fmt == d3d9::D3DFMT_S8_LOCKABLE         //85
        || d3d9Fmt == d3d9::D3DFMT_Q16W16V16U16        //110
        || d3d9Fmt == d3d9::D3DFMT_R16F                //111
        || d3d9Fmt == d3d9::D3DFMT_G16R16F             //112
        || d3d9Fmt == d3d9::D3DFMT_A16B16G16R16F       //113
        || d3d9Fmt == d3d9::D3DFMT_R32F                //114
        || d3d9Fmt == d3d9::D3DFMT_G32R32F             //115
        || d3d9Fmt == d3d9::D3DFMT_A32B32G32R32F       //116
        || d3d9Fmt == d3d9::D3DFMT_CxV8U8              //117
        || d3d9Fmt == d3d9::D3DFMT_A1                  //118
        || d3d9Fmt == d3d9::D3DFMT_A2B10G10R10_XR_BIAS //119
        || d3d9Fmt == (d3d9::D3DFORMAT) MAKEFOURCC('D', 'F', '1', '6')
        || d3d9Fmt == (d3d9::D3DFORMAT) MAKEFOURCC('D', 'F', '2', '4')
        || d3d9Fmt == (d3d9::D3DFORMAT) MAKEFOURCC('I', 'N', 'T', 'Z');
  }

  // Get bytes per pixel (or 4x4 block for DXT)
  inline UINT getFormatStride(D3DFORMAT fmt) {
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

  inline UINT getSurfaceSize(D3DFORMAT Format, UINT Width, UINT Height) {
    if (isDXTFormat(Format)) {
      Width = ((Width + 3) >> 2);
      Height = ((Height + 3) >> 2);
    }
    return Width * Height * getFormatStride(Format);
  }

}
