#pragma once

#include "d3d8_include.h"

namespace dxvk {

  constexpr bool isDXT(D3DFORMAT fmt) {
    return fmt == D3DFMT_DXT1
        || fmt == D3DFMT_DXT2
        || fmt == D3DFMT_DXT3
        || fmt == D3DFMT_DXT4
        || fmt == D3DFMT_DXT5;
  }

  constexpr bool isDXT(d3d9::D3DFORMAT fmt) {
    return isDXT(D3DFORMAT(fmt));
  }

  constexpr bool isUnsupportedSurfaceFormat(D3DFORMAT fmt) {
    // mirror what dxvk doesn't support in terms of d3d9 surface formats
    return fmt == D3DFMT_R8G8B8
        || fmt == D3DFMT_R3G3B2
        || fmt == D3DFMT_A8R3G3B2
        || fmt == D3DFMT_A8P8
        || fmt == D3DFMT_P8;
        // not included in the d3d8 spec
        //|| fmt == D3DFMT_CXV8U8;
  }

  constexpr bool isSupportedDepthStencilFormat(D3DFORMAT fmt) {
    // native d3d8 doesn't support D3DFMT_D32, D3DFMT_D15S1 or D3DFMT_D24X4S4
    return fmt == D3DFMT_D16_LOCKABLE
        || fmt == D3DFMT_D16
        //|| fmt == D3DFMT_D32
        //|| fmt == D3DFMT_D15S1
        //|| fmt == D3DFMT_D24X4S4
        || fmt == D3DFMT_D24S8
        || fmt == D3DFMT_D24X8;
  }

  constexpr bool isDepthStencilFormat(D3DFORMAT fmt) {
    return fmt == D3DFMT_D16_LOCKABLE
        || fmt == D3DFMT_D16
        || fmt == D3DFMT_D32
        || fmt == D3DFMT_D15S1
        || fmt == D3DFMT_D24X4S4
        || fmt == D3DFMT_D24S8
        || fmt == D3DFMT_D24X8;
  }

  // Get bytes per pixel (or 4x4 block for DXT)
  constexpr UINT getFormatStride(D3DFORMAT fmt) {
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

  constexpr uint32_t GetVertexCount8(D3DPRIMITIVETYPE type, UINT count) {
    switch (type) {
      default:
      case D3DPT_TRIANGLELIST:  return count * 3;
      case D3DPT_POINTLIST:     return count;
      case D3DPT_LINELIST:      return count * 2;
      case D3DPT_LINESTRIP:     return count + 1;
      case D3DPT_TRIANGLESTRIP: return count + 2;
      case D3DPT_TRIANGLEFAN:   return count + 2;
    }
  }

  // Essentially the same logic as D3D9VertexDecl::SetFVF
  constexpr UINT GetFVFStride(DWORD FVF) {
    uint32_t texCount = 0;

    uint32_t betas = 0;
    uint8_t betaIdx = 0xFF;

    UINT size = 0;

    switch (FVF & D3DFVF_POSITION_MASK) {
      case D3DFVF_XYZ:
      case D3DFVF_XYZB1:
      case D3DFVF_XYZB2:
      case D3DFVF_XYZB3:
      case D3DFVF_XYZB4:
      case D3DFVF_XYZB5:
        size += sizeof(float) * 3;

        if ((FVF & D3DFVF_POSITION_MASK) == D3DFVF_XYZ)
          break;

        betas = (((FVF & D3DFVF_XYZB5) - D3DFVF_XYZB1) >> 1) + 1;
        if (FVF & D3DFVF_LASTBETA_D3DCOLOR)
          betaIdx = sizeof(D3DCOLOR);
        else if (FVF & D3DFVF_LASTBETA_UBYTE4)
          betaIdx = sizeof(BYTE) * 4;
        else if ((FVF & D3DFVF_XYZB5) == D3DFVF_XYZB5)
          betaIdx = sizeof(float);

        if (betaIdx != 0xFF)
          betas--;

        if (betas > 0) {
          if (betas <= 4)
            size += sizeof(float) * betas;
        }

        if (betaIdx != 0xFF) {
          size += betaIdx;
        }
        break;

      case D3DFVF_XYZW:
      case D3DFVF_XYZRHW:
        size += sizeof(float) * 4;
        break;

      default:
        break;
    }

    if (FVF & D3DFVF_NORMAL) {
      size += sizeof(float) * 3;
    }
    if (FVF & D3DFVF_PSIZE) {
      size += sizeof(float);
    }
    if (FVF & D3DFVF_DIFFUSE) {
      size += sizeof(D3DCOLOR);
    }
    if (FVF & D3DFVF_SPECULAR) {
      size += sizeof(D3DCOLOR);
    }

    texCount = (FVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    texCount = std::min(texCount, 8u);

    for (uint32_t i = 0; i < texCount; i++) {
      switch ((FVF >> (16 + i * 2)) & 0x3) {
        case D3DFVF_TEXTUREFORMAT1:
          size += sizeof(float);
          break;

        case D3DFVF_TEXTUREFORMAT2:
          size += sizeof(float) * 2;
          break;

        case D3DFVF_TEXTUREFORMAT3:
          size += sizeof(float) * 3;
          break;

        case D3DFVF_TEXTUREFORMAT4:
          size += sizeof(float) * 4;
          break;

        default:
          break;
      }
    }

    return size;
  }

  constexpr UINT getSurfaceSize(D3DFORMAT Format, UINT Width, UINT Height) {
    if (isDXT(Format)) {
      Width = ((Width + 3) >> 2);
      Height = ((Height + 3) >> 2);
    }
    return Width * Height * getFormatStride(Format);
  }

}
