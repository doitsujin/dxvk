#include "d3d9_sampler.h"

namespace dxvk {

  size_t D3D9SamplerKeyHash::operator () (const D3D9SamplerKey& key) const {
    DxvkHashState state;

    std::hash<DWORD>                dhash;
    std::hash<D3DTEXTUREADDRESS>    tahash;
    std::hash<D3DTEXTUREFILTERTYPE> tfhash;
    std::hash<float>                fhash;
    std::hash<bool>                 bhash;

    state.add(tahash(key.AddressU));
    state.add(tahash(key.AddressV));
    state.add(tahash(key.AddressW));
    state.add(tfhash(key.MagFilter));
    state.add(tfhash(key.MinFilter));
    state.add(tfhash(key.MipFilter));
    state.add(dhash (key.MaxAnisotropy));
    state.add(fhash (key.MipmapLodBias));
    state.add(dhash (key.MaxMipLevel));
    state.add(dhash (key.BorderColor));
    state.add(bhash (key.Depth));

    return state;
  }


  bool D3D9SamplerKeyEq::operator () (const D3D9SamplerKey& a, const D3D9SamplerKey& b) const {
    return a.AddressU       == b.AddressU
        && a.AddressV       == b.AddressV
        && a.AddressW       == b.AddressW
        && a.MagFilter      == b.MagFilter
        && a.MinFilter      == b.MinFilter
        && a.MipFilter      == b.MipFilter
        && a.MaxAnisotropy  == b.MaxAnisotropy
        && a.MipmapLodBias  == b.MipmapLodBias
        && a.MaxMipLevel    == b.MaxMipLevel
        && a.BorderColor    == b.BorderColor
        && a.Depth          == b.Depth;
  }

}