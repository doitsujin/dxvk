#include "d3d9_buffer.h"

namespace dxvk {

  ////////////////////////
  // D3D9VertexBuffer
  ////////////////////////

  D3D9VertexBuffer::D3D9VertexBuffer(
          D3D9DeviceEx*      pDevice,
    const D3D9_BUFFER_DESC*  pDesc)
  : D3D9VertexBufferBase(pDevice, pDesc) {

  }


  HRESULT STDMETHODCALLTYPE D3D9VertexBuffer::QueryInterface(
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

    Logger::warn("D3D9VertexBuffer::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  D3DRESOURCETYPE STDMETHODCALLTYPE D3D9VertexBuffer::GetType() {
    return D3DRTYPE_VERTEXBUFFER;
  }


  HRESULT STDMETHODCALLTYPE D3D9VertexBuffer::GetDesc(
          D3DVERTEXBUFFER_DESC* pDesc) {
    if (pDesc == nullptr)
      return D3DERR_INVALIDCALL;

    const D3D9_BUFFER_DESC* desc = m_buffer.Desc();

    pDesc->Format = static_cast<D3DFORMAT>(desc->Format);
    pDesc->Type   = desc->Type;
    pDesc->Usage  = desc->Usage;
    pDesc->Pool   = desc->Pool;
    pDesc->Size   = desc->Size;
    pDesc->FVF    = desc->FVF;

    return D3D_OK;
  }


  //////////////////////
  // D3D9IndexBuffer
  //////////////////////


  D3D9IndexBuffer::D3D9IndexBuffer(
          D3D9DeviceEx*      pDevice,
    const D3D9_BUFFER_DESC*  pDesc)
  : D3D9IndexBufferBase(pDevice, pDesc) {

  }


  HRESULT STDMETHODCALLTYPE D3D9IndexBuffer::QueryInterface(
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

    Logger::warn("D3D9IndexBuffer::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  D3DRESOURCETYPE STDMETHODCALLTYPE D3D9IndexBuffer::GetType() {
    return D3DRTYPE_INDEXBUFFER;
  }


  HRESULT STDMETHODCALLTYPE D3D9IndexBuffer::GetDesc(
          D3DINDEXBUFFER_DESC* pDesc) {
    if (pDesc == nullptr)
      return D3DERR_INVALIDCALL;

    const D3D9_BUFFER_DESC* desc = m_buffer.Desc();

    pDesc->Format = static_cast<D3DFORMAT>(desc->Format);
    pDesc->Type   = desc->Type;
    pDesc->Usage  = desc->Usage;
    pDesc->Pool   = desc->Pool;
    pDesc->Size   = desc->Size;

    return D3D_OK;
  }

}