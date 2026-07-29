#pragma once

#include "ddraw_include.h"

#include <vector>
#include <cmath>
#include <utility>
#include <type_traits>

namespace dxvk {

  template <size_t N>
  static void CopyToStringArray(char (&dst)[N], const char* src) {
    str::strlcpy(dst, src, N);
  }

  struct CubeMapAttachedSurfaces {
    IDirectDrawSurface7* positiveX = nullptr;
    IDirectDrawSurface7* negativeX = nullptr;
    IDirectDrawSurface7* positiveY = nullptr;
    IDirectDrawSurface7* negativeY = nullptr;
    IDirectDrawSurface7* positiveZ = nullptr;
    IDirectDrawSurface7* negativeZ = nullptr;
  };

  struct AttachedSurface {
    IDirectDrawSurface*  surface      = nullptr;
    DDSURFACEDESC        desc         = { };
  };

  struct AttachedSurface4 {
    IDirectDrawSurface4* surface4     = nullptr;
    DDSURFACEDESC2       desc2        = { };
  };

  struct AttachedSurface7 {
    IDirectDrawSurface7* surface7     = nullptr;
    DDSURFACEDESC2       desc2        = { };
  };

  inline d3d9::D3DCUBEMAP_FACES GetCubemapFace(DDSURFACEDESC2* desc) {
    const DWORD cubeFaceMask = desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_ALLFACES;

    switch (cubeFaceMask) {
      default:
      case DDSCAPS2_CUBEMAP_POSITIVEX: return d3d9::D3DCUBEMAP_FACE_POSITIVE_X;
      case DDSCAPS2_CUBEMAP_NEGATIVEX: return d3d9::D3DCUBEMAP_FACE_NEGATIVE_X;
      case DDSCAPS2_CUBEMAP_POSITIVEY: return d3d9::D3DCUBEMAP_FACE_POSITIVE_Y;
      case DDSCAPS2_CUBEMAP_NEGATIVEY: return d3d9::D3DCUBEMAP_FACE_NEGATIVE_Y;
      case DDSCAPS2_CUBEMAP_POSITIVEZ: return d3d9::D3DCUBEMAP_FACE_POSITIVE_Z;
      case DDSCAPS2_CUBEMAP_NEGATIVEZ: return d3d9::D3DCUBEMAP_FACE_NEGATIVE_Z;
    }
  }

