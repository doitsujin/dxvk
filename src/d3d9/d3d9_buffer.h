#pragma once

#include "d3d9_resource.h"

#include "d3d9_common_buffer.h"

namespace dxvk {

  template <typename... Type>
  class Direct3DBuffer9 : public Direct3DResource9<Type...> {

  public:

    Direct3DBuffer9(
            Direct3DDevice9Ex* pDevice,
      const D3D9_BUFFER_DESC*  pDesc)
      : Direct3DResource9<Type...>{ pDevice }
      , m_buffer{ new Direct3DCommonBuffer9{ pDevice, pDesc } } {}

    HRESULT STDMETHODCALLTYPE Lock(
      UINT OffsetToLock,
      UINT SizeToLock,
      void** ppbData,
      DWORD Flags) final {
      return m_buffer->Lock(
        OffsetToLock,
        SizeToLock,
        ppbData,
        Flags);
    }

    HRESULT STDMETHODCALLTYPE Unlock() final {
      return m_buffer->Unlock();
    }

  protected:

    Rc<Direct3DCommonBuffer9> m_buffer;

  };


  using Direct3DVertexBuffer9Base = Direct3DBuffer9<IDirect3DVertexBuffer9>;
  class Direct3DVertexBuffer9 final : public Direct3DVertexBuffer9Base {

  public:

    Direct3DVertexBuffer9(
            Direct3DDevice9Ex* pDevice,
      const D3D9_BUFFER_DESC*  pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject) final;

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final;

    HRESULT STDMETHODCALLTYPE GetDesc(
            D3DVERTEXBUFFER_DESC* pDesc) final;

  };

  using Direct3DIndexBuffer9Base = Direct3DBuffer9<IDirect3DIndexBuffer9>;
  class Direct3DIndexBuffer9 final : public Direct3DIndexBuffer9Base {

  public:

    Direct3DIndexBuffer9(
            Direct3DDevice9Ex* pDevice,
      const D3D9_BUFFER_DESC*  pDesc);

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void** ppvObject) final;

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final;

    HRESULT STDMETHODCALLTYPE GetDesc(
            D3DINDEXBUFFER_DESC* pDesc) final;

  };

}