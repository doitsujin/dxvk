#pragma once

#include "d3d9_include.h"

namespace dxvk {

  class DxvkShader;

  namespace D3D9ShaderPermutations {
    enum D3D9ShaderPermutation {
      None,
      FlatShade,
      Count
    };
  }
  using D3D9ShaderPermutation = D3D9ShaderPermutations::D3D9ShaderPermutation;

  using DxsoPermutations = std::array<Rc<DxvkShader>, D3D9ShaderPermutations::Count>;

}