#pragma once

#include <unordered_map>

#include "d3d9_include.h"

#include "../dxvk/dxvk_shader.h"

namespace dxvk {

  class D3D9VertexDecl;
  class D3D9DeviceEx;

  struct D3D9CompactVertexElement {
      uint8_t Stream : 4;
      uint8_t Type : 5;
      uint8_t Method : 3;
      uint8_t Usage : 4;
      uint8_t UsageIndex;
      uint16_t Offset;

      D3D9CompactVertexElement(const D3DVERTEXELEMENT9& element) 
        : Stream(element.Stream), Type(element.Type), Method(element.Method),
          Usage(element.Usage), UsageIndex(element.UsageIndex), Offset(element.Offset) {}
  };

  using D3D9CompactVertexElements = small_vector<D3D9CompactVertexElement, 4>;

  struct D3D9VertexDeclHash {
    size_t operator () (const D3D9CompactVertexElements& key) const;
  };

  struct D3D9VertexDeclEq {
    bool operator () (const D3D9CompactVertexElements& a, const D3D9CompactVertexElements& b) const;
  };

  class D3D9SWVPEmulator {

  public:

    Rc<DxvkShader> GetShaderModule(D3D9DeviceEx* pDevice,  D3D9CompactVertexElements&& elements);

  private:

    dxvk::mutex                               m_mutex;

    std::unordered_map<
      D3D9CompactVertexElements, Rc<DxvkShader>,
      D3D9VertexDeclHash, D3D9VertexDeclEq>   m_modules;

  };

}
