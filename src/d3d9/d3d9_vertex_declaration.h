#pragma once

#include "d3d9_device_child.h"
#include "d3d9_util.h"

#include <vector>

namespace dxvk {

  enum class D3D9VertexDeclFlag {
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

    UINT GetSize(UINT Stream) const {
      return m_sizes[Stream];
    }

    bool TestFlag(D3D9VertexDeclFlag flag) const {
      return m_flags.test(flag);
    }

    D3D9VertexDeclFlags GetFlags() const {
      return m_flags;
    }

    uint32_t GetTexcoordMask() const {
      return m_texcoordMask;
    }

    uint32_t GetStreamMask() const {
      return m_streamMask;
    }

  private:

    bool MapD3DDeclToFvf(
      const D3DVERTEXELEMENT9& element,
            DWORD fvf,
            DWORD& outFvf,
            DWORD& texCountPostUpdate);

    DWORD MapD3D9VertexElementsToFvf();

    DWORD MapD3DDeclTypeFloatToFvfXYZBn(BYTE type);

    bool MapD3DDeclUsageTexCoordToFvfTexCoordSize(
      const D3DVERTEXELEMENT9& element,
            DWORD fvf,
            DWORD& outFvf,
            DWORD& texCountPostUpdate);

    void Classify();

    D3D9VertexDeclFlags            m_flags;

    D3D9VertexElements             m_elements;

    DWORD                          m_fvf;

    uint32_t                       m_texcoordMask = 0;

    uint32_t                       m_streamMask = 0;

    std::array<uint32_t, caps::MaxStreams> m_sizes = {};

  };

}