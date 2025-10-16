#pragma once

#include "ddraw_include.h"
#include "ddraw_options.h"
#include "ddraw_caps.h"

#include "../util/util_matrix.h"

#include <vector>
#include <utility>

namespace dxvk {

  struct PackedVertexBuffer {
    UINT stride;
    std::vector<uint8_t> vertexData;
  };

  struct VertexStreamInfo {
    D3DPRIMITIVETYPE d3dpt;
    D3DVERTEXTYPE d3dvt;
    DWORD dwFlags = 0;
  };

  inline bool IsValidDDrawCapsSize(DWORD size) {
    return size == sizeof(DDCAPS_DX7)
        || size == sizeof(DDCAPS_DX6)
        || size == sizeof(DDCAPS_DX5)
        || size == sizeof(DDCAPS_DX3);
  }

  // MS, in their infinite wisdom, decided to have 3 distinct versions
  // of D3DDEVICEDESC, the first shipped with D3D2/3, the second with D3D5,
  // and the third (which is what we have in modern headers) with D3D6.
  // All 3 sizes are valid in D3D6, and the first 2 are in D3D5, but let's
  // consider them all valid and save ourselves some trouble.
  inline bool IsValidD3DDeviceDescSize(DWORD size) {
    return size == sizeof(D3DDEVICEDESC)
        || size == sizeof(D3DDEVICEDESC2)
        || size == sizeof(D3DDEVICEDESC3);
  }

  // The structures used in FindDevice calls are also affected because
  // of the above D3DDEVICEDESC jank, which is just lovely...
  inline bool IsValidFindDeviceResultSize(DWORD size) {
    return size == sizeof(D3DFINDDEVICERESULT)
        || size == sizeof(D3DFINDDEVICERESULT2)
        || size == sizeof(D3DFINDDEVICERESULT3);
  }

  inline bool IsVSyncFlipFlag(DWORD flag) {
    return !(flag & DDFLIP_NOVSYNC)   ||
            (flag & DDFLIP_INTERVAL2) ||
            (flag & DDFLIP_INTERVAL3) ||
            (flag & DDFLIP_INTERVAL4);
  }

  // D3D5 D3DVERTEXTYPE to D3D6 and above DWORD vertex codes
  inline DWORD ConvertVertexType(D3DVERTEXTYPE vertexType) {
    switch (vertexType) {
      default:
      case D3DVT_VERTEX:   return D3DFVF_VERTEX;
      case D3DVT_LVERTEX:  return D3DFVF_LVERTEX;
      case D3DVT_TLVERTEX: return D3DFVF_TLVERTEX;
    }
  }

  // D3D6 and above DWORD vertex codes to D3D5 D3DVERTEXTYPE
  inline D3DVERTEXTYPE ConvertFVFType(DWORD fvfType) {
    switch (fvfType) {
      default:
      case D3DFVF_VERTEX:   return D3DVT_VERTEX;
      case D3DFVF_LVERTEX:  return D3DVT_LVERTEX;
      case D3DFVF_TLVERTEX: return D3DVT_TLVERTEX;
    }
  }

