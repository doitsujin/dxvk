#include "d3d9_vertex_declaration.h"

namespace dxvk {

  Direct3DVertexDeclaration9::Direct3DVertexDeclaration9(
          Direct3DDevice9Ex* pDevice,
    const D3DVERTEXELEMENT9* pVertexElements,
          uint32_t           DeclCount)
    : Direct3DVertexDeclaration9Base{ pDevice }
    , m_elements{ DeclCount } {
    std::memcpy(m_elements.data(), pVertexElements, sizeof(D3DVERTEXELEMENT9) * DeclCount);
  }

  HRESULT STDMETHODCALLTYPE Direct3DVertexDeclaration9::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DVertexDeclaration9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3DVertexDeclaration9::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DVertexDeclaration9::GetDeclaration(
          D3DVERTEXELEMENT9* pElement,
          UINT*              pNumElements) {
    if (pNumElements == nullptr)
      return D3DERR_INVALIDCALL;

    if (pElement == nullptr) {
      *pNumElements = m_elements.size();
      return D3D_OK;
    }

    const UINT count = std::min(*pNumElements, m_elements.size());
    std::memcpy(pElement, m_elements.data(), sizeof(D3DVERTEXELEMENT9) * count);

    return D3D_OK;
  }

  const D3DVERTEXELEMENT9* Direct3DVertexDeclaration9::GetElement(uint32_t index) {
    if (index >= m_elements.size())
      return nullptr;

    return &m_elements[index];
  }

}