#pragma once

#include "d3d8_include.h"
#include "d3d8_options.h"

namespace dxvk {

  struct D3D9VertexShaderCode {
    d3d9::D3DVERTEXELEMENT9 declaration[MAXD3DDECLLENGTH + 1];
    std::vector<DWORD> function;
  };

  struct D3D8VertexShaderInfo {
    Com<d3d9::IDirect3DVertexDeclaration9>  pVertexDecl;
    Com<d3d9::IDirect3DVertexShader9>       pVertexShader;
    std::vector<DWORD>                      declaration;
    std::vector<DWORD>                      function;
  };

  HRESULT TranslateVertexShader8(
    const DWORD*          pDeclaration,
    const DWORD*          pFunction,
    const D3D8Options&    overrides,
    D3D9VertexShaderCode& pTranslatedVS);

}