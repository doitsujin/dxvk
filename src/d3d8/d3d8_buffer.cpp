#include "d3d8_buffer.h"
#include "d3d8_device.h"

namespace dxvk {

  // D3D8VertexBuffer

  D3D8VertexBuffer::D3D8VertexBuffer(
          D3D8Device*                         pDevice,
          Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer,
          D3DPOOL                             Pool,
          DWORD                               Usage)
    : D3D8VertexBufferBase(pDevice, std::move(pBuffer), Pool, Usage) {
  }

  D3DRESOURCETYPE STDMETHODCALLTYPE D3D8VertexBuffer::GetType() { return D3DRTYPE_VERTEXBUFFER; }

  HRESULT STDMETHODCALLTYPE D3D8VertexBuffer::GetDesc(D3DVERTEXBUFFER_DESC* pDesc) {
    return GetD3D9()->GetDesc(reinterpret_cast<d3d9::D3DVERTEXBUFFER_DESC*>(pDesc));
  }

  // D3D8IndexBuffer

  D3D8IndexBuffer::D3D8IndexBuffer(
          D3D8Device*                        pDevice,
          Com<d3d9::IDirect3DIndexBuffer9>&& pBuffer,
          D3DPOOL                            Pool,
          DWORD                              Usage)
    : D3D8IndexBufferBase(pDevice, std::move(pBuffer), Pool, Usage) {
  }

  D3DRESOURCETYPE STDMETHODCALLTYPE D3D8IndexBuffer::GetType() { return D3DRTYPE_INDEXBUFFER; }

  HRESULT STDMETHODCALLTYPE D3D8IndexBuffer::GetDesc(D3DINDEXBUFFER_DESC* pDesc) {
    return GetD3D9()->GetDesc(reinterpret_cast<d3d9::D3DINDEXBUFFER_DESC*>(pDesc));
  }

}