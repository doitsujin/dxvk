#pragma once

#include "d3d8_include.h"
#include "d3d8_options.h"
#include "d3d8_resource.h"

namespace dxvk {

  template <typename D3D9, typename D3D8>
  class D3D8Buffer : public D3D8Resource<D3D9, D3D8> {

  public:

    D3D8Buffer(
            D3D8Device*     pDevice,
            Com<D3D9>&&     pBuffer,
            D3DPOOL         Pool,
            DWORD           Usage)
      : D3D8Resource<D3D9, D3D8> (pDevice, Pool, std::move(pBuffer))
      , m_usage                  (Usage) {
      m_options = this->GetParent()->GetOptions();
    }

    HRESULT STDMETHODCALLTYPE Lock(
            UINT   OffsetToLock,
            UINT   SizeToLock,
            BYTE** ppbData,
            DWORD  Flags) {

      if (m_options->forceLegacyDiscard &&
          (Flags & D3DLOCK_DISCARD) &&
         !((m_usage & D3DUSAGE_DYNAMIC) &&
           (m_usage & D3DUSAGE_WRITEONLY)))
          Flags &= ~D3DLOCK_DISCARD;

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

  protected:

    const D3D8Options* m_options;
    const DWORD        m_usage;

  };


  using D3D8VertexBufferBase = D3D8Buffer<d3d9::IDirect3DVertexBuffer9, IDirect3DVertexBuffer8>;
  class D3D8VertexBuffer : public D3D8VertexBufferBase {

  public:

    D3D8VertexBuffer(
            D3D8Device*                         pDevice,
            Com<d3d9::IDirect3DVertexBuffer9>&& pBuffer,
            D3DPOOL                             Pool,
            DWORD                               Usage);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final;

    HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC* pDesc);

  };

  using D3D8IndexBufferBase = D3D8Buffer<d3d9::IDirect3DIndexBuffer9, IDirect3DIndexBuffer8>;
  class D3D8IndexBuffer final : public D3D8IndexBufferBase {

  public:

    D3D8IndexBuffer(
            D3D8Device*                        pDevice,
            Com<d3d9::IDirect3DIndexBuffer9>&& pBuffer,
            D3DPOOL                            Pool,
            DWORD                              Usage);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final;

    HRESULT STDMETHODCALLTYPE GetDesc(D3DINDEXBUFFER_DESC* pDesc) final;

  };

}