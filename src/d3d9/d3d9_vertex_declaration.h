#pragma once

#include "d3d9_device_child.h"

#include <vector>

namespace dxvk {

  using Direct3DVertexDeclaration9Base = Direct3DDeviceChild9<IDirect3DVertexDeclaration9>;
  class Direct3DVertexDeclaration9 final : public Direct3DVertexDeclaration9Base {

  public:

    Direct3DVertexDeclaration9(
            Direct3DDevice9Ex* pDevice,
      const D3DVERTEXELEMENT9* pVertexElements,
            uint32_t           DeclCount);

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetDeclaration(
            D3DVERTEXELEMENT9* pElement,
            UINT*              pNumElements);

    const std::vector<D3DVERTEXELEMENT9>& GetElements() {
      return m_elements;
    }

  private:

    std::vector<D3DVERTEXELEMENT9> m_elements;

  };

}