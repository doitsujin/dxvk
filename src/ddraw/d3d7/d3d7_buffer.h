#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"

#include "../ddraw_common_interface.h"

#include "d3d7_interface.h"
#include "d3d7_device.h"

namespace dxvk {

  class D3D7VertexBuffer final : public DDrawChildObject<D3D7Interface, IDirect3DVertexBuffer7> {

  public:

    D3D7VertexBuffer(
          D3D7Interface* pParent,
          D3DVERTEXBUFFERDESC* pDesc);

    ~D3D7VertexBuffer();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc);

    HRESULT STDMETHODCALLTYPE Lock(DWORD flags, void **data, DWORD *data_size);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER7 lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE ProcessVerticesStrided(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwSrcIndex, LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE Optimize(LPDIRECT3DDEVICE7 lpD3DDevice, DWORD dwFlags);

    HRESULT InitializeD3D9();

    void RefreshD3DDevice();

    bool IsInitialized() const {
      return m_vb9 != nullptr;
    }

    d3d9::IDirect3DVertexBuffer9* GetD3D9VertexBuffer() const {
      return m_vb9.ptr();
    }

    DWORD GetFVF() const {
      return m_desc.dwFVF;
    }

    DWORD GetStride() const {
      return m_stride;
    }

    bool IsLocked() const {
      return m_locked;
    }

    D3D7Device* GetDevice() const {
      return m_d3d7Device;
    }

  private:

    inline bool IsOptimized() const {
      return m_desc.dwCaps & D3DVBCAPS_OPTIMIZED;
    }

    bool                              m_locked        = false;
    bool                              m_legacyDiscard = false;

    DDrawCommonInterface*             m_commonIntf    = nullptr;

    D3DVERTEXBUFFERDESC               m_desc;

    UINT                              m_stride        = 0;
    UINT                              m_size          = 0;

    D3D7Device*                       m_d3d7Device    = nullptr;

    Com<d3d9::IDirect3DVertexBuffer9> m_vb9;

  };

}
