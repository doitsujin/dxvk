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

  inline void NormalizeSamplerKey(D3D9SamplerKey& key) {
    key.AddressU = std::clamp(key.AddressU, D3DTADDRESS_WRAP, D3DTADDRESS_MIRRORONCE);
    key.AddressV = std::clamp(key.AddressV, D3DTADDRESS_WRAP, D3DTADDRESS_MIRRORONCE);
    key.AddressW = std::clamp(key.AddressW, D3DTADDRESS_WRAP, D3DTADDRESS_MIRRORONCE);

    key.MagFilter = std::clamp(key.MagFilter, D3DTEXF_NONE, D3DTEXF_ANISOTROPIC);
    key.MinFilter = std::clamp(key.MinFilter, D3DTEXF_NONE, D3DTEXF_ANISOTROPIC);
    key.MipFilter = std::clamp(key.MipFilter, D3DTEXF_NONE, D3DTEXF_ANISOTROPIC);

    key.MaxAnisotropy = std::clamp<DWORD>(key.MaxAnisotropy, 0, 16);

    if ( key.AddressU != D3DTADDRESS_BORDER
      && key.AddressV != D3DTADDRESS_BORDER
      && key.AddressW != D3DTADDRESS_BORDER)
      key.BorderColor = 0;
  }

}