  inline d3d9::D3DTRANSFORMSTATETYPE ConvertTransformState(D3DTRANSFORMSTATETYPE tst) {
    switch (tst) {
      case D3DTRANSFORMSTATE_WORLD:  return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD);
      case D3DTRANSFORMSTATE_WORLD1: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD1);
      case D3DTRANSFORMSTATE_WORLD2: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD2);
      case D3DTRANSFORMSTATE_WORLD3: return d3d9::D3DTRANSFORMSTATETYPE(D3DTS_WORLD3);
      default: return d3d9::D3DTRANSFORMSTATETYPE(tst);
    }
  }

  inline DWORD ConvertD3D6LockFlags(DWORD lockFlags, bool isSurface) {
    DWORD lockFlagsD3D9 = 0;

    // DDLOCK_WAIT is default for DDraw surfaces, so ignore it
    // and only factor in DDLOCK_DONOTWAIT. Buffers locks have
    // a flipped logic compared to D3D9.
    if ((!isSurface && !(lockFlags & DDLOCK_WAIT))
     || (isSurface  &&  (lockFlags & DDLOCK_DONOTWAIT))) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_DONOTWAIT;
    }
    if (lockFlags & DDLOCK_NOSYSLOCK) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_NOSYSLOCK;
    }
    // Not sure if both can happen at the same time, but play it safe
    if ((lockFlags & DDLOCK_READONLY) && !(lockFlags & DDLOCK_WRITEONLY)) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_READONLY;
    }

    return lockFlagsD3D9;
  }

  inline DWORD ConvertD3D7LockFlags(DWORD lockFlags, bool legacyDiscard, bool isSurface) {
    DWORD lockFlagsD3D9 = 0;

    // DDLOCK_WAIT is default for DDraw7 surfaces, so ignore it
    // and only factor in DDLOCK_DONOTWAIT. Buffers locks have
    // a flipped logic compared to D3D9.
    if ((!isSurface && !(lockFlags & DDLOCK_WAIT))
     || (isSurface  &&  (lockFlags & DDLOCK_DONOTWAIT))) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_DONOTWAIT;
    }
    if (lockFlags & DDLOCK_NOSYSLOCK) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_NOSYSLOCK;
    }
    // Not sure if both can happen at the same time, but play it safe
    if ((lockFlags & DDLOCK_READONLY) && !(lockFlags & DDLOCK_WRITEONLY)) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_READONLY;
    }
    // This is called "legacy DISCARD" in D8VK, which apparently
    // is expected to be enforced implicitly in D3D7. Earlier versions
    // of D3D do not feature a DISCARD (or NOOVERWRITE) lock flag.
    if ((lockFlags & DDLOCK_DISCARDCONTENTS) && !legacyDiscard) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_DISCARD;
    }
    if (lockFlags & DDLOCK_NOOVERWRITE) {
      lockFlagsD3D9 |= (DWORD)D3DLOCK_NOOVERWRITE;
    }

    return lockFlagsD3D9;
  }

  inline DWORD ConvertD3D6UsageFlags(DWORD usageFlags, DWORD creationFlags, d3d9::D3DPOOL pool) {
    DWORD usageFlagsD3D9 = 0;

    // The D3D6 docs do not mention the presence of a D3DVBCAPS_DONOTCLIP flag,
    // and only the creation flag D3DDP_DONOTCLIP is touted as being usable
    if ((creationFlags & D3DDP_DONOTCLIP) || (usageFlags & D3DVBCAPS_DONOTCLIP)) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_DONOTCLIP;
    }
    if (usageFlags & D3DVBCAPS_WRITEONLY) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_WRITEONLY;
    }
    if (pool != d3d9::D3DPOOL_MANAGED) {
      // D3D6 does not use DDLOCK_DISCARDCONTENTS or
      // DDLOCK_NOOVERWRITE, however, still mark all non-MANAGED
      // buffers as DYNAMIC to handle any potential CPU read-backs
      usageFlagsD3D9 |= D3DUSAGE_DYNAMIC;
    }

    return usageFlagsD3D9;
  }

  inline DWORD ConvertD3D7UsageFlags(DWORD usageFlags, d3d9::D3DPOOL pool) {
    DWORD usageFlagsD3D9 = 0;

    if (usageFlags & D3DVBCAPS_DONOTCLIP) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_DONOTCLIP;
    }
    if (usageFlags & D3DVBCAPS_WRITEONLY) {
      usageFlagsD3D9 |= (DWORD)D3DUSAGE_WRITEONLY;
    }
    if (pool != d3d9::D3DPOOL_MANAGED) {
      // Though D3D7 does not specify it, all non-MANAGED buffers
      // need to be DYNAMIC, either due to some lock flags not
      // working properly otherwise, or for performance reasons
      usageFlagsD3D9 |= D3DUSAGE_DYNAMIC;
    }

    return usageFlagsD3D9;
  }

  inline size_t GetFVFPositionSize(DWORD fvf) {
    size_t size = 0;

    switch (fvf & D3DFVF_POSITION_MASK) {
      case D3DFVF_XYZ:
        size += 3 * sizeof(FLOAT);
        break;
      case D3DFVF_XYZRHW:
        size += 4 * sizeof(FLOAT);
        break;
      case D3DFVF_XYZB1:
        size += 4 * sizeof(FLOAT);
        break;
      case D3DFVF_XYZB2:
        size += 5 * sizeof(FLOAT);
        break;
      case D3DFVF_XYZB3:
        size += 6 * sizeof(FLOAT);
        break;
      case D3DFVF_XYZB4:
        size += 7 * sizeof(FLOAT);
        break;
      case D3DFVF_XYZB5:
        size += 8 * sizeof(FLOAT);
        break;
    }

    return size;
  }

  inline size_t GetFVFTexCoordSize(DWORD fvf, DWORD coord) {
    size_t size = 0;

    const DWORD texCoordSize = (fvf >> (coord * 2 + 16)) & 0x3;
    switch (texCoordSize) {
      // D3DFVF_TEXTUREFORMAT2 0
      case D3DFVF_TEXTUREFORMAT2:
        size += 2 * sizeof(FLOAT);
        break;
      // D3DFVF_TEXTUREFORMAT3 1
      case D3DFVF_TEXTUREFORMAT3:
        size += 3 * sizeof(FLOAT);
        break;
      // D3DFVF_TEXTUREFORMAT4 2
      case D3DFVF_TEXTUREFORMAT4:
        size += 4 * sizeof(FLOAT);
        break;
      // D3DFVF_TEXTUREFORMAT1 3
      case D3DFVF_TEXTUREFORMAT1:
        size += sizeof(FLOAT);
        break;
    }

    return size;
  }

  inline size_t GetFVFSize(DWORD fvf) {
    size_t size = 0;

    size += GetFVFPositionSize(fvf);

    if (fvf & D3DFVF_NORMAL) {
      size += 3 * sizeof(FLOAT);
    }
    if (fvf & D3DFVF_RESERVED1) {
      size += sizeof(DWORD);
    }
    if (fvf & D3DFVF_DIFFUSE) {
      size += sizeof(D3DCOLOR);
    }
    if (fvf & D3DFVF_SPECULAR) {
      size += sizeof(D3DCOLOR);
    }

    const DWORD textureCount = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    for (DWORD coord = 0; coord < textureCount; coord++)
      size += GetFVFTexCoordSize(fvf, coord);

    return size;
  }

  inline UINT GetPrimitiveCount(D3DPRIMITIVETYPE PrimitiveType, DWORD VertexCount) {
    switch (PrimitiveType) {
      case D3DPT_POINTLIST:     return static_cast<UINT>(VertexCount);
      case D3DPT_LINELIST:      return static_cast<UINT>(VertexCount / 2);
      case D3DPT_LINESTRIP:     return static_cast<UINT>(VertexCount - 1);
      default:
      case D3DPT_TRIANGLELIST:  return static_cast<UINT>(VertexCount / 3);
      case D3DPT_TRIANGLESTRIP: return static_cast<UINT>(VertexCount - 2);
      case D3DPT_TRIANGLEFAN:   return static_cast<UINT>(VertexCount - 2);
    }
  }

  inline PackedVertexBuffer TransformStridedtoUP(
        DWORD dwFVF,
        LPD3DDRAWPRIMITIVESTRIDEDDATA lpVBStrided,
        DWORD dwNumVertices) {
    PackedVertexBuffer pvb;
    pvb.stride = GetFVFSize(dwFVF);
    pvb.vertexData.resize(pvb.stride * dwNumVertices);

    const DWORD dwNumTextures = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;

    for (DWORD i = 0; i < dwNumVertices; i++) {
      uint8_t* ptr = pvb.vertexData.data() + i * pvb.stride;

      if ((dwFVF & D3DFVF_POSITION_MASK) && lpVBStrided->position.lpvData) {
        const size_t size = GetFVFPositionSize(dwFVF);
        memcpy(ptr, static_cast<uint8_t*>(lpVBStrided->position.lpvData) + i * lpVBStrided->position.dwStride, size);
        ptr += size;
      }

      if ((dwFVF & D3DFVF_NORMAL) && lpVBStrided->normal.lpvData) {
        memcpy(ptr, static_cast<uint8_t*>(lpVBStrided->normal.lpvData) + i * lpVBStrided->normal.dwStride, 3 * sizeof(FLOAT));
        ptr += 3 * sizeof(FLOAT);
      }

      if (dwFVF & D3DFVF_RESERVED1) {
        // D3DLVERTEX
        ptr += sizeof(DWORD);
      }

      if ((dwFVF & D3DFVF_DIFFUSE) && lpVBStrided->diffuse.lpvData) {
        memcpy(ptr, static_cast<uint8_t*>(lpVBStrided->diffuse.lpvData) + i * lpVBStrided->diffuse.dwStride, sizeof(D3DCOLOR));
        ptr += sizeof(D3DCOLOR);
      }

      if ((dwFVF & D3DFVF_SPECULAR) && lpVBStrided->specular.lpvData) {
        memcpy(ptr, static_cast<uint8_t*>(lpVBStrided->specular.lpvData) + i * lpVBStrided->specular.dwStride, sizeof(D3DCOLOR));
        ptr += sizeof(D3DCOLOR);
      }

      for (DWORD t = 0; t < dwNumTextures; t++) {
        if (lpVBStrided->textureCoords[t].lpvData) {
          const size_t size = GetFVFTexCoordSize(dwFVF, t);
          memcpy(ptr, static_cast<uint8_t*>(lpVBStrided->textureCoords[t].lpvData) + i * lpVBStrided->textureCoords[t].dwStride, size);
          ptr += size;
        }
      }
    }

    return pvb;
  }

  // If this D3DTEXTURESTAGESTATETYPE has been remapped to a d3d9::D3DSAMPLERSTATETYPE
  // it will be returned, otherwise returns -1u
  inline d3d9::D3DSAMPLERSTATETYPE ConvertSamplerStateType(const D3DTEXTURESTAGESTATETYPE StageType) {
    switch (StageType) {
      // 13-21:
      case D3DTSS_ADDRESSU:       return d3d9::D3DSAMP_ADDRESSU;
      case D3DTSS_ADDRESSV:       return d3d9::D3DSAMP_ADDRESSV;
      case D3DTSS_BORDERCOLOR:    return d3d9::D3DSAMP_BORDERCOLOR;
      case D3DTSS_MAGFILTER:      return d3d9::D3DSAMP_MAGFILTER;
      case D3DTSS_MINFILTER:      return d3d9::D3DSAMP_MINFILTER;
      case D3DTSS_MIPFILTER:      return d3d9::D3DSAMP_MIPFILTER;
      case D3DTSS_MIPMAPLODBIAS:  return d3d9::D3DSAMP_MIPMAPLODBIAS;
      case D3DTSS_MAXMIPLEVEL:    return d3d9::D3DSAMP_MAXMIPLEVEL;
      case D3DTSS_MAXANISOTROPY:  return d3d9::D3DSAMP_MAXANISOTROPY;
      default:                    return d3d9::D3DSAMPLERSTATETYPE(-1u);
    }
  }

  inline DWORD DecodeD3D7TexFilterValues(const D3DTEXTURESTAGESTATETYPE StageType, const DWORD FilterType7) {
    switch (StageType) {
      case D3DTSS_MAGFILTER:
        switch (FilterType7) {
          default:
          case D3DTFG_POINT:          return d3d9::D3DTEXF_POINT;
          case D3DTFG_LINEAR:         return d3d9::D3DTEXF_LINEAR;
          // 5 in D3DTEXTUREMAGFILTER, 3 in D3DTEXTUREFILTERTYPE
          case D3DTFG_ANISOTROPIC:    return d3d9::D3DTEXF_ANISOTROPIC;
        }
        break;
      case D3DTSS_MINFILTER:
        switch (FilterType7) {
          default:
          case D3DTFN_POINT:          return d3d9::D3DTEXF_POINT;
          case D3DTFN_LINEAR:         return d3d9::D3DTEXF_LINEAR;
          case D3DTFN_ANISOTROPIC:    return d3d9::D3DTEXF_ANISOTROPIC;
        }
        break;
      case D3DTSS_MIPFILTER:
        switch (FilterType7) {
          // All values in D3DTEXTUREMIPFILTER are offset by +1
          // vs D3DTEXTUREFILTERTYPE...
          default:
          case D3DTFP_NONE:           return d3d9::D3DTEXF_NONE;
          case D3DTFP_POINT:          return d3d9::D3DTEXF_POINT;
          case D3DTFP_LINEAR:         return d3d9::D3DTEXF_LINEAR;
        }
        break;
      default: return 0;
    }
  }

  inline DWORD DecodeD3D9TexFilterValues(const D3DTEXTURESTAGESTATETYPE StageType, const DWORD FilterType9) {
    switch (StageType) {
      case D3DTSS_MAGFILTER:
        switch (FilterType9) {
          default:
          case d3d9::D3DTEXF_POINT:       return D3DTFG_POINT;
          case d3d9::D3DTEXF_LINEAR:      return D3DTFG_LINEAR;
          // 5 in D3DTEXTUREMAGFILTER, 3 in D3DTEXTUREFILTERTYPE
          case d3d9::D3DTEXF_ANISOTROPIC: return D3DTFG_ANISOTROPIC;
        }
        break;
      case D3DTSS_MINFILTER:
        switch (FilterType9) {
          default:
          case d3d9::D3DTEXF_POINT:       return D3DTFN_POINT;
          case d3d9::D3DTEXF_LINEAR:      return D3DTFN_LINEAR;
          case d3d9::D3DTEXF_ANISOTROPIC: return D3DTFN_ANISOTROPIC;
        }
        break;
      case D3DTSS_MIPFILTER:
        switch (FilterType9) {
          // All values in D3DTEXTUREMIPFILTER are offset by +1
          // vs D3DTEXTUREFILTERTYPE...
          default:
          case d3d9::D3DTEXF_NONE:        return D3DTFP_NONE;
          case d3d9::D3DTEXF_POINT:       return D3DTFP_POINT;
          case d3d9::D3DTEXF_LINEAR:      return D3DTFP_LINEAR;
        }
        break;
      default: return 0;
    }
  }

  inline DWORD DecodeTextureMinValues(DWORD minFilter, DWORD mipFilter) {
    switch (minFilter) {
      case d3d9::D3DTEXF_POINT:
        switch (mipFilter) {
          default:
          case d3d9::D3DTEXF_NONE:   return D3DFILTER_NEAREST;
          case d3d9::D3DTEXF_POINT:  return D3DFILTER_MIPNEAREST;
          case d3d9::D3DTEXF_LINEAR: return D3DFILTER_LINEARMIPNEAREST;
        }
        break;
      case d3d9::D3DTEXF_LINEAR:
        switch (mipFilter) {
          default:
          case d3d9::D3DTEXF_NONE:   return D3DFILTER_LINEAR;
          case d3d9::D3DTEXF_POINT:  return D3DFILTER_MIPLINEAR;
          case d3d9::D3DTEXF_LINEAR: return D3DFILTER_LINEARMIPLINEAR;
        }
        break;
      default: return 0;
    }
  }

  inline HRESULT ValidateViewportRT(
        DWORD dwX, DWORD dwY, DWORD dwWidth,
        DWORD dwHeight, DWORD rtWidth, DWORD rtHeight) {
    if (unlikely(dwX + dwWidth  > rtWidth || dwY + dwHeight > rtHeight)) {
      // On Linux/Wine and in windowed mode, we can get in situations
      // where the actual render target dimensions are off by one
      // pixel to what the game sets them to. Allow this corner case
      // to skip the validation, in order to prevent issues.
      const bool isOnePixelWider  = dwX + dwWidth  == rtWidth  + 1;
      const bool isOnePixelTaller = dwY + dwHeight == rtHeight + 1;

      if (unlikely(isOnePixelWider || isOnePixelTaller)) {
        Logger::debug("ValidateViewportRT: Viewport exceeds render target dimensions by one pixel");
      } else {
        return DDERR_INVALIDPARAMS;
      }
    }

    return D3D_OK;
  }

  inline D3DDEVICEDESC3 GetD3D3Caps(const IID rclsid, const D3DOptions* options) {
    D3DDEVICEDESC3 desc;

    desc.dwSize    = sizeof(D3DDEVICEDESC3);

    desc.dwFlags   = D3DDD_BCLIPPING
                   | D3DDD_COLORMODEL
                   | D3DDD_DEVCAPS
                   | D3DDD_DEVICERENDERBITDEPTH
                   | D3DDD_DEVICEZBUFFERBITDEPTH
                   | D3DDD_LIGHTINGCAPS
                   | D3DDD_LINECAPS
                   | D3DDD_MAXBUFFERSIZE
                   | D3DDD_MAXVERTEXCOUNT
                   | D3DDD_TRANSFORMCAPS
                   | D3DDD_TRICAPS;

    desc.dcmColorModel = D3DCOLOR_RGB;

    desc.dwDevCaps = D3DDEVCAPS_EXECUTESYSTEMMEMORY
                   | D3DDEVCAPS_EXECUTEVIDEOMEMORY
                   | D3DDEVCAPS_FLOATTLVERTEX
                // | D3DDEVCAPS_SORTDECREASINGZ
                // | D3DDEVCAPS_SORTEXACT
                // | D3DDEVCAPS_SORTINCREASINGZ
                // | D3DDEVCAPS_TEXTURESYSTEMMEMORY
                   | D3DDEVCAPS_TEXTUREVIDEOMEMORY
                   | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY
                   | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;

    // Also advertised in D3D3
    if (rclsid == IID_IDirect3DHALDevice) {
      desc.dwDevCaps |= D3DDEVCAPS_HWRASTERIZATION
                      | D3DDEVCAPS_HWTRANSFORMANDLIGHT
                      | D3DDEVCAPS_DRAWPRIMITIVES2
                      | D3DDEVCAPS_DRAWPRIMITIVES2EX;
    }

    D3DTRANSFORMCAPS transformCaps;
    transformCaps.dwSize = sizeof(D3DTRANSFORMCAPS);
    transformCaps.dwCaps = D3DTRANSFORMCAPS_CLIP;

    desc.dtcTransformCaps = transformCaps;

    desc.bClipping = TRUE;

    D3DLIGHTINGCAPS lightingCaps;
    lightingCaps.dwSize  = sizeof(D3DLIGHTINGCAPS);
    lightingCaps.dwCaps  = D3DLIGHTCAPS_POINT
                         | D3DLIGHTCAPS_SPOT
                         | D3DLIGHTCAPS_DIRECTIONAL
                         | D3DLIGHTCAPS_PARALLELPOINT; // Not supported by D3D9
                      // | D3DLIGHTCAPS_GLSPOT; // Specific to D3D3
    lightingCaps.dwLightingModel = D3DLIGHTINGMODEL_RGB;
    lightingCaps.dwNumLights = ddrawCaps::MaxEnabledLights;

    desc.dlcLightingCaps = lightingCaps;

    D3DPRIMCAPS prim;
    prim.dwSize = sizeof(D3DPRIMCAPS);

    prim.dwMiscCaps           = D3DPMISCCAPS_CULLCCW
                              | D3DPMISCCAPS_CULLCW
                              | D3DPMISCCAPS_CULLNONE
                           // | D3DPMISCCAPS_CONFORMANT
                           // | D3DPMISCCAPS_LINEPATTERNREP
                           // | D3DPMISCCAPS_MASKPLANES
                              | D3DPMISCCAPS_MASKZ;

    prim.dwRasterCaps         = D3DPRASTERCAPS_DITHER
                              | D3DPRASTERCAPS_FOGTABLE
                              | D3DPRASTERCAPS_FOGVERTEX
                           // | D3DPRASTERCAPS_PAT
                           // | D3DPRASTERCAPS_ROP2
                           // | D3DPRASTERCAPS_STIPPLE
                              | D3DPRASTERCAPS_SUBPIXEL
                           // | D3DPRASTERCAPS_SUBPIXELX
                           // | D3DPRASTERCAPS_XOR
                              | D3DPRASTERCAPS_ZTEST;

    prim.dwZCmpCaps           = D3DPCMPCAPS_ALWAYS
                              | D3DPCMPCAPS_EQUAL
                              | D3DPCMPCAPS_GREATER
                              | D3DPCMPCAPS_GREATEREQUAL
                              | D3DPCMPCAPS_LESS
                              | D3DPCMPCAPS_LESSEQUAL
                              | D3DPCMPCAPS_NEVER
                              | D3DPCMPCAPS_NOTEQUAL;

    prim.dwSrcBlendCaps       = D3DPBLENDCAPS_BOTHINVSRCALPHA
                              | D3DPBLENDCAPS_BOTHSRCALPHA
                              | D3DPBLENDCAPS_DESTALPHA
                              | D3DPBLENDCAPS_DESTCOLOR
                              | D3DPBLENDCAPS_INVDESTALPHA
                              | D3DPBLENDCAPS_INVDESTCOLOR
                              | D3DPBLENDCAPS_INVSRCALPHA
                              | D3DPBLENDCAPS_INVSRCCOLOR
                              | D3DPBLENDCAPS_ONE
                              | D3DPBLENDCAPS_SRCALPHA
                              | D3DPBLENDCAPS_SRCALPHASAT
                              | D3DPBLENDCAPS_SRCCOLOR
                              | D3DPBLENDCAPS_ZERO;

    prim.dwDestBlendCaps      = prim.dwSrcBlendCaps;

    prim.dwAlphaCmpCaps       = prim.dwZCmpCaps;

    prim.dwShadeCaps          = D3DPSHADECAPS_ALPHAFLATBLEND
                           // | D3DPSHADECAPS_ALPHAFLATSTIPPLED
                              | D3DPSHADECAPS_ALPHAGOURAUDBLEND
                           // | D3DPSHADECAPS_ALPHAGOURAUDSTIPPLED
                           // | D3DPSHADECAPS_ALPHAPHONGBLEND
                           // | D3DPSHADECAPS_ALPHAPHONGSTIPPLED
                           // | D3DPSHADECAPS_COLORFLATMONO
                              | D3DPSHADECAPS_COLORFLATRGB
                           // | D3DPSHADECAPS_COLORGOURAUDMONO
                              | D3DPSHADECAPS_COLORGOURAUDRGB
                           // | D3DPSHADECAPS_COLORPHONGMONO
                           // | D3DPSHADECAPS_COLORPHONGRGB
                              | D3DPSHADECAPS_FOGFLAT
                              | D3DPSHADECAPS_FOGGOURAUD
                           // | D3DPSHADECAPS_FOGPHONG
                           // | D3DPSHADECAPS_SPECULARFLATMONO
                              | D3DPSHADECAPS_SPECULARFLATRGB
                           // | D3DPSHADECAPS_SPECULARGOURAUDMONO
                              | D3DPSHADECAPS_SPECULARGOURAUDRGB;
                           // | D3DPSHADECAPS_SPECULARPHONGMONO
                           // | D3DPSHADECAPS_SPECULARPHONGRGB;

    prim.dwTextureCaps        = D3DPTEXTURECAPS_ALPHA
                              | D3DPTEXTURECAPS_BORDER
                              | D3DPTEXTURECAPS_PERSPECTIVE
                              | D3DPTEXTURECAPS_POW2 // Always reported on early D3D cards
                              | D3DPTEXTURECAPS_NONPOW2CONDITIONAL // Reported only on later (D3D7/8) D3D cards
                           // | D3DPTEXTURECAPS_SQUAREONLY
                              | D3DPTEXTURECAPS_TRANSPARENCY;

    prim.dwTextureFilterCaps  = D3DPTFILTERCAPS_LINEAR
                              | D3DPTFILTERCAPS_LINEARMIPLINEAR
                              | D3DPTFILTERCAPS_LINEARMIPNEAREST
                              | D3DPTFILTERCAPS_MIPLINEAR
                              | D3DPTFILTERCAPS_MIPNEAREST
                              | D3DPTFILTERCAPS_NEAREST;

    prim.dwTextureBlendCaps   = D3DPTBLENDCAPS_COPY
                              | D3DPTBLENDCAPS_DECAL
                              | D3DPTBLENDCAPS_DECALALPHA
                              | D3DPTBLENDCAPS_DECALMASK
                              | D3DPTBLENDCAPS_MODULATE
                              | D3DPTBLENDCAPS_MODULATEALPHA
                              | D3DPTBLENDCAPS_MODULATEMASK;

    prim.dwTextureAddressCaps = D3DPTADDRESSCAPS_CLAMP
                              | D3DPTADDRESSCAPS_MIRROR
                              | D3DPTADDRESSCAPS_WRAP;

    prim.dwStippleWidth       = 0;
    prim.dwStippleHeight      = 0;

    desc.dpcLineCaps          = prim;
    desc.dpcTriCaps           = prim;

    desc.dwDeviceRenderBitDepth  = DDBD_16 | DDBD_24 | DDBD_32;
    desc.dwDeviceZBufferBitDepth = options->supportD16 ? DDBD_16 | DDBD_24 : DDBD_24;
    desc.dwMaxBufferSize         = 0;
    desc.dwMaxVertexCount        = D3DMAXNUMVERTICES;

    return desc;
  }

  inline D3DDEVICEDESC2 GetD3D5Caps(const IID rclsid, const D3DOptions* options) {
    D3DDEVICEDESC2 desc;

    desc.dwSize    = sizeof(D3DDEVICEDESC2);

    desc.dwFlags   = D3DDD_BCLIPPING
                   | D3DDD_COLORMODEL
                   | D3DDD_DEVCAPS
                   | D3DDD_DEVICERENDERBITDEPTH
                   | D3DDD_DEVICEZBUFFERBITDEPTH
                   | D3DDD_LIGHTINGCAPS
                   | D3DDD_LINECAPS
                   | D3DDD_MAXBUFFERSIZE
                   | D3DDD_MAXVERTEXCOUNT
                   | D3DDD_TRANSFORMCAPS
                   | D3DDD_TRICAPS;

    desc.dcmColorModel = D3DCOLOR_RGB;

    desc.dwDevCaps = D3DDEVCAPS_CANRENDERAFTERFLIP
                   | D3DDEVCAPS_DRAWPRIMTLVERTEX
                   | D3DDEVCAPS_EXECUTESYSTEMMEMORY
                   | D3DDEVCAPS_EXECUTEVIDEOMEMORY
                   | D3DDEVCAPS_FLOATTLVERTEX
                // | D3DDEVCAPS_SEPARATETEXTUREMEMORIES
                // | D3DDEVCAPS_SORTDECREASINGZ
                // | D3DDEVCAPS_SORTEXACT
                // | D3DDEVCAPS_SORTINCREASINGZ
                // | D3DDEVCAPS_TEXTURENONLOCALVIDMEM // Exposed through a config option
                // | D3DDEVCAPS_TEXTURESYSTEMMEMORY
                   | D3DDEVCAPS_TEXTUREVIDEOMEMORY
                   | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY
                   | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;

    // Powerslide uses a broken rendering path if non-local video memory is advertized
    if (likely(options->nonLocalVideoMemory)) {
      desc.dwDevCaps |= D3DDEVCAPS_TEXTURENONLOCALVIDMEM;
    }

    // Also advertised in D3D5
    if (rclsid == IID_IDirect3DHALDevice) {
      desc.dwDevCaps |= D3DDEVCAPS_HWRASTERIZATION
                      | D3DDEVCAPS_HWTRANSFORMANDLIGHT
                      | D3DDEVCAPS_DRAWPRIMITIVES2
                      | D3DDEVCAPS_DRAWPRIMITIVES2EX;
    }

    D3DTRANSFORMCAPS transformCaps;
    transformCaps.dwSize = sizeof(D3DTRANSFORMCAPS);
    transformCaps.dwCaps = D3DTRANSFORMCAPS_CLIP;

    desc.dtcTransformCaps = transformCaps;

    desc.bClipping = TRUE;

    D3DLIGHTINGCAPS lightingCaps;
    lightingCaps.dwSize  = sizeof(D3DLIGHTINGCAPS);
    lightingCaps.dwCaps  = D3DLIGHTCAPS_POINT
                         | D3DLIGHTCAPS_SPOT
                         | D3DLIGHTCAPS_DIRECTIONAL
                         | D3DLIGHTCAPS_PARALLELPOINT; // Not supported by D3D9
    lightingCaps.dwLightingModel = D3DLIGHTINGMODEL_RGB;
    lightingCaps.dwNumLights = ddrawCaps::MaxEnabledLights;

    desc.dlcLightingCaps = lightingCaps;

    D3DPRIMCAPS prim;
    prim.dwSize = sizeof(D3DPRIMCAPS);

    prim.dwMiscCaps           = D3DPMISCCAPS_CULLCCW
                              | D3DPMISCCAPS_CULLCW
                              | D3DPMISCCAPS_CULLNONE
                           // | D3DPMISCCAPS_CONFORMANT
                           // | D3DPMISCCAPS_LINEPATTERNREP
                           // | D3DPMISCCAPS_MASKPLANES
                              | D3DPMISCCAPS_MASKZ;

    prim.dwRasterCaps         = D3DPRASTERCAPS_ANISOTROPY
                              | D3DPRASTERCAPS_ANTIALIASEDGES // Technically not implemented in D3D9
                           // | D3DPRASTERCAPS_ANTIALIASSORTDEPENDENT
                           // | D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT
                              | D3DPRASTERCAPS_DITHER
                              | D3DPRASTERCAPS_FOGRANGE
                              | D3DPRASTERCAPS_FOGTABLE
                              | D3DPRASTERCAPS_FOGVERTEX
                              | D3DPRASTERCAPS_MIPMAPLODBIAS
                           // | D3DPRASTERCAPS_PAT
                           // | D3DPRASTERCAPS_ROP2
                           // | D3DPRASTERCAPS_STIPPLE
                              | D3DPRASTERCAPS_SUBPIXEL
                           // | D3DPRASTERCAPS_SUBPIXELX
                           // | D3DPRASTERCAPS_TRANSLUCENTSORTINDEPENDENT
                           // | D3DPRASTERCAPS_WBUFFER
                              | D3DPRASTERCAPS_WFOG
                           // | D3DPRASTERCAPS_XOR
                              | D3DPRASTERCAPS_ZBIAS
                           // | D3DPRASTERCAPS_ZBUFFERLESSHSR
                              | D3DPRASTERCAPS_ZFOG
                              | D3DPRASTERCAPS_ZTEST;

    if (unlikely(options->emulateFSAA != FSAAEmulation::Disabled)) {
      prim.dwRasterCaps |= D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT;
    }

    prim.dwZCmpCaps           = D3DPCMPCAPS_ALWAYS
                              | D3DPCMPCAPS_EQUAL
                              | D3DPCMPCAPS_GREATER
                              | D3DPCMPCAPS_GREATEREQUAL
                              | D3DPCMPCAPS_LESS
                              | D3DPCMPCAPS_LESSEQUAL
                              | D3DPCMPCAPS_NEVER
                              | D3DPCMPCAPS_NOTEQUAL;

    prim.dwSrcBlendCaps       = D3DPBLENDCAPS_BOTHINVSRCALPHA
                              | D3DPBLENDCAPS_BOTHSRCALPHA
                              | D3DPBLENDCAPS_DESTALPHA
                              | D3DPBLENDCAPS_DESTCOLOR
                              | D3DPBLENDCAPS_INVDESTALPHA
                              | D3DPBLENDCAPS_INVDESTCOLOR
                              | D3DPBLENDCAPS_INVSRCALPHA
                              | D3DPBLENDCAPS_INVSRCCOLOR
                              | D3DPBLENDCAPS_ONE
                              | D3DPBLENDCAPS_SRCALPHA
                              | D3DPBLENDCAPS_SRCALPHASAT
                              | D3DPBLENDCAPS_SRCCOLOR
                              | D3DPBLENDCAPS_ZERO;

    prim.dwDestBlendCaps      = prim.dwSrcBlendCaps;

    prim.dwAlphaCmpCaps       = prim.dwZCmpCaps;

    prim.dwShadeCaps          = D3DPSHADECAPS_ALPHAFLATBLEND
                           // | D3DPSHADECAPS_ALPHAFLATSTIPPLED
                              | D3DPSHADECAPS_ALPHAGOURAUDBLEND
                           // | D3DPSHADECAPS_ALPHAGOURAUDSTIPPLED
                           // | D3DPSHADECAPS_ALPHAPHONGBLEND
                           // | D3DPSHADECAPS_ALPHAPHONGSTIPPLED
                           // | D3DPSHADECAPS_COLORFLATMONO
                              | D3DPSHADECAPS_COLORFLATRGB
                           // | D3DPSHADECAPS_COLORGOURAUDMONO
                              | D3DPSHADECAPS_COLORGOURAUDRGB
                           // | D3DPSHADECAPS_COLORPHONGMONO
                           // | D3DPSHADECAPS_COLORPHONGRGB
                              | D3DPSHADECAPS_FOGFLAT
                              | D3DPSHADECAPS_FOGGOURAUD
                           // | D3DPSHADECAPS_FOGPHONG
                           // | D3DPSHADECAPS_SPECULARFLATMONO
                              | D3DPSHADECAPS_SPECULARFLATRGB
                           // | D3DPSHADECAPS_SPECULARGOURAUDMONO
                              | D3DPSHADECAPS_SPECULARGOURAUDRGB;
                           // | D3DPSHADECAPS_SPECULARPHONGMONO
                           // | D3DPSHADECAPS_SPECULARPHONGRGB;

    prim.dwTextureCaps        = D3DPTEXTURECAPS_ALPHA
                              | D3DPTEXTURECAPS_BORDER
                              | D3DPTEXTURECAPS_PERSPECTIVE
                              | D3DPTEXTURECAPS_POW2 // Always reported on early D3D cards
                              | D3DPTEXTURECAPS_NONPOW2CONDITIONAL // Reported only on later (D3D7/8) D3D cards
                           // | D3DPTEXTURECAPS_SQUAREONLY
                              | D3DPTEXTURECAPS_TRANSPARENCY;

    prim.dwTextureFilterCaps  = D3DPTFILTERCAPS_LINEAR
                              | D3DPTFILTERCAPS_LINEARMIPLINEAR
                              | D3DPTFILTERCAPS_LINEARMIPNEAREST
                              | D3DPTFILTERCAPS_MIPLINEAR
                              | D3DPTFILTERCAPS_MIPNEAREST
                              | D3DPTFILTERCAPS_NEAREST;

    prim.dwTextureBlendCaps   = D3DPTBLENDCAPS_ADD
                              | D3DPTBLENDCAPS_COPY
                              | D3DPTBLENDCAPS_DECAL
                              | D3DPTBLENDCAPS_DECALALPHA
                              | D3DPTBLENDCAPS_DECALMASK
                              | D3DPTBLENDCAPS_MODULATE
                              | D3DPTBLENDCAPS_MODULATEALPHA
                              | D3DPTBLENDCAPS_MODULATEMASK;

    prim.dwTextureAddressCaps = D3DPTADDRESSCAPS_BORDER
                              | D3DPTADDRESSCAPS_CLAMP
                              | D3DPTADDRESSCAPS_INDEPENDENTUV
                              | D3DPTADDRESSCAPS_MIRROR
                              | D3DPTADDRESSCAPS_WRAP;

    prim.dwStippleWidth       = 0;
    prim.dwStippleHeight      = 0;

    desc.dpcLineCaps          = prim;
    desc.dpcTriCaps           = prim;

    desc.dwDeviceRenderBitDepth  = DDBD_16 | DDBD_24 | DDBD_32;
    desc.dwDeviceZBufferBitDepth = options->supportD16 ? DDBD_16 | DDBD_24 : DDBD_24;
    desc.dwMaxBufferSize         = 0;
    desc.dwMaxVertexCount        = D3DMAXNUMVERTICES;
    desc.dwMinTextureWidth       = 1;
    desc.dwMinTextureHeight      = 1;
    desc.dwMaxTextureWidth       = ddrawCaps::MaxTextureDimension;
    desc.dwMaxTextureHeight      = ddrawCaps::MaxTextureDimension;
    desc.dwMinStippleWidth       = 0;
    desc.dwMinStippleHeight      = 0;
    desc.dwMaxStippleWidth       = 0;
    desc.dwMaxStippleHeight      = 0;

    return desc;
  }

  inline D3DDEVICEDESC GetD3D6Caps(const IID rclsid, const D3DOptions* options) {
    D3DDEVICEDESC desc;

    desc.dwSize    = sizeof(D3DDEVICEDESC);

    desc.dwFlags   = D3DDD_BCLIPPING
                   | D3DDD_COLORMODEL
                   | D3DDD_DEVCAPS
                   | D3DDD_DEVICERENDERBITDEPTH
                   | D3DDD_DEVICEZBUFFERBITDEPTH
                   | D3DDD_LIGHTINGCAPS
                   | D3DDD_LINECAPS
                   | D3DDD_MAXBUFFERSIZE
                   | D3DDD_MAXVERTEXCOUNT
                   | D3DDD_TRANSFORMCAPS
                   | D3DDD_TRICAPS;

    desc.dcmColorModel = D3DCOLOR_RGB;

    desc.dwDevCaps = D3DDEVCAPS_CANRENDERAFTERFLIP
                   | D3DDEVCAPS_DRAWPRIMTLVERTEX
                   | D3DDEVCAPS_EXECUTESYSTEMMEMORY
                   | D3DDEVCAPS_EXECUTEVIDEOMEMORY
                   | D3DDEVCAPS_FLOATTLVERTEX
                // | D3DDEVCAPS_SEPARATETEXTUREMEMORIES
                // | D3DDEVCAPS_SORTDECREASINGZ
                // | D3DDEVCAPS_SORTEXACT
                // | D3DDEVCAPS_SORTINCREASINGZ
                // | D3DDEVCAPS_TEXTURENONLOCALVIDMEM // Exposed through a config option
                // | D3DDEVCAPS_TEXTURESYSTEMMEMORY
                   | D3DDEVCAPS_TEXTUREVIDEOMEMORY
                   | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY
                   | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;

    // Powerslide uses a broken rendering path if non-local video memory is advertized
    if (likely(options->nonLocalVideoMemory)) {
      desc.dwDevCaps |= D3DDEVCAPS_TEXTURENONLOCALVIDMEM;
    }

    // Also advertised in D3D6
    if (rclsid == IID_IDirect3DHALDevice) {
      desc.dwDevCaps |= D3DDEVCAPS_HWRASTERIZATION
                      | D3DDEVCAPS_HWTRANSFORMANDLIGHT
                      | D3DDEVCAPS_DRAWPRIMITIVES2
                      | D3DDEVCAPS_DRAWPRIMITIVES2EX;
    }

    D3DTRANSFORMCAPS transformCaps;
    transformCaps.dwSize = sizeof(D3DTRANSFORMCAPS);
    transformCaps.dwCaps = D3DTRANSFORMCAPS_CLIP;

    desc.dtcTransformCaps = transformCaps;

    desc.bClipping = TRUE;

    D3DLIGHTINGCAPS lightingCaps;
    lightingCaps.dwSize  = sizeof(D3DLIGHTINGCAPS);
    lightingCaps.dwCaps  = D3DLIGHTCAPS_POINT
                         | D3DLIGHTCAPS_SPOT
                         | D3DLIGHTCAPS_DIRECTIONAL
                         | D3DLIGHTCAPS_PARALLELPOINT; // Not supported by D3D9
    lightingCaps.dwLightingModel = D3DLIGHTINGMODEL_RGB;
    lightingCaps.dwNumLights = ddrawCaps::MaxEnabledLights;

    desc.dlcLightingCaps = lightingCaps;

    D3DPRIMCAPS prim;
    prim.dwSize = sizeof(D3DPRIMCAPS);

    prim.dwMiscCaps           = D3DPMISCCAPS_CULLCCW
                              | D3DPMISCCAPS_CULLCW
                              | D3DPMISCCAPS_CULLNONE
                           // | D3DPMISCCAPS_CONFORMANT
                           // | D3DPMISCCAPS_LINEPATTERNREP
                           // | D3DPMISCCAPS_MASKPLANES
                              | D3DPMISCCAPS_MASKZ;

    prim.dwRasterCaps         = D3DPRASTERCAPS_ANISOTROPY
                              | D3DPRASTERCAPS_ANTIALIASEDGES // Technically not implemented in D3D9
                           // | D3DPRASTERCAPS_ANTIALIASSORTDEPENDENT
                           // | D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT
                              | D3DPRASTERCAPS_DITHER
                              | D3DPRASTERCAPS_FOGRANGE
                              | D3DPRASTERCAPS_FOGTABLE
                              | D3DPRASTERCAPS_FOGVERTEX
                              | D3DPRASTERCAPS_MIPMAPLODBIAS
                           // | D3DPRASTERCAPS_PAT
                           // | D3DPRASTERCAPS_ROP2
                           // | D3DPRASTERCAPS_STIPPLE
                              | D3DPRASTERCAPS_SUBPIXEL
                           // | D3DPRASTERCAPS_SUBPIXELX
                           // | D3DPRASTERCAPS_TRANSLUCENTSORTINDEPENDENT
                           // | D3DPRASTERCAPS_WBUFFER
                              | D3DPRASTERCAPS_WFOG
                           // | D3DPRASTERCAPS_XOR
                              | D3DPRASTERCAPS_ZBIAS
                           // | D3DPRASTERCAPS_ZBUFFERLESSHSR
                              | D3DPRASTERCAPS_ZFOG
                              | D3DPRASTERCAPS_ZTEST;

    if (unlikely(options->emulateFSAA != FSAAEmulation::Disabled)) {
      prim.dwRasterCaps |= D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT;
    }

    prim.dwZCmpCaps           = D3DPCMPCAPS_ALWAYS
                              | D3DPCMPCAPS_EQUAL
                              | D3DPCMPCAPS_GREATER
                              | D3DPCMPCAPS_GREATEREQUAL
                              | D3DPCMPCAPS_LESS
                              | D3DPCMPCAPS_LESSEQUAL
                              | D3DPCMPCAPS_NEVER
                              | D3DPCMPCAPS_NOTEQUAL;

    prim.dwSrcBlendCaps       = D3DPBLENDCAPS_BOTHINVSRCALPHA
                              | D3DPBLENDCAPS_BOTHSRCALPHA
                              | D3DPBLENDCAPS_DESTALPHA
                              | D3DPBLENDCAPS_DESTCOLOR
                              | D3DPBLENDCAPS_INVDESTALPHA
                              | D3DPBLENDCAPS_INVDESTCOLOR
                              | D3DPBLENDCAPS_INVSRCALPHA
                              | D3DPBLENDCAPS_INVSRCCOLOR
                              | D3DPBLENDCAPS_ONE
                              | D3DPBLENDCAPS_SRCALPHA
                              | D3DPBLENDCAPS_SRCALPHASAT
                              | D3DPBLENDCAPS_SRCCOLOR
                              | D3DPBLENDCAPS_ZERO;

    prim.dwDestBlendCaps      = prim.dwSrcBlendCaps;

    prim.dwAlphaCmpCaps       = prim.dwZCmpCaps;

    prim.dwShadeCaps          = D3DPSHADECAPS_ALPHAFLATBLEND
                           // | D3DPSHADECAPS_ALPHAFLATSTIPPLED
                              | D3DPSHADECAPS_ALPHAGOURAUDBLEND
                           // | D3DPSHADECAPS_ALPHAGOURAUDSTIPPLED
                           // | D3DPSHADECAPS_ALPHAPHONGBLEND
                           // | D3DPSHADECAPS_ALPHAPHONGSTIPPLED
                           // | D3DPSHADECAPS_COLORFLATMONO
                              | D3DPSHADECAPS_COLORFLATRGB
                           // | D3DPSHADECAPS_COLORGOURAUDMONO
                              | D3DPSHADECAPS_COLORGOURAUDRGB
                           // | D3DPSHADECAPS_COLORPHONGMONO
                           // | D3DPSHADECAPS_COLORPHONGRGB
                              | D3DPSHADECAPS_FOGFLAT
                              | D3DPSHADECAPS_FOGGOURAUD
                           // | D3DPSHADECAPS_FOGPHONG
                           // | D3DPSHADECAPS_SPECULARFLATMONO
                              | D3DPSHADECAPS_SPECULARFLATRGB
                           // | D3DPSHADECAPS_SPECULARGOURAUDMONO
                              | D3DPSHADECAPS_SPECULARGOURAUDRGB;
                           // | D3DPSHADECAPS_SPECULARPHONGMONO
                           // | D3DPSHADECAPS_SPECULARPHONGRGB;

    prim.dwTextureCaps        = D3DPTEXTURECAPS_ALPHA
                              | D3DPTEXTURECAPS_ALPHAPALETTE
                              | D3DPTEXTURECAPS_BORDER
                              | D3DPTEXTURECAPS_PERSPECTIVE
                              | D3DPTEXTURECAPS_POW2 // Always reported on early D3D cards
                              | D3DPTEXTURECAPS_NONPOW2CONDITIONAL // Reported only on later (D3D7/8) D3D cards
                           // | D3DPTEXTURECAPS_SQUAREONLY
                              | D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE
                              | D3DPTEXTURECAPS_TRANSPARENCY;

    prim.dwTextureFilterCaps  = D3DPTFILTERCAPS_LINEAR
                              | D3DPTFILTERCAPS_LINEARMIPLINEAR
                              | D3DPTFILTERCAPS_LINEARMIPNEAREST
                              | D3DPTFILTERCAPS_MIPLINEAR
                              | D3DPTFILTERCAPS_MIPNEAREST
                              | D3DPTFILTERCAPS_NEAREST
                           // | D3DPTFILTERCAPS_MAGFAFLATCUBIC
                              | D3DPTFILTERCAPS_MAGFANISOTROPIC
                           // | D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC
                              | D3DPTFILTERCAPS_MAGFLINEAR
                              | D3DPTFILTERCAPS_MAGFPOINT
                              | D3DPTFILTERCAPS_MINFANISOTROPIC
                              | D3DPTFILTERCAPS_MINFLINEAR
                              | D3DPTFILTERCAPS_MINFPOINT
                              | D3DPTFILTERCAPS_MIPFLINEAR
                              | D3DPTFILTERCAPS_MIPFPOINT;

    prim.dwTextureBlendCaps   = D3DPTBLENDCAPS_ADD
                              | D3DPTBLENDCAPS_COPY
                              | D3DPTBLENDCAPS_DECAL
                              | D3DPTBLENDCAPS_DECALALPHA
                              | D3DPTBLENDCAPS_DECALMASK
                              | D3DPTBLENDCAPS_MODULATE
                              | D3DPTBLENDCAPS_MODULATEALPHA
                              | D3DPTBLENDCAPS_MODULATEMASK;

    prim.dwTextureAddressCaps = D3DPTADDRESSCAPS_BORDER
                              | D3DPTADDRESSCAPS_CLAMP
                              | D3DPTADDRESSCAPS_INDEPENDENTUV
                              | D3DPTADDRESSCAPS_MIRROR
                              | D3DPTADDRESSCAPS_WRAP;

    prim.dwStippleWidth       = 0;
    prim.dwStippleHeight      = 0;

    desc.dpcLineCaps          = prim;
    desc.dpcTriCaps           = prim;

    desc.dwDeviceRenderBitDepth  = DDBD_16 | DDBD_24 | DDBD_32;
    desc.dwDeviceZBufferBitDepth = options->supportD16 ? DDBD_16 | DDBD_24 : DDBD_24;
    desc.dwMaxBufferSize         = 0;
    desc.dwMaxVertexCount        = D3DMAXNUMVERTICES;
    desc.dwMinTextureWidth       = 1;
    desc.dwMinTextureHeight      = 1;
    desc.dwMaxTextureWidth       = ddrawCaps::MaxTextureDimension;
    desc.dwMaxTextureHeight      = ddrawCaps::MaxTextureDimension;
    desc.dwMinStippleWidth       = 0;
    desc.dwMinStippleHeight      = 0;
    desc.dwMaxStippleWidth       = 0;
    desc.dwMaxStippleHeight      = 0;
    desc.dwMaxTextureRepeat      = 8192;
    desc.dwMaxTextureAspectRatio = ddrawCaps::MaxTextureDimension;
    desc.dwMaxAnisotropy         = 16;
    desc.dvGuardBandLeft         = -32768.0f;
    desc.dvGuardBandTop          = -32768.0f;
    desc.dvGuardBandRight        = 32768.0f;
    desc.dvGuardBandBottom       = 32768.0f;
    desc.dvExtentsAdjust         = 0.0f;

    desc.dwStencilCaps        = D3DSTENCILCAPS_DECR
                              | D3DSTENCILCAPS_DECRSAT
                              | D3DSTENCILCAPS_INCR
                              | D3DSTENCILCAPS_INCRSAT
                              | D3DSTENCILCAPS_INVERT
                              | D3DSTENCILCAPS_KEEP
                              | D3DSTENCILCAPS_REPLACE
                              | D3DSTENCILCAPS_ZERO;

    desc.dwFVFCaps                = (ddrawCaps::MaxSimultaneousTextures & D3DFVFCAPS_TEXCOORDCOUNTMASK);
                                // | D3DFVFCAPS_DONOTSTRIPELEMENTS;

    desc.dwTextureOpCaps          = D3DTEXOPCAPS_ADD
                                  | D3DTEXOPCAPS_ADDSIGNED
                                  | D3DTEXOPCAPS_ADDSIGNED2X
                                  | D3DTEXOPCAPS_ADDSMOOTH
                                  | D3DTEXOPCAPS_BLENDCURRENTALPHA
                                  | D3DTEXOPCAPS_BLENDDIFFUSEALPHA
                                  | D3DTEXOPCAPS_BLENDFACTORALPHA
                                  | D3DTEXOPCAPS_BLENDTEXTUREALPHA
                                  | D3DTEXOPCAPS_BLENDTEXTUREALPHAPM
                                  | D3DTEXOPCAPS_BUMPENVMAP
                                  | D3DTEXOPCAPS_BUMPENVMAPLUMINANCE
                                  | D3DTEXOPCAPS_DISABLE
                                  | D3DTEXOPCAPS_DOTPRODUCT3
                                  | D3DTEXOPCAPS_MODULATE
                                  | D3DTEXOPCAPS_MODULATE2X
                                  | D3DTEXOPCAPS_MODULATE4X
                                  | D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR
                                  | D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA
                                  | D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR
                                  | D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA
                                  | D3DTEXOPCAPS_PREMODULATE
                                  | D3DTEXOPCAPS_SELECTARG1
                                  | D3DTEXOPCAPS_SELECTARG2
                                  | D3DTEXOPCAPS_SUBTRACT;

    desc.wMaxTextureBlendStages   = ddrawCaps::MaxTextureBlendStages;
    desc.wMaxSimultaneousTextures = ddrawCaps::MaxSimultaneousTextures;

    return desc;
  }

  inline D3DDEVICEDESC7 GetD3D7Caps(const IID rclsid, const D3DOptions* options) {
    D3DDEVICEDESC7 desc7;

    desc7.dwDevCaps = D3DDEVCAPS_CANBLTSYSTONONLOCAL
                    | D3DDEVCAPS_CANRENDERAFTERFLIP
                    | D3DDEVCAPS_DRAWPRIMTLVERTEX
                    | D3DDEVCAPS_EXECUTESYSTEMMEMORY
                    | D3DDEVCAPS_EXECUTEVIDEOMEMORY
                    | D3DDEVCAPS_FLOATTLVERTEX
                 // | D3DDEVCAPS_HWRASTERIZATION
                 // | D3DDEVCAPS_HWTRANSFORMANDLIGHT
                 // | D3DDEVCAPS_DRAWPRIMITIVES2
                 // | D3DDEVCAPS_SEPARATETEXTUREMEMORIES
                 // | D3DDEVCAPS_DRAWPRIMITIVES2EX
                 // | D3DDEVCAPS_SORTDECREASINGZ
                 // | D3DDEVCAPS_SORTEXACT
                 // | D3DDEVCAPS_SORTINCREASINGZ
                 // | D3DDEVCAPS_STRIDEDVERTICES // Mentioned in the docs, but apparently is a ghost
                 // | D3DDEVCAPS_TEXTURENONLOCALVIDMEM // Exposed through a config option
                 // | D3DDEVCAPS_TEXTURESYSTEMMEMORY
                    | D3DDEVCAPS_TEXTUREVIDEOMEMORY
                    | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY
                    | D3DDEVCAPS_TLVERTEXVIDEOMEMORY;

    // Powerslide uses a broken rendering path if non-local video memory is advertized
    if (likely(options->nonLocalVideoMemory)) {
      desc7.dwDevCaps |= D3DDEVCAPS_TEXTURENONLOCALVIDMEM;
    }

    if (rclsid == IID_IDirect3DTnLHalDevice) {
      desc7.dwDevCaps |= D3DDEVCAPS_HWRASTERIZATION
                       | D3DDEVCAPS_HWTRANSFORMANDLIGHT
                       | D3DDEVCAPS_DRAWPRIMITIVES2
                       | D3DDEVCAPS_DRAWPRIMITIVES2EX;
    }
    else if (rclsid == IID_IDirect3DHALDevice) {
      desc7.dwDevCaps |= D3DDEVCAPS_HWRASTERIZATION
                       | D3DDEVCAPS_DRAWPRIMITIVES2
                       | D3DDEVCAPS_DRAWPRIMITIVES2EX;
    }

    D3DPRIMCAPS prim;
    prim.dwSize = sizeof(D3DPRIMCAPS);

    prim.dwMiscCaps           = D3DPMISCCAPS_CULLCCW
                              | D3DPMISCCAPS_CULLCW
                              | D3DPMISCCAPS_CULLNONE
                           // | D3DPMISCCAPS_CONFORMANT
                           // | D3DPMISCCAPS_LINEPATTERNREP
                           // | D3DPMISCCAPS_MASKPLANES
                              | D3DPMISCCAPS_MASKZ;

    prim.dwRasterCaps         = D3DPRASTERCAPS_ANISOTROPY
                              | D3DPRASTERCAPS_ANTIALIASEDGES // Technically not implemented in D3D9
                           // | D3DPRASTERCAPS_ANTIALIASSORTDEPENDENT
                           // | D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT
                              | D3DPRASTERCAPS_DITHER
                              | D3DPRASTERCAPS_FOGRANGE
                              | D3DPRASTERCAPS_FOGTABLE
                              | D3DPRASTERCAPS_FOGVERTEX
                              | D3DPRASTERCAPS_MIPMAPLODBIAS
                           // | D3DPRASTERCAPS_PAT
                           // | D3DPRASTERCAPS_ROP2
                           // | D3DPRASTERCAPS_STIPPLE
                              | D3DPRASTERCAPS_SUBPIXEL
                           // | D3DPRASTERCAPS_SUBPIXELX
                           // | D3DPRASTERCAPS_TRANSLUCENTSORTINDEPENDENT
                           // | D3DPRASTERCAPS_WBUFFER
                              | D3DPRASTERCAPS_WFOG
                           // | D3DPRASTERCAPS_XOR
                              | D3DPRASTERCAPS_ZBIAS
                           // | D3DPRASTERCAPS_ZBUFFERLESSHSR
                              | D3DPRASTERCAPS_ZFOG
                              | D3DPRASTERCAPS_ZTEST;

    if (unlikely(options->emulateFSAA != FSAAEmulation::Disabled)) {
      prim.dwRasterCaps |= D3DPRASTERCAPS_ANTIALIASSORTINDEPENDENT;
    }

    prim.dwZCmpCaps           = D3DPCMPCAPS_ALWAYS
                              | D3DPCMPCAPS_EQUAL
                              | D3DPCMPCAPS_GREATER
                              | D3DPCMPCAPS_GREATEREQUAL
                              | D3DPCMPCAPS_LESS
                              | D3DPCMPCAPS_LESSEQUAL
                              | D3DPCMPCAPS_NEVER
                              | D3DPCMPCAPS_NOTEQUAL;

    prim.dwSrcBlendCaps       = D3DPBLENDCAPS_BOTHINVSRCALPHA
                              | D3DPBLENDCAPS_BOTHSRCALPHA
                              | D3DPBLENDCAPS_DESTALPHA
                              | D3DPBLENDCAPS_DESTCOLOR
                              | D3DPBLENDCAPS_INVDESTALPHA
                              | D3DPBLENDCAPS_INVDESTCOLOR
                              | D3DPBLENDCAPS_INVSRCALPHA
                              | D3DPBLENDCAPS_INVSRCCOLOR
                              | D3DPBLENDCAPS_ONE
                              | D3DPBLENDCAPS_SRCALPHA
                              | D3DPBLENDCAPS_SRCALPHASAT
                              | D3DPBLENDCAPS_SRCCOLOR
                              | D3DPBLENDCAPS_ZERO;

    prim.dwDestBlendCaps      = prim.dwSrcBlendCaps;

    prim.dwAlphaCmpCaps       = prim.dwZCmpCaps;

    prim.dwShadeCaps          = D3DPSHADECAPS_ALPHAFLATBLEND
                           // | D3DPSHADECAPS_ALPHAFLATSTIPPLED
                              | D3DPSHADECAPS_ALPHAGOURAUDBLEND
                           // | D3DPSHADECAPS_ALPHAGOURAUDSTIPPLED
                           // | D3DPSHADECAPS_ALPHAPHONGBLEND
                           // | D3DPSHADECAPS_ALPHAPHONGSTIPPLED
                           // | D3DPSHADECAPS_COLORFLATMONO
                              | D3DPSHADECAPS_COLORFLATRGB
                           // | D3DPSHADECAPS_COLORGOURAUDMONO
                              | D3DPSHADECAPS_COLORGOURAUDRGB
                           // | D3DPSHADECAPS_COLORPHONGMONO
                           // | D3DPSHADECAPS_COLORPHONGRGB
                              | D3DPSHADECAPS_FOGFLAT
                              | D3DPSHADECAPS_FOGGOURAUD
                           // | D3DPSHADECAPS_FOGPHONG
                           // | D3DPSHADECAPS_SPECULARFLATMONO
                              | D3DPSHADECAPS_SPECULARFLATRGB
                           // | D3DPSHADECAPS_SPECULARGOURAUDMONO
                              | D3DPSHADECAPS_SPECULARGOURAUDRGB;
                           // | D3DPSHADECAPS_SPECULARPHONGMONO
                           // | D3DPSHADECAPS_SPECULARPHONGRGB;

    prim.dwTextureCaps        = D3DPTEXTURECAPS_ALPHA
                              | D3DPTEXTURECAPS_ALPHAPALETTE
                              | D3DPTEXTURECAPS_BORDER
                           // | D3DPTEXTURECAPS_COLORKEYBLEND
                              | D3DPTEXTURECAPS_CUBEMAP
                              | D3DPTEXTURECAPS_PERSPECTIVE
                              | D3DPTEXTURECAPS_PROJECTED
                              | D3DPTEXTURECAPS_POW2 // Always reported on early D3D cards
                              | D3DPTEXTURECAPS_NONPOW2CONDITIONAL // Reported only on later (D3D7/8) D3D cards
                           // | D3DPTEXTURECAPS_SQUAREONLY
                              | D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE
                              | D3DPTEXTURECAPS_TRANSPARENCY;

    prim.dwTextureFilterCaps  = D3DPTFILTERCAPS_LINEAR
                              | D3DPTFILTERCAPS_LINEARMIPLINEAR
                              | D3DPTFILTERCAPS_LINEARMIPNEAREST
                              | D3DPTFILTERCAPS_MIPLINEAR
                              | D3DPTFILTERCAPS_MIPNEAREST
                              | D3DPTFILTERCAPS_NEAREST
                           // | D3DPTFILTERCAPS_MAGFAFLATCUBIC
                              | D3DPTFILTERCAPS_MAGFANISOTROPIC
                           // | D3DPTFILTERCAPS_MAGFGAUSSIANCUBIC
                              | D3DPTFILTERCAPS_MAGFLINEAR
                              | D3DPTFILTERCAPS_MAGFPOINT
                              | D3DPTFILTERCAPS_MINFANISOTROPIC
                              | D3DPTFILTERCAPS_MINFLINEAR
                              | D3DPTFILTERCAPS_MINFPOINT
                              | D3DPTFILTERCAPS_MIPFLINEAR
                              | D3DPTFILTERCAPS_MIPFPOINT;

    // Allegedly a deprecated item, but some d3d7 games,
    // like Summoner, still expect it to contain legacy values
    prim.dwTextureBlendCaps   = D3DPTBLENDCAPS_ADD
                              | D3DPTBLENDCAPS_COPY
                              | D3DPTBLENDCAPS_DECAL
                              | D3DPTBLENDCAPS_DECALALPHA
                              | D3DPTBLENDCAPS_DECALMASK
                              | D3DPTBLENDCAPS_MODULATE
                              | D3DPTBLENDCAPS_MODULATEALPHA
                              | D3DPTBLENDCAPS_MODULATEMASK;

    prim.dwTextureAddressCaps = D3DPTADDRESSCAPS_BORDER
                              | D3DPTADDRESSCAPS_CLAMP
                              | D3DPTADDRESSCAPS_INDEPENDENTUV
                              | D3DPTADDRESSCAPS_MIRROR
                              | D3DPTADDRESSCAPS_WRAP;

    prim.dwStippleWidth       = 0;
    prim.dwStippleHeight      = 0;

    desc7.dpcLineCaps         = prim;
    desc7.dpcTriCaps          = prim;

    desc7.dwDeviceRenderBitDepth  = DDBD_16 | DDBD_24 | DDBD_32;
    desc7.dwDeviceZBufferBitDepth = options->supportD16 ? DDBD_16 | DDBD_24 : DDBD_24;
    desc7.dwMinTextureWidth       = 1;
    desc7.dwMinTextureHeight      = 1;
    desc7.dwMaxTextureWidth       = ddrawCaps::MaxTextureDimension;
    desc7.dwMaxTextureHeight      = ddrawCaps::MaxTextureDimension;
    desc7.dwMaxTextureRepeat      = 8192;
    desc7.dwMaxTextureAspectRatio = 8192;
    desc7.dwMaxAnisotropy         = 16;
    desc7.dvGuardBandLeft         = -32768.0f;
    desc7.dvGuardBandTop          = -32768.0f;
    desc7.dvGuardBandRight        = 32768.0f;
    desc7.dvGuardBandBottom       = 32768.0f;
    desc7.dvExtentsAdjust         = 0.0f;

    desc7.dwStencilCaps        = D3DSTENCILCAPS_DECR
                               | D3DSTENCILCAPS_DECRSAT
                               | D3DSTENCILCAPS_INCR
                               | D3DSTENCILCAPS_INCRSAT
                               | D3DSTENCILCAPS_INVERT
                               | D3DSTENCILCAPS_KEEP
                               | D3DSTENCILCAPS_REPLACE
                               | D3DSTENCILCAPS_ZERO;

    desc7.dwFVFCaps                = (ddrawCaps::MaxSimultaneousTextures & D3DFVFCAPS_TEXCOORDCOUNTMASK);
                                // | D3DFVFCAPS_DONOTSTRIPELEMENTS;

    desc7.dwTextureOpCaps          = D3DTEXOPCAPS_ADD
                                   | D3DTEXOPCAPS_ADDSIGNED
                                   | D3DTEXOPCAPS_ADDSIGNED2X
                                   | D3DTEXOPCAPS_ADDSMOOTH
                                   | D3DTEXOPCAPS_BLENDCURRENTALPHA
                                   | D3DTEXOPCAPS_BLENDDIFFUSEALPHA
                                   | D3DTEXOPCAPS_BLENDFACTORALPHA
                                   | D3DTEXOPCAPS_BLENDTEXTUREALPHA
                                   | D3DTEXOPCAPS_BLENDTEXTUREALPHAPM
                                   | D3DTEXOPCAPS_BUMPENVMAP
                                   | D3DTEXOPCAPS_BUMPENVMAPLUMINANCE
                                   | D3DTEXOPCAPS_DISABLE
                                   | D3DTEXOPCAPS_DOTPRODUCT3
                                   | D3DTEXOPCAPS_MODULATE
                                   | D3DTEXOPCAPS_MODULATE2X
                                   | D3DTEXOPCAPS_MODULATE4X
                                   | D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR
                                   | D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA
                                   | D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR
                                   | D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA
                                   | D3DTEXOPCAPS_PREMODULATE
                                   | D3DTEXOPCAPS_SELECTARG1
                                   | D3DTEXOPCAPS_SELECTARG2
                                   | D3DTEXOPCAPS_SUBTRACT;

    desc7.wMaxTextureBlendStages   = ddrawCaps::MaxTextureBlendStages;
    desc7.wMaxSimultaneousTextures = ddrawCaps::MaxSimultaneousTextures;
    desc7.dwMaxActiveLights        = ddrawCaps::MaxEnabledLights;
    desc7.dvMaxVertexW             = 1e10f;

    desc7.deviceGUID               = rclsid;

    desc7.wMaxUserClipPlanes       = ddrawCaps::MaxClipPlanes;
    desc7.wMaxVertexBlendMatrices  = 4;

    desc7.dwVertexProcessingCaps   = D3DVTXPCAPS_TEXGEN
                                   | D3DVTXPCAPS_MATERIALSOURCE7
                                   | D3DVTXPCAPS_VERTEXFOG
                                   | D3DVTXPCAPS_DIRECTIONALLIGHTS
                                   | D3DVTXPCAPS_POSITIONALLIGHTS
                                   | D3DVTXPCAPS_LOCALVIEWER;

    desc7.dwReserved1              = 0;
    desc7.dwReserved2              = 0;
    desc7.dwReserved3              = 0;
    desc7.dwReserved4              = 0;

    return desc7;
  }

  inline Matrix4 MatrixD3DTo4(const D3DMATRIX* m) {
    Matrix4 r;

    r.data[0] = Vector4(m->_11, m->_12, m->_13, m->_14);
    r.data[1] = Vector4(m->_21, m->_22, m->_23, m->_24);
    r.data[2] = Vector4(m->_31, m->_32, m->_33, m->_34);
    r.data[3] = Vector4(m->_41, m->_42, m->_43, m->_44);

    return r;
  }

  inline D3DMATRIX Matrix4ToD3D(const Matrix4* m) {
    D3DMATRIX r;

    r._11 = m->data[0][0]; r._12 = m->data[0][1]; r._13 = m->data[0][2]; r._14 = m->data[0][3];
    r._21 = m->data[1][0]; r._22 = m->data[1][1]; r._23 = m->data[1][2]; r._24 = m->data[1][3];
    r._31 = m->data[2][0]; r._32 = m->data[2][1]; r._33 = m->data[2][2]; r._34 = m->data[2][3];
    r._41 = m->data[3][0]; r._42 = m->data[3][1]; r._43 = m->data[3][2]; r._44 = m->data[3][3];

    return r;
  }

}