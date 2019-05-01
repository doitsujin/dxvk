#pragma once

#include "d3d9_device_child.h"

#include <vector>

namespace dxvk {

  using D3D9VertexDeclBase = D3D9DeviceChild<IDirect3DVertexDeclaration9>;
  class D3D9VertexDecl final : public D3D9VertexDeclBase {

  public:

    D3D9VertexDecl(
            D3D9DeviceEx*      pDevice,
            DWORD              FVF);

    D3D9VertexDecl(
            D3D9DeviceEx*      pDevice,
      const D3DVERTEXELEMENT9* pVertexElements,
            uint32_t           DeclCount);

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetDeclaration(
            D3DVERTEXELEMENT9* pElement,
            UINT*              pNumElements);

    inline DWORD GetFVF() {
      return m_fvf;
    }

    void SetFVF(DWORD FVF);

    const std::vector<D3DVERTEXELEMENT9>& GetElements() {
      return m_elements;
    }

  private:

    std::vector<D3DVERTEXELEMENT9> m_elements;

    DWORD                          m_fvf;

  };

}