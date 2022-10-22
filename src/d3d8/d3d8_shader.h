#pragma once

#include "d3d8_include.h"

namespace dxvk {

    struct D3D9VertexShaderCode {
      d3d9::D3DVERTEXELEMENT9 declaration[MAXD3DDECLLENGTH + 1];
      std::vector<DWORD> function;
    };

    D3D9VertexShaderCode translateVertexShader8(const DWORD* pDeclaration, const DWORD* pFunction);

}