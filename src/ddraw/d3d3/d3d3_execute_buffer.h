#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"

#include "d3d3_device.h"

#include <vector>

namespace dxvk {

  class D3D3ExecuteBuffer final : public DDrawChildObject<D3D3Device, IDirect3DExecuteBuffer> {

  public:

    D3D3ExecuteBuffer(D3D3Device* pParent, D3DEXECUTEBUFFERDESC* pDesc);

    ~D3D3ExecuteBuffer();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetExecuteData(LPD3DEXECUTEDATA lpData);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DEXECUTEBUFFERDESC lpDesc);

    HRESULT STDMETHODCALLTYPE Lock(LPD3DEXECUTEBUFFERDESC lpDesc);

    HRESULT STDMETHODCALLTYPE Optimize(DWORD dwUnknown);

    HRESULT STDMETHODCALLTYPE SetExecuteData(LPD3DEXECUTEDATA lpData);

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE Validate(LPDWORD lpdwOffset, LPD3DVALIDATECALLBACK lpFunc, LPVOID lpUserArg, DWORD dwReserved);

    bool IsLocked() const {
      return m_locked;
    }

    void SetExecutedState(bool executed) {
      m_executed = executed;
    }

    D3DEXECUTEDATA* GetExecuteDataInternal() {
      return &m_executeData;
    }

    std::vector<uint8_t> GetBuffer() const {
      return m_buffer;
    }

  private:

    bool                 m_locked     = false;
    bool                 m_executed   = false;

    D3DEXECUTEDATA       m_executeData;

    std::vector<uint8_t> m_buffer;

    uint32_t             m_buffCount  = 0;
    static std::atomic<uint32_t> s_buffCount;

  };

}