  inline d3d9::D3DFORMAT ConvertFormat(DDPIXELFORMAT& fmt) {
    d3d9::D3DFORMAT d3d9Format;

    // Empire of the Ants erroneously marks all DDPF_RGB surfaces
    // with the DDPF_ZBUFFER flag as well, so prioritize DDPF_RGB
    if (fmt.dwFlags & DDPF_RGB) {
      //Logger::debug(str::format("ConvertFormat: fmt.dwRGBBitCount:     ", fmt.dwRGBBitCount,
      //                        "\nConvertFormat: fmt.dwRGBAlphaBitMask: ", fmt.dwRGBAlphaBitMask,
      //                        "\nConvertFormat: fmt.dwRBitMask:        ", fmt.dwRBitMask,
      //                        "\nConvertFormat: fmt.dwGBitMask:        ", fmt.dwGBitMask,
      //                        "\nConvertFormat: fmt.dwBBitMask:        ", fmt.dwBBitMask));

      switch (fmt.dwRGBBitCount) {
        case 8:
          // R: 1110 0000
          d3d9Format = (fmt.dwFlags & DDPF_PALETTEINDEXED8) ? d3d9::D3DFMT_P8 : d3d9::D3DFMT_R3G3B2;
          break;

        case 16:
          switch (fmt.dwRBitMask) {
            case (0xF << 8):
              // A: 1111 0000 0000 0000
              // R: 0000 1111 0000 0000
              d3d9Format = d3d9::D3DFMT_A4R4G4B4;
              break;

            case (0x1F << 10):
              // A: 1000 0000 0000 0000
              // R: 0111 1100 0000 0000
              d3d9Format = fmt.dwRGBAlphaBitMask ? d3d9::D3DFMT_A1R5G5B5 : d3d9::D3DFMT_X1R5G5B5;
              break;

            case (0x1F << 11):
              // R: 1111 1000 0000 0000
              d3d9Format = d3d9::D3DFMT_R5G6B5;
              break;

            default:
              Logger::warn("ConvertFormat: Unhandled dwRGBBitCount 16 format");
              d3d9Format = d3d9::D3DFMT_UNKNOWN;
              break;
          }
          break;

        // Drakan: Order of the Flame uses a dwRGBBitCount of 24
        // to request for D3DFMT_X8R8G8B8, based on provided bitmasks
        case 24:
          d3d9Format = d3d9::D3DFMT_X8R8G8B8;
          break;

        case 32:
          // A: 1111 1111 0000 0000 0000 0000 0000 0000
          // R: 0000 0000 1111 1111 0000 0000 0000 0000
          d3d9Format = fmt.dwRGBAlphaBitMask ? d3d9::D3DFMT_A8R8G8B8 : d3d9::D3DFMT_X8R8G8B8;
          break;

        default:
          Logger::warn("ConvertFormat: Unhandled dwRGBBitCount format");
          d3d9Format = d3d9::D3DFMT_UNKNOWN;
          break;
      }
    // Depth formats will traditionally store stencil info in the MSB, however
    // some games will apparently try to use the LSB, so handle both cases
    } else if (fmt.dwFlags & DDPF_ZBUFFER) {
      //Logger::debug(str::format("ConvertFormat: fmt.dwZBufferBitDepth: ", fmt.dwZBufferBitDepth,
      //                        "\nConvertFormat: fmt.dwZBitMask:        ",        fmt.dwZBitMask,
      //                        "\nConvertFormat: fmt.dwStencilBitMask:  ",  fmt.dwStencilBitMask));

      switch (fmt.dwZBufferBitDepth) {
        case 16:
          switch (fmt.dwStencilBitMask) {
            case 0:
              // D: 1111 1111 1111 1111
              // S: 0000 0000 0000 0000
              d3d9Format = d3d9::D3DFMT_D16;
              break;

            case 0x1:
            case (0x1 << 15):
              // D: 0111 1111 1111 1111
              // S: 1000 0000 0000 0000
              d3d9Format = d3d9::D3DFMT_D15S1;
              break;

            default:
              Logger::warn("ConvertFormat: Unhandled dwStencilBitMask 16 format");
              d3d9Format = d3d9::D3DFMT_UNKNOWN;
              break;
          }
          break;

        // We don't support or expose a 24-bit depth stencil per se,
        // but some applications request one anyway. Use 32-bit depth.
        case 24:
        case 32:
          switch (fmt.dwStencilBitMask) {
            case 0:
              switch (fmt.dwZBitMask) {
                case 0xFFFFFFFF:
                  // D: 1111 1111 1111 1111 1111 1111 1111 1111
                  // S: 0000 0000 0000 0000 0000 0000 0000 0000
                  d3d9Format = d3d9::D3DFMT_D32;
                  break;

                case 0xFFFFFF:
                case DWORD(0xFFFFFF << 8):
                  // D: 0000 0000 1111 1111 1111 1111 1111 1111
                  // S: 0000 0000 0000 0000 0000 0000 0000 0000
                  d3d9Format = d3d9::D3DFMT_D24X8;
                  break;

                default:
                  Logger::warn("ConvertFormat: Unhandled dwZBitMask 24/32 format");
                  d3d9Format = d3d9::D3DFMT_UNKNOWN;
                  break;
              }
              break;

            case 0xFF:
            case DWORD(0xFF << 24):
              // D: 0000 0000 1111 1111 1111 1111 1111 1111
              // S: 1111 1111 0000 0000 0000 0000 0000 0000
              d3d9Format = d3d9::D3DFMT_D24S8;
              break;

            case 0xF:
            case DWORD(0xF << 24):
              // D: 0000 0000 1111 1111 1111 1111 1111 1111
              // S: 1111 0000 0000 0000 0000 0000 0000 0000
              Logger::warn("ConvertFormat: Unsupported format D3DFMT_D24X4S4");
              d3d9Format = d3d9::D3DFMT_D24S8;
              break;

            default:
              Logger::warn("ConvertFormat: Unhandled dwStencilBitMask 24/32 format");
              d3d9Format = d3d9::D3DFMT_UNKNOWN;
              break;
          }
          break;

        default:
          Logger::warn("ConvertFormat: Unhandled dwZBufferBitDepth format");
          d3d9Format = d3d9::D3DFMT_UNKNOWN;
          break;
      }
    } else if (fmt.dwFlags & DDPF_BUMPDUDV) {
      //Logger::debug(str::format("ConvertFormat: fmt.dwBumpBitCount:         ", fmt.dwBumpBitCount,
      //                        "\nConvertFormat: fmt.dwBumpLuminanceBitMask: ", fmt.dwBumpLuminanceBitMask,
      //                        "\nConvertFormat: fmt.dwBumpDvBitMask:        ", fmt.dwBumpDvBitMask,
      //                        "\nConvertFormat: fmt.dwBumpDuBitMask:        ", fmt.dwBumpDuBitMask));

      switch (fmt.dwBumpBitCount) {
        case 16:
          // L: 1111 1100 0000 0000
          // V: 0000 0011 1110 0000
          // U: 0000 0000 0001 1111
          //
          // L: 0000 0000 0000 0000
          // V: 1111 1111 0000 0000
          // U: 0000 0000 1111 1111
          d3d9Format = fmt.dwBumpLuminanceBitMask ? d3d9::D3DFMT_L6V5U5 : d3d9::D3DFMT_V8U8;
          break;

        case 32:
          // A: 0000 0000 0000 0000 0000 0000 0000 0000
          // L: 0000 0000 1111 1111 0000 0000 0000 0000
          // V: 0000 0000 0000 0000 1111 1111 0000 0000
          // U: 0000 0000 0000 0000 0000 0000 1111 1111
          d3d9Format = d3d9::D3DFMT_X8L8V8U8;
          break;

        default:
          Logger::warn("ConvertFormat: Unhandled dwBumpBitCount format");
          d3d9Format = d3d9::D3DFMT_UNKNOWN;
          break;
      }
    } else if (fmt.dwFlags & DDPF_FOURCC) {
      switch (fmt.dwFourCC) {
        case MAKEFOURCC('D', 'X', 'T', '1'):
          d3d9Format = d3d9::D3DFMT_DXT1;
          break;

        case MAKEFOURCC('D', 'X', 'T', '2'):
          d3d9Format = d3d9::D3DFMT_DXT2;
          break;

        case MAKEFOURCC('D', 'X', 'T', '3'):
          d3d9Format = d3d9::D3DFMT_DXT3;
          break;

        case MAKEFOURCC('D', 'X', 'T', '4'):
          d3d9Format = d3d9::D3DFMT_DXT4;
          break;

        case MAKEFOURCC('D', 'X', 'T', '5'):
          d3d9Format = d3d9::D3DFMT_DXT5;
          break;

        case MAKEFOURCC('Y', 'U', 'Y', '2'):
          d3d9Format = d3d9::D3DFMT_YUY2;
          break;

        default:
          Logger::warn("ConvertFormat: Unhandled FOURCC payload");
          d3d9Format = d3d9::D3DFMT_UNKNOWN;
          break;
      }
    // Luminance/alpha-luminance formats appear to be unused in practice in early D3D
    } else if (unlikely(fmt.dwFlags & DDPF_LUMINANCE)) {
      //Logger::debug(str::format("ConvertFormat: fmt.dwLuminanceBitCount:     ", fmt.dwLuminanceBitCount,
      //                        "\nConvertFormat: fmt.dwLuminanceAlphaBitMask: ", fmt.dwLuminanceAlphaBitMask,
      //                        "\nConvertFormat: fmt.dwLuminanceBitMask:      ", fmt.dwLuminanceBitMask));

      switch (fmt.dwLuminanceBitCount) {
        case 8:
          switch (fmt.dwLuminanceBitMask) {
            case 0xFF:
              // L: 1111 1111
              d3d9Format = d3d9::D3DFMT_L8;
              break;

            case 0xF:
              // A: 1111 0000
              // L: 0000 1111
              d3d9Format = d3d9::D3DFMT_A4L4;
              break;

            default:
              Logger::warn("ConvertFormat: Unhandled dwLuminanceBitCount 8 format");
              d3d9Format = d3d9::D3DFMT_UNKNOWN;
              break;
          }
          break;

        case 16:
          // A: 1111 1111 0000 0000
          // L: 0000 0000 1111 1111
          d3d9Format = d3d9::D3DFMT_A8L8;
          break;

        default:
          Logger::warn("ConvertFormat: Unhandled dwLuminanceBitCount format");
          d3d9Format = d3d9::D3DFMT_UNKNOWN;
          break;
      }
    } else {
      Logger::warn("ConvertFormat: Unhandled bit payload");
      d3d9Format = d3d9::D3DFMT_UNKNOWN;
    }

    return d3d9Format;
  }

