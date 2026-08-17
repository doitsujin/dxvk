#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"

#include "../ddraw_common_interface.h"

#include "d3d6_interface.h"
#include "d3d6_device.h"

namespace dxvk {

  class D3D6VertexBuffer final : public DDrawChildObject<D3D6Interface, IDirect3DVertexBuffer> {

  public:

    D3D6VertexBuffer(
          D3D6Interface* pParent,
          DWORD creationFlags,
          D3DVERTEXBUFFERDESC* pDesc);

    ~D3D6VertexBuffer();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetVertexBufferDesc(LPD3DVERTEXBUFFERDESC lpVBDesc);

    HRESULT STDMETHODCALLTYPE Lock(DWORD dwFlags, LPVOID* lplpData, LPDWORD lpdwSize);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE ProcessVertices(DWORD dwVertexOp, DWORD dwDestIndex, DWORD dwCount, LPDIRECT3DVERTEXBUFFER lpSrcBuffer, DWORD dwSrcIndex, LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE Optimize(LPDIRECT3DDEVICE3 lpD3DDevice, DWORD dwFlags);

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

    DWORD GetNumVertices() const {
      return m_size / m_stride;
    }

    bool IsLocked() const {
      return m_locked;
    }

    D3D6Device* GetDevice() const {
      return m_d3d6Device;
    }

  private:

    inline bool IsOptimized() const {
      return m_desc.dwCaps & D3DVBCAPS_OPTIMIZED;
    }

    std::atomic<bool>                 m_locked        = false;

    DDrawCommonInterface*             m_commonIntf    = nullptr;

    DWORD                             m_creationFlags = 0;
    D3DVERTEXBUFFERDESC               m_desc;

    UINT                              m_stride        = 0;
    UINT                              m_size          = 0;

    D3D6Device*                       m_d3d6Device    = nullptr;

    Com<d3d9::IDirect3DVertexBuffer9> m_vb9;

  };

}
