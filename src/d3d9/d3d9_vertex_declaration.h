#pragma once

#include "d3d9_device_child.h"
#include "d3d9_util.h"

#include <vector>

namespace dxvk {

  enum D3D9VertexDeclFlag {
    HasColor0,
    HasColor1,
    HasPositionT,
    HasPointSize,
    HasFog,
    HasBlendWeight,
    HasBlendIndices
  };
  using D3D9VertexDeclFlags = Flags<D3D9VertexDeclFlag>;

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

    const D3D9VertexElements& GetElements() const {
      return m_elements;
    }

    UINT GetSize() const {
      return m_size;
    }

    bool TestFlag(D3D9VertexDeclFlag flag) const {
      return m_flags.test(flag);
    }

    uint32_t GetTexcoordMask() const {
      return m_texcoordMask;
    }

  private:

    void Classify();

    D3D9VertexDeclFlags            m_flags;

    D3D9VertexElements             m_elements;

    DWORD                          m_fvf;

    uint32_t                       m_texcoordMask = 0;

    // The size of Stream 0. That's all we care about.
    uint32_t                       m_size = 0;

  };

}