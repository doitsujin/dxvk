#pragma once

// Implements IDirect3DVertexBuffer8 and IDirect3DIndexBuffer8

#include "d3d8_include.h"
#include "d3d8_resource.h"

namespace dxvk {

  template <typename D3D9, typename D3D8>
  class D3D8Buffer : public D3D8Resource<D3D9, D3D8> {

  public:

    D3D8Buffer(
            D3D8DeviceEx*   pDevice,
            Com<D3D9>&&     pBuffer)
      : D3D8Resource<D3D9, D3D8> ( pDevice, std::move(pBuffer) ) {
    }

    HRESULT STDMETHODCALLTYPE Lock(
            UINT   OffsetToLock,
            UINT   SizeToLock,
            BYTE** ppbData,
            DWORD  Flags) {
      return this->GetD3D9()->Lock(
        OffsetToLock,
        SizeToLock,
        reinterpret_cast<void**>(ppbData),
        Flags);
    }

    HRESULT STDMETHODCALLTYPE Unlock() {
      return this->GetD3D9()->Unlock();
    }

    void STDMETHODCALLTYPE PreLoad() {
      this->GetD3D9()->PreLoad();
    }

  };


  using D3D8VertexBufferBase = D3D8Buffer<d3d9::IDirect3DVertexBuffer9, IDirect3DVertexBuffer8>;
  class D3D8VertexBuffer final : public D3D8VertexBufferBase {

  public:

    D3D8VertexBuffer(
        D3D8DeviceEx*                       pDevice,
        Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer)
      : D3D8VertexBufferBase(pDevice, std::move(pBuffer)) {
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final { return D3DRTYPE_VERTEXBUFFER; }

    HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC* pDesc) final {
      // TODO: Remove reinterpret cast.
      return GetD3D9()->GetDesc(reinterpret_cast<d3d9::D3DVERTEXBUFFER_DESC*>(pDesc));
    }

  };

  using D3D8IndexBufferBase = D3D8Buffer<d3d9::IDirect3DIndexBuffer9, IDirect3DIndexBuffer8>;
  class D3D8IndexBuffer final : public D3D8IndexBufferBase {

  public:

    D3D8IndexBuffer(
        D3D8DeviceEx*                      pDevice,
        Com<d3d9::IDirect3DIndexBuffer9>&& pBuffer)
      : D3D8IndexBufferBase(pDevice, std::move(pBuffer)) {
    }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final { return D3DRTYPE_INDEXBUFFER; }

    HRESULT STDMETHODCALLTYPE GetDesc(D3DINDEXBUFFER_DESC* pDesc) final {
      // TODO: Remove reinterpret_cast.
      return GetD3D9()->GetDesc(reinterpret_cast<d3d9::D3DINDEXBUFFER_DESC*>(pDesc));
    }

  };
}