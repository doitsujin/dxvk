#pragma once

#include "d3d8_include.h"
#include "d3d8_resource.h"

namespace dxvk {

  template <typename Base, typename Buffer>
  class D3D8Buffer : public D3D8Resource<Base> {

  public:

    D3D8Buffer(
            D3D8DeviceEx*   pDevice,
            Buffer*         pBuffer)
      : D3D8Resource<Base>  ( pDevice )
      , m_buffer(pBuffer) { }

    HRESULT STDMETHODCALLTYPE Lock(
            UINT   OffsetToLock,
            UINT   SizeToLock,
            BYTE** ppbData,
            DWORD  Flags) {
      return m_buffer->Lock(
        OffsetToLock,
        SizeToLock,
        (void**)ppbData,
        Flags);
    }

    HRESULT STDMETHODCALLTYPE Unlock() {
      return m_buffer->Unlock();
    }

    void STDMETHODCALLTYPE PreLoad() {
      m_buffer->PreLoad();
    }

    // allow conversion to wrapped type
    inline operator Buffer* () const { return m_buffer; }

  protected:
    Buffer* m_buffer;

  };


  using D3D8VertexBufferBase = D3D8Buffer<IDirect3DVertexBuffer8, d3d9::IDirect3DVertexBuffer9>;
  class D3D8VertexBuffer final : public D3D8VertexBufferBase {

  public:

    D3D8VertexBuffer(
        D3D8DeviceEx*                 pDevice,
        d3d9::IDirect3DVertexBuffer9* pBuffer) : D3D8VertexBufferBase(pDevice, pBuffer) { }

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject) final { return D3DERR_INVALIDCALL; }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final { return D3DRTYPE_VERTEXBUFFER; }

    HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC* pDesc) final {
      return m_buffer->GetDesc((d3d9::D3DVERTEXBUFFER_DESC*)pDesc);
    }

  };

  using D3D8IndexBufferBase = D3D8Buffer<IDirect3DIndexBuffer8, d3d9::IDirect3DIndexBuffer9>;
  class D3D8IndexBuffer final : public D3D8IndexBufferBase {

  public:

    D3D8IndexBuffer(
        D3D8DeviceEx*                 pDevice,
        d3d9::IDirect3DIndexBuffer9*  pBuffer) : D3D8IndexBufferBase(pDevice, pBuffer) { }

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject) final { return D3DERR_INVALIDCALL; }

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final { return D3DRTYPE_INDEXBUFFER; }

    HRESULT STDMETHODCALLTYPE GetDesc(D3DINDEXBUFFER_DESC* pDesc) final {
      return m_buffer->GetDesc((d3d9::D3DINDEXBUFFER_DESC*)pDesc);
    }

  };
}