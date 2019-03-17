#pragma once

#include "d3d9_include.h"

#include "../dxvk/dxvk_hash.h"

namespace dxvk {

  struct D3D9SamplerKey {
    D3DTEXTUREADDRESS AddressU;
    D3DTEXTUREADDRESS AddressV;
    D3DTEXTUREADDRESS AddressW;
    D3DTEXTUREFILTERTYPE MagFilter;
    D3DTEXTUREFILTERTYPE MinFilter;
    D3DTEXTUREFILTERTYPE MipFilter;
    DWORD MaxAnisotropy;
    float MipmapLodBias;
    DWORD MaxMipLevel;
    D3DCOLOR BorderColor;
  };

  struct D3D9SamplerKeyHash {
    size_t operator () (const D3D9SamplerKey& key) const;
  };

  struct D3D9SamplerKeyEq {
    bool operator () (const D3D9SamplerKey& a, const D3D9SamplerKey& b) const;
  };

}