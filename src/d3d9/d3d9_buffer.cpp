#include "d3d9_buffer.h"

namespace dxvk {

  ////////////////////////
  // Direct3DVertexBuffer9
  ////////////////////////

  Direct3DVertexBuffer9::Direct3DVertexBuffer9(
          Direct3DDevice9Ex* pDevice,
    const D3D9_BUFFER_DESC*  pDesc)
    : Direct3DVertexBuffer9Base { pDevice, pDesc } {}

  HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer9::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DResource9)
     || riid == __uuidof(IDirect3DVertexBuffer9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3DVertexBuffer9::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DVertexBuffer9::GetType() {
    return D3DRTYPE_VERTEXBUFFER;
  }

  HRESULT STDMETHODCALLTYPE Direct3DVertexBuffer9::GetDesc(
          D3DVERTEXBUFFER_DESC* pDesc) {
    if (pDesc == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9_BUFFER_DESC desc;
    m_buffer->GetDesc(&desc);

    pDesc->Format = static_cast<D3DFORMAT>(desc.Format);
    pDesc->Type   = desc.Type;
    pDesc->Usage  = desc.Usage;
    pDesc->Pool   = desc.Pool;
    pDesc->Size   = desc.Size;
    pDesc->FVF    = desc.FVF;

    return D3D_OK;
  }

  //////////////////////
  //Direct3DIndexBuffer9
  //////////////////////

  Direct3DIndexBuffer9::Direct3DIndexBuffer9(
          Direct3DDevice9Ex* pDevice,
    const D3D9_BUFFER_DESC*  pDesc)
    : Direct3DIndexBuffer9Base { pDevice, pDesc } {}

  HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer9::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DResource9)
     || riid == __uuidof(IDirect3DIndexBuffer9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3DIndexBuffer9::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DIndexBuffer9::GetType() {
    return D3DRTYPE_INDEXBUFFER;
  }

  HRESULT STDMETHODCALLTYPE Direct3DIndexBuffer9::GetDesc(
          D3DINDEXBUFFER_DESC* pDesc) {
    if (pDesc == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9_BUFFER_DESC desc;
    m_buffer->GetDesc(&desc);

    pDesc->Format = static_cast<D3DFORMAT>(desc.Format);
    pDesc->Type   = desc.Type;
    pDesc->Usage  = desc.Usage;
    pDesc->Pool   = desc.Pool;
    pDesc->Size   = desc.Size;

    return D3D_OK;
  }

}