  inline DDPIXELFORMAT GetTextureFormat(d3d9::D3DFORMAT format) {
    DDPIXELFORMAT tformat = { };
    tformat.dwSize = sizeof(DDPIXELFORMAT);

    switch (format) {
      case d3d9::D3DFMT_A8R8G8B8:
        tformat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
        tformat.dwRGBBitCount = 32;
        tformat.dwRGBAlphaBitMask = 0xff000000;
        tformat.dwRBitMask = 0x00ff0000;
        tformat.dwGBitMask = 0x0000ff00;
        tformat.dwBBitMask = 0x000000ff;
        break;

      case d3d9::D3DFMT_X8R8G8B8:
        tformat.dwFlags = DDPF_RGB;
        tformat.dwRGBBitCount = 32;
        tformat.dwRGBAlphaBitMask = 0x00000000;
        tformat.dwRBitMask = 0x00ff0000;
        tformat.dwGBitMask = 0x0000ff00;
        tformat.dwBBitMask = 0x000000ff;
        break;

      case d3d9::D3DFMT_R5G6B5:
        tformat.dwFlags = DDPF_RGB;
        tformat.dwRGBBitCount = 16;
        tformat.dwRGBAlphaBitMask = 0x0000;
        tformat.dwRBitMask = 0xf800;
        tformat.dwGBitMask = 0x07e0;
        tformat.dwBBitMask = 0x001f;
        break;

      case d3d9::D3DFMT_X1R5G5B5:
        tformat.dwFlags = DDPF_RGB;
        tformat.dwRGBBitCount = 16;
        tformat.dwRGBAlphaBitMask = 0x0000;
        tformat.dwRBitMask = 0x7c00;
        tformat.dwGBitMask = 0x03e0;
        tformat.dwBBitMask = 0x001f;
        break;

      case d3d9::D3DFMT_A1R5G5B5:
        tformat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
        tformat.dwRGBBitCount = 16;
        tformat.dwRGBAlphaBitMask = 0x8000;
        tformat.dwRBitMask = 0x7c00;
        tformat.dwGBitMask = 0x03e0;
        tformat.dwBBitMask = 0x001f;
        break;

      case d3d9::D3DFMT_A4R4G4B4:
        tformat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
        tformat.dwRGBBitCount = 16;
        tformat.dwRGBAlphaBitMask = 0xf000;
        tformat.dwRBitMask = 0x0f00;
        tformat.dwGBitMask = 0x00f0;
        tformat.dwBBitMask = 0x000f;
        break;

      case d3d9::D3DFMT_R3G3B2:
        tformat.dwFlags = DDPF_RGB;
        tformat.dwRGBBitCount = 8;
        tformat.dwRGBAlphaBitMask = 0x00;
        tformat.dwRBitMask = 0xe0;
        tformat.dwGBitMask = 0x1c;
        tformat.dwBBitMask = 0x03;
        break;

      case d3d9::D3DFMT_P8:
        tformat.dwFlags = DDPF_RGB | DDPF_PALETTEINDEXED8;
        tformat.dwRGBBitCount = 8;
        break;

      case d3d9::D3DFMT_L8:
        tformat.dwFlags = DDPF_LUMINANCE;
        tformat.dwLuminanceBitCount = 8;
        tformat.dwLuminanceBitMask = 0xff;
        break;

      case d3d9::D3DFMT_A8L8:
        tformat.dwFlags = DDPF_ALPHAPIXELS | DDPF_LUMINANCE;
        tformat.dwLuminanceBitCount = 16;
        tformat.dwLuminanceAlphaBitMask = 0xff00;
        tformat.dwLuminanceBitMask = 0x00ff;
        break;

      case d3d9::D3DFMT_A4L4:
        tformat.dwFlags = DDPF_ALPHAPIXELS | DDPF_LUMINANCE;
        tformat.dwLuminanceAlphaBitMask = 0xf0;
        tformat.dwLuminanceBitCount = 8;
        tformat.dwLuminanceBitMask = 0x0f;
        break;

      case d3d9::D3DFMT_V8U8:
        tformat.dwFlags = DDPF_BUMPDUDV;
        tformat.dwBumpBitCount = 16;
        tformat.dwBumpDvBitMask = 0xff00;
        tformat.dwBumpDuBitMask = 0x00ff;
        break;

      case d3d9::D3DFMT_L6V5U5:
        tformat.dwFlags = DDPF_BUMPDUDV | DDPF_BUMPLUMINANCE;
        tformat.dwBumpBitCount = 16;
        tformat.dwBumpLuminanceBitMask = 0xfc00;
        tformat.dwBumpDvBitMask = 0x03e0;
        tformat.dwBumpDuBitMask = 0x001f;
        break;

      case d3d9::D3DFMT_X8L8V8U8:
        tformat.dwFlags = DDPF_BUMPDUDV | DDPF_BUMPLUMINANCE;
        tformat.dwBumpBitCount = 32;
        tformat.dwLuminanceAlphaBitMask = 0x00000000;
        tformat.dwBumpLuminanceBitMask = 0x00ff0000;
        tformat.dwBumpDvBitMask = 0x0000ff00;
        tformat.dwBumpDuBitMask = 0x000000ff;
        break;

      case d3d9::D3DFMT_DXT1:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '1');
        break;

      case d3d9::D3DFMT_DXT2:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '2');
        break;

      case d3d9::D3DFMT_DXT3:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '3');
        break;

      case d3d9::D3DFMT_DXT4:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '4');
        break;

      case d3d9::D3DFMT_DXT5:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('D', 'X', 'T', '5');
        break;

      case d3d9::D3DFMT_YUY2:
        tformat.dwFlags = DDPF_FOURCC;
        tformat.dwFourCC = MAKEFOURCC('Y', 'U', 'Y', '2');
        break;

      default:
      case d3d9::D3DFMT_UNKNOWN:
        Logger::err("GetTextureFormat: Unhandled format");
        break;
    }

    return tformat;
  }

  inline DDPIXELFORMAT GetZBufferFormat(d3d9::D3DFORMAT format) {
    DDPIXELFORMAT zformat = { };
    zformat.dwSize = sizeof(DDPIXELFORMAT);

    switch (format) {
      case d3d9::D3DFMT_D15S1:
        zformat.dwFlags = DDPF_ZBUFFER | DDPF_STENCILBUFFER;
        zformat.dwZBufferBitDepth = 16;
        zformat.dwZBitMask = 0x7fff;
        zformat.dwStencilBitDepth = 1;
        zformat.dwStencilBitMask = 0x8000;
        break;

      case d3d9::D3DFMT_D16:
        zformat.dwFlags = DDPF_ZBUFFER;
        zformat.dwZBufferBitDepth = 16;
        zformat.dwZBitMask = 0xffff;
        zformat.dwStencilBitDepth = 0;
        zformat.dwStencilBitMask = 0x0000;
        break;

      // Historically has had dwZBufferBitDepth = 24 too in some cases,
      // however we fix that up during format enumeration. Use 32 here.
      case d3d9::D3DFMT_D24X8:
        zformat.dwFlags = DDPF_ZBUFFER;
        zformat.dwZBufferBitDepth = 32;
        zformat.dwZBitMask = 0x00ffffff;
        zformat.dwStencilBitDepth = 0;
        zformat.dwStencilBitMask = 0x00000000;
        break;

      case d3d9::D3DFMT_D24X4S4:
        zformat.dwFlags = DDPF_ZBUFFER | DDPF_STENCILBUFFER;
        zformat.dwZBufferBitDepth = 32;
        zformat.dwZBitMask = 0x00ffffff;
        zformat.dwStencilBitDepth = 4;
        zformat.dwStencilBitMask = 0xf0000000;
        break;

      case d3d9::D3DFMT_D24S8:
        zformat.dwFlags = DDPF_ZBUFFER | DDPF_STENCILBUFFER;
        zformat.dwZBufferBitDepth = 32;
        zformat.dwZBitMask = 0x00ffffff;
        zformat.dwStencilBitDepth = 8;
        zformat.dwStencilBitMask = 0xff000000;
        break;

      case d3d9::D3DFMT_D32:
        zformat.dwFlags = DDPF_ZBUFFER;
        zformat.dwZBufferBitDepth = 32;
        zformat.dwZBitMask = 0xffffffff;
        zformat.dwStencilBitDepth = 0;
        zformat.dwStencilBitMask = 0x00000000;
        break;

      default:
      case d3d9::D3DFMT_UNKNOWN:
        Logger::err("GetZBufferFormat: Unhandled format");
        break;
    }

    return zformat;
  }

  template <typename DescType>
  inline HRESULT ValidateSurfaceFlags(DescType* desc) {
    const bool isOffScreenPlainSurface = desc->ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN;
    const bool isTexture               = desc->ddsCaps.dwCaps & DDSCAPS_TEXTURE;
    const bool isInVideoMemory         = desc->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY;
    const bool isInSystemMemory        = desc->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY;
    const bool isWriteOnly             = desc->ddsCaps.dwCaps & DDSCAPS_WRITEONLY;

    if (unlikely(isOffScreenPlainSurface && (isInVideoMemory || isInSystemMemory))) {
      return isWriteOnly ? DDERR_INVALIDCAPS : DD_OK;
    }

    if (unlikely(isTexture && (isInVideoMemory || isInSystemMemory))) {
      return isWriteOnly ? DDERR_INVALIDCAPS : DD_OK;
    }

    return DD_OK;
  }

  inline HRESULT STDMETHODCALLTYPE ListBackBufferSurfacesCallback(IDirectDrawSurface* subsurf, DDSURFACEDESC* desc, void* ctx) {
    IDirectDrawSurface** nextBackBuffer = static_cast<IDirectDrawSurface**>(ctx);

    if (desc->ddsCaps.dwCaps & DDSCAPS_FLIP) {
      *nextBackBuffer = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  inline HRESULT STDMETHODCALLTYPE ListBackBufferSurfaces4Callback(IDirectDrawSurface4* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface4** nextBackBuffer = static_cast<IDirectDrawSurface4**>(ctx);

    if (desc->ddsCaps.dwCaps & DDSCAPS_FLIP) {
      *nextBackBuffer = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  inline HRESULT STDMETHODCALLTYPE ListBackBufferSurfaces7Callback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface7** nextBackBuffer = static_cast<IDirectDrawSurface7**>(ctx);

    if (desc->ddsCaps.dwCaps & DDSCAPS_FLIP) {
      *nextBackBuffer = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  inline HRESULT STDMETHODCALLTYPE ListMipChainSurfacesCallback(IDirectDrawSurface* subsurf, DDSURFACEDESC* desc, void* ctx) {
    IDirectDrawSurface** nextMip = static_cast<IDirectDrawSurface**>(ctx);

    if (desc->ddsCaps.dwCaps & DDSCAPS_MIPMAP) {
      *nextMip = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  inline HRESULT STDMETHODCALLTYPE ListMipChainSurfaces4Callback(IDirectDrawSurface4* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface4** nextMip = static_cast<IDirectDrawSurface4**>(ctx);

    if ((desc->ddsCaps.dwCaps  & DDSCAPS_MIPMAP)
     || (desc->ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL)) {
      *nextMip = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  inline HRESULT STDMETHODCALLTYPE ListMipChainSurfaces7Callback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface7** nextMip = static_cast<IDirectDrawSurface7**>(ctx);

    if ((desc->ddsCaps.dwCaps  & DDSCAPS_MIPMAP)
     || (desc->ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL)) {
      *nextMip = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
  }

  inline HRESULT STDMETHODCALLTYPE EnumAttachedSurfacesCallback(IDirectDrawSurface* surface, DDSURFACEDESC* desc, void* ctx) {
    auto& attachedSurfaces = *static_cast<std::vector<AttachedSurface>*>(ctx);

    AttachedSurface attachedSurface = { surface, *desc };
    attachedSurfaces.push_back(attachedSurface);

    return DDENUMRET_OK;
  }

  inline HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces4Callback(IDirectDrawSurface4* surface, DDSURFACEDESC2* desc, void* ctx) {
    auto& attachedSurfaces = *static_cast<std::vector<AttachedSurface4>*>(ctx);

    AttachedSurface4 attachedSurface4 = { surface, *desc };
    attachedSurfaces.push_back(attachedSurface4);

    return DDENUMRET_OK;
  }

  inline HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces7Callback(IDirectDrawSurface7* surface, DDSURFACEDESC2* desc, void* ctx) {
    auto& attachedSurfaces = *static_cast<std::vector<AttachedSurface7>*>(ctx);

    AttachedSurface7 attachedSurface7 = { surface, *desc };
    attachedSurfaces.push_back(attachedSurface7);

    return DDENUMRET_OK;
  }

  inline HRESULT STDMETHODCALLTYPE EnumAndAttachCubeMapFacesCallback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    CubeMapAttachedSurfaces* cubeMapAttachedSurfaces = static_cast<CubeMapAttachedSurfaces*>(ctx);

    const DWORD cubeFaceMask = desc->ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_ALLFACES;

    switch (cubeFaceMask) {
      case DDSCAPS2_CUBEMAP_POSITIVEX:
        cubeMapAttachedSurfaces->positiveX = subsurf;
        break;
      case DDSCAPS2_CUBEMAP_NEGATIVEX:
        cubeMapAttachedSurfaces->negativeX = subsurf;
        break;
      case DDSCAPS2_CUBEMAP_POSITIVEY:
        cubeMapAttachedSurfaces->positiveY = subsurf;
        break;
      case DDSCAPS2_CUBEMAP_NEGATIVEY:
        cubeMapAttachedSurfaces->negativeY = subsurf;
        break;
      case DDSCAPS2_CUBEMAP_POSITIVEZ:
        cubeMapAttachedSurfaces->positiveZ = subsurf;
        break;
      case DDSCAPS2_CUBEMAP_NEGATIVEZ:
        cubeMapAttachedSurfaces->negativeZ = subsurf;
        break;
      default:
        break;
    }

    return DDENUMRET_OK;
  }

  inline void BlitToD3D9CubeMap(
        d3d9::IDirect3DCubeTexture9* cubeTex9,
        IDirectDrawSurface7* surface,
        const uint16_t mipLevels,
        const bool isDXTFormat) {
    DDSURFACEDESC2 desc;
    desc.dwSize = sizeof(DDSURFACEDESC2);
    surface->GetSurfaceDesc(&desc);
    const d3d9::D3DCUBEMAP_FACES face = GetCubemapFace(&desc);
    IDirectDrawSurface7* mipMap = surface;

    for (uint16_t i = 0; i < mipLevels; i++) {
      // Should never occur normally, but acts as a last ditch safety check
      if (unlikely(mipMap == nullptr)) {
        Logger::warn(str::format("BlitToD3D9CubeMap: Last found source mip ", i - 1));
        break;
      }

      d3d9::D3DLOCKED_RECT rect9mip;
      // D3DLOCK_DISCARD will get ignored for MANAGED/SYSTEMMEM, but will work on DEFAULT
      HRESULT hr9 = cubeTex9->LockRect(face, i, &rect9mip, NULL, D3DLOCK_DISCARD);
      if (likely(SUCCEEDED(hr9))) {
        DDSURFACEDESC2 descMip;
        descMip.dwSize = sizeof(DDSURFACEDESC2);
        HRESULT hr = mipMap->Lock(NULL, &descMip, DDLOCK_READONLY, NULL);
        if (likely(SUCCEEDED(hr))) {
          // The lock pitch of a DXT surface represents its entire size, apparently
          if (isDXTFormat) {
            const size_t size = static_cast<size_t>(descMip.lPitch);
            memcpy(rect9mip.pBits, descMip.lpSurface, size);
            //Logger::debug(str::format("BlitToD3D9CubeMap: Done blitting DXT mip ", i));
          } else if (descMip.lPitch != rect9mip.Pitch) {
            //Logger::debug(str::format("BlitToD3D9CubeMap: Incompatible mip map ", i, " pitch"));

            uint8_t* data9 = reinterpret_cast<uint8_t*>(rect9mip.pBits);
            uint8_t* data7 = reinterpret_cast<uint8_t*>(descMip.lpSurface);

            const size_t copyPitch = std::min<size_t>(descMip.lPitch, rect9mip.Pitch);
            for (uint32_t h = 0; h < descMip.dwHeight; h++)
              memcpy(&data9[h * rect9mip.Pitch], &data7[h * descMip.lPitch], copyPitch);

            //Logger::debug(str::format("BlitToD3D9CubeMap: Done blitting mip ", i, " row by row"));
          } else {
            const size_t size = static_cast<size_t>(descMip.dwHeight * descMip.lPitch);
            memcpy(rect9mip.pBits, descMip.lpSurface, size);
            //Logger::debug(str::format("BlitToD3D9CubeMap: Done blitting mip ", i));
          }
          mipMap->Unlock(NULL);
        } else {
          Logger::warn(str::format("BlitToD3D9CubeMap: Failed to lock mip ", i));
        }
        cubeTex9->UnlockRect(face, i);

        IDirectDrawSurface7* parentSurface = mipMap;
        mipMap = nullptr;

        parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces7Callback);
      } else {
        Logger::warn(str::format("BlitToD3D9CubeMap: Failed to lock D3D9 mip ", i));
      }
    }
  }

  template <typename SurfaceType, typename DescType>
  inline void BlitToD3D9Texture(
        d3d9::IDirect3DTexture9* texture9,
        SurfaceType* surface,
        const uint16_t mipLevels,
        const bool isDXTFormat) {
    SurfaceType* mipMap = surface;

    for (uint16_t i = 0; i < mipLevels; i++) {
      // Should never occur normally, but acts as a last ditch safety check
      if (unlikely(mipMap == nullptr)) {
        Logger::warn(str::format("BlitToD3D9Texture: Last found source mip ", i - 1));
        break;
      }

      d3d9::D3DLOCKED_RECT rect9mip;
      // D3DLOCK_DISCARD will get ignored for MANAGED/SYSTEMMEM, but will work on DEFAULT
      HRESULT hr9 = texture9->LockRect(i, &rect9mip, NULL, D3DLOCK_DISCARD);
      if (likely(SUCCEEDED(hr9))) {
        DescType descMip;
        descMip.dwSize = sizeof(DescType);
        HRESULT hr = mipMap->Lock(NULL, &descMip, DDLOCK_READONLY, NULL);
        if (likely(SUCCEEDED(hr))) {
          // The lock pitch of a DXT surface represents its entire size, apparently
          if (isDXTFormat) {
            const size_t size = static_cast<size_t>(descMip.lPitch);
            memcpy(rect9mip.pBits, descMip.lpSurface, size);
            //Logger::debug(str::format("BlitToD3D9Texture: Done blitting DXT mip ", i));
          } else if (descMip.lPitch != rect9mip.Pitch) {
            //Logger::debug(str::format("BlitToD3D9Texture: Incompatible mip map ", i, " pitch"));

            uint8_t* data9 = reinterpret_cast<uint8_t*>(rect9mip.pBits);
            uint8_t* data7 = reinterpret_cast<uint8_t*>(descMip.lpSurface);

            const size_t copyPitch = std::min<size_t>(descMip.lPitch, rect9mip.Pitch);
            for (uint32_t h = 0; h < descMip.dwHeight; h++)
              memcpy(&data9[h * rect9mip.Pitch], &data7[h * descMip.lPitch], copyPitch);

            //Logger::debug(str::format("BlitToD3D9Texture: Done blitting mip ", i, " row by row"));
          } else {
            const size_t size = static_cast<size_t>(descMip.dwHeight * descMip.lPitch);
            memcpy(rect9mip.pBits, descMip.lpSurface, size);
            //Logger::debug(str::format("BlitToD3D9Texture: Done blitting mip ", i));
          }
          mipMap->Unlock(NULL);
        } else {
          Logger::warn(str::format("BlitToD3D9Texture: Failed to lock mip ", i));
        }
        texture9->UnlockRect(i);

        SurfaceType* parentSurface = mipMap;
        mipMap = nullptr;

        if constexpr (std::is_same<SurfaceType, IDirectDrawSurface7>::value) {
          parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces7Callback);
        } else if constexpr (std::is_same<SurfaceType, IDirectDrawSurface4>::value) {
          parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces4Callback);
        } else if constexpr (std::is_same<SurfaceType, IDirectDrawSurface>::value) {
          parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfacesCallback);
        } else {
          Logger::err("BlitToD3D9Texture: Unsupported surface type");
        }
      } else {
        Logger::warn(str::format("BlitToD3D9Texture: Failed to lock D3D9 mip ", i));
      }
    }
  }

  template <typename SurfaceType, typename DescType>
  inline void BlitToD3D9Surface(
        d3d9::IDirect3DSurface9* surface9,
        SurfaceType* surface,
        const bool isDXTFormat) {
    d3d9::D3DLOCKED_RECT rect9;
    // D3DLOCK_DISCARD will get ignored for MANAGED/SYSTEMMEM, but will work on DEFAULT
    HRESULT hr9 = surface9->LockRect(&rect9, NULL, D3DLOCK_DISCARD);
    if (likely(SUCCEEDED(hr9))) {
      DescType desc;
      desc.dwSize = sizeof(DescType);
      HRESULT hr = surface->Lock(NULL, &desc, DDLOCK_READONLY, NULL);
      if (likely(SUCCEEDED(hr))) {
        // The lock pitch of a DXT surface represents its entire size, apparently
        if (isDXTFormat) {
          const size_t size = static_cast<size_t>(desc.lPitch);
          memcpy(rect9.pBits, desc.lpSurface, size);
          //Logger::debug("BlitToD3D9Surface: Done blitting DXT surface");
        } else if (desc.lPitch != rect9.Pitch) {
          //Logger::debug("BlitToD3D9Surface: Incompatible surface pitch");

          uint8_t* data9 = reinterpret_cast<uint8_t*>(rect9.pBits);
          uint8_t* data7 = reinterpret_cast<uint8_t*>(desc.lpSurface);

          const size_t copyPitch = std::min<size_t>(desc.lPitch, rect9.Pitch);
          for (uint32_t h = 0; h < desc.dwHeight; h++)
            memcpy(&data9[h * rect9.Pitch], &data7[h * desc.lPitch], copyPitch);

          //Logger::debug("BlitToD3D9Surface: Done blitting surface row by row");
        } else {
          const size_t size = static_cast<size_t>(desc.dwHeight * desc.lPitch);
          memcpy(rect9.pBits, desc.lpSurface, size);
          //Logger::debug("BlitToD3D9Surface: Done blitting surface");
        }
        surface->Unlock(NULL);
      } else {
        Logger::warn("BlitToD3D9Surface: Failed to lock surface");
      }
      surface9->UnlockRect();
    } else {
      Logger::warn("BlitToD3D9Surface: Failed to lock D3D9 surface");
    }
  }

  template <typename SurfaceType, typename DescType>
  inline void BlitToDDrawSurface(
        SurfaceType* surface,
        d3d9::IDirect3DSurface9* surface9,
        const bool isDXTFormat) {
    DescType desc;
    desc.dwSize = sizeof(DescType);
    HRESULT hr = surface->Lock(NULL, &desc, DDLOCK_WRITEONLY, NULL);
    if (likely(SUCCEEDED(hr))) {
      d3d9::D3DLOCKED_RECT rect9;
      HRESULT hr9 = surface9->LockRect(&rect9, NULL, D3DLOCK_READONLY);
      if (likely(SUCCEEDED(hr9))) {
        // The lock pitch of a DXT surface represents its entire size, apparently
        if (unlikely(isDXTFormat)) {
          const size_t size = static_cast<size_t>(desc.lPitch);
          memcpy(desc.lpSurface, rect9.pBits, size);
          //Logger::debug("BlitToDDrawSurface: Done blitting DXT surface");
        } else if (desc.lPitch != rect9.Pitch) {
          //Logger::debug("BlitToDDrawSurface: Incompatible surface pitch");

          uint8_t* data7 = reinterpret_cast<uint8_t*>(desc.lpSurface);
          uint8_t* data9 = reinterpret_cast<uint8_t*>(rect9.pBits);

          const size_t copyPitch = std::min<size_t>(desc.lPitch, rect9.Pitch);
          for (uint32_t h = 0; h < desc.dwHeight; h++)
            memcpy(&data7[h * desc.lPitch], &data9[h * rect9.Pitch], copyPitch);

          //Logger::debug("BlitToDDrawSurface: Done blitting surface row by row");
        } else {
          const size_t size = static_cast<size_t>(desc.dwHeight * desc.lPitch);
          memcpy(desc.lpSurface, rect9.pBits, size);
          //Logger::debug("BlitToDDrawSurface: Done blitting surface");
        }
        surface9->UnlockRect();
      } else {
        Logger::warn("BlitToDDrawSurface: Failed to lock D3D9 surface");
      }
      surface->Unlock(NULL);
    } else {
      Logger::warn("BlitToDDrawSurface: Failed to lock surface");
    }
  }

  inline DDCOLORKEY GetColorChannel(DWORD pixel, DWORD mask) {
    uint32_t shift = 0;
    DWORD cmask = mask;
    while ((cmask & 1) == 0) {
      cmask >>= 1;
      shift++;
    }

    uint32_t bits = 0;
    cmask = mask;
    while (cmask) {
      if (cmask & 1)
        bits++;
      cmask >>= 1;
    }

    const DWORD value = (pixel & mask) >> shift;
    const DWORD max = (1 << bits) - 1;
    const float cvalue = static_cast<float>(value) * 255.0f / static_cast<float>(max);

    DDCOLORKEY colorKey = { };
    colorKey.dwColorSpaceLowValue  = static_cast<DWORD>(std::max(0.0f, floorf(cvalue - 0.5f)));
    colorKey.dwColorSpaceHighValue = static_cast<DWORD>(std::min(255.0f, floorf(cvalue + 0.5f)));

    return colorKey;
  }

  inline DDCOLORKEY ColorKeyToARGB(const DDPIXELFORMAT* fmt, DWORD colorKey) {
    DDCOLORKEY rgbColorKey = { };

    if (unlikely(!(fmt->dwFlags & DDPF_RGB)))
      return rgbColorKey;

    DDCOLORKEY b = GetColorChannel(colorKey, fmt->dwBBitMask);
    DDCOLORKEY g = GetColorChannel(colorKey, fmt->dwGBitMask);
    DDCOLORKEY r = GetColorChannel(colorKey, fmt->dwRBitMask);
    DDCOLORKEY a = (fmt->dwFlags & DDPF_ALPHAPIXELS) ? GetColorChannel(colorKey, fmt->dwRGBAlphaBitMask)
                                                     : DDCOLORKEY{255,255};

    rgbColorKey.dwColorSpaceLowValue  = b.dwColorSpaceLowValue |
                                       (g.dwColorSpaceLowValue << 8) |
                                       (r.dwColorSpaceLowValue << 16)|
                                       (a.dwColorSpaceLowValue << 24);
    rgbColorKey.dwColorSpaceHighValue = b.dwColorSpaceHighValue |
                                       (g.dwColorSpaceHighValue << 8) |
                                       (r.dwColorSpaceHighValue << 16)|
                                       (a.dwColorSpaceHighValue << 24);

    return rgbColorKey;
  }

}