#pragma once

#include <unordered_map>

#include "d3d9_include.h"

#include "../dxvk/dxvk_shader.h"

namespace dxvk {

  class D3D9VertexDecl;
  class D3D9DeviceEx;

  struct D3D9VertexDeclHash {
    size_t operator () (const D3D9VertexElements& key) const;
  };

  struct D3D9VertexDeclEq {
    bool operator () (const D3D9VertexElements& a, const D3D9VertexElements& b) const;
  };

  class D3D9SWVPEmulator {

  public:

    Rc<DxvkShader> GetShaderModule(D3D9DeviceEx* pDevice, const D3D9VertexDecl* pDecl);

  private:

    dxvk::mutex                               m_mutex;

    std::unordered_map<
      D3D9VertexElements, Rc<DxvkShader>,
      D3D9VertexDeclHash, D3D9VertexDeclEq>   m_modules;

  };

}
