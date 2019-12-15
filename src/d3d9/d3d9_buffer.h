#pragma once

#include "d3d9_resource.h"

#include "d3d9_common_buffer.h"

namespace dxvk {

  template <typename... Type>
  class D3D9Buffer : public D3D9Resource<Type...> {

  public:

    D3D9Buffer(
            D3D9DeviceEx*      pDevice,
      const D3D9_BUFFER_DESC*  pDesc)
      : D3D9Resource<Type...>  ( pDevice )
      , m_buffer               ( pDevice, pDesc ) { }

    HRESULT STDMETHODCALLTYPE Lock(
            UINT   OffsetToLock,
            UINT   SizeToLock,
            void** ppbData,
            DWORD  Flags) final {
      return m_buffer.Lock(
        OffsetToLock,
        SizeToLock,
        ppbData,
        Flags);
    }

    HRESULT STDMETHODCALLTYPE Unlock() final {
      return m_buffer.Unlock();
    }

    D3D9CommonBuffer* GetCommonBuffer() {
      return &m_buffer;
    }

  protected:

    D3D9CommonBuffer m_buffer;

  };


  using D3D9VertexBufferBase = D3D9Buffer<IDirect3DVertexBuffer9>;
  class D3D9VertexBuffer final : public D3D9VertexBufferBase {

  public:

    D3D9VertexBuffer(
            D3D9DeviceEx*      pDevice,
      const D3D9_BUFFER_DESC*  pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject) final;

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final;

    HRESULT STDMETHODCALLTYPE GetDesc(
            D3DVERTEXBUFFER_DESC* pDesc) final;

  };

  using D3D9IndexBufferBase = D3D9Buffer<IDirect3DIndexBuffer9>;
  class D3D9IndexBuffer final : public D3D9IndexBufferBase {

  public:

    D3D9IndexBuffer(
            D3D9DeviceEx*      pDevice,
      const D3D9_BUFFER_DESC*  pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject) final;

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final;

    HRESULT STDMETHODCALLTYPE GetDesc(
            D3DINDEXBUFFER_DESC* pDesc) final;

  };

  template <typename T>
  inline D3D9CommonBuffer* GetCommonBuffer(const T& pResource) {
    return pResource != nullptr ? pResource->GetCommonBuffer() : nullptr;
  }

}