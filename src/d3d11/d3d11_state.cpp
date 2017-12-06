#include "d3d11_state.h"

namespace dxvk {
  
  size_t D3D11StateDescHash::operator () (const D3D11_RASTERIZER_DESC& desc) const {
    DxvkHashState hash;
    hash.add(desc.FillMode);
    hash.add(desc.CullMode);
    hash.add(desc.FrontCounterClockwise);
    hash.add(desc.DepthBias);
    hash.add(desc.SlopeScaledDepthBias);
    hash.add(desc.DepthBiasClamp);
    hash.add(desc.DepthClipEnable);
    hash.add(desc.ScissorEnable);
    hash.add(desc.MultisampleEnable);
    hash.add(desc.AntialiasedLineEnable);
    return hash;
  }
  
  
  bool D3D11StateDescEqual::operator () (const D3D11_RASTERIZER_DESC& a, const D3D11_RASTERIZER_DESC& b) const {
    return a.FillMode              == b.FillMode
        && a.CullMode              == b.CullMode
        && a.FrontCounterClockwise == b.FrontCounterClockwise
        && a.DepthBias             == b.DepthBias
        && a.SlopeScaledDepthBias  == b.SlopeScaledDepthBias
        && a.DepthBiasClamp        == b.DepthBiasClamp
        && a.DepthClipEnable       == b.DepthClipEnable
        && a.ScissorEnable         == b.ScissorEnable
        && a.MultisampleEnable     == b.MultisampleEnable
        && a.AntialiasedLineEnable == b.AntialiasedLineEnable;
  }
  
}