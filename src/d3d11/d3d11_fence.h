#pragma once

#include "../dxvk/dxvk_fence.h"
#include "../dxvk/dxvk_gpu_query.h"

#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Fence : public D3D11DeviceChild<ID3D11Fence> {

  public:
    
    D3D11Fence(
            D3D11Device*        pDevice,
            UINT64              InitialValue,
            D3D11_FENCE_FLAG    Flags,
            HANDLE              hFence);

    ~D3D11Fence();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID              riid,
            void**              ppvObject);

    HRESULT STDMETHODCALLTYPE CreateSharedHandle(
      const SECURITY_ATTRIBUTES* pAttributes,
            DWORD               dwAccess,
            LPCWSTR             lpName,
            HANDLE*             pHandle);

    HRESULT STDMETHODCALLTYPE SetEventOnCompletion(
            UINT64              Value,
            HANDLE              hEvent);

    UINT64 STDMETHODCALLTYPE GetCompletedValue();

    Rc<DxvkFence> GetFence() const {
      return m_fence;
    }
    
  private:
    
    Rc<DxvkFence> m_fence;
    D3D11_FENCE_FLAG m_flags;

  };
  
}
