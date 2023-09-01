#pragma once

#include "d3d11_on_12_interfaces.h"

#include "../util/log/log.h"

namespace dxvk {

  class D3D11Device;
  class D3D11DXGIDevice;

  /**
   * \brief Resource info for 11on12 resources
   */
  struct D3D11_ON_12_RESOURCE_INFO {
    Com<ID3D12Resource> Resource;
    UINT64 VulkanHandle = 0;
    UINT64 VulkanOffset = 0;
    BOOL IsWrappedResource = FALSE;
    D3D12_RESOURCE_STATES InputState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES OutputState = D3D12_RESOURCE_STATE_COMMON;
  };


  class D3D11on12Device : public ID3D11On12Device {

  public:

    D3D11on12Device(
            D3D11DXGIDevice*        pContainer,
            D3D11Device*            pDevice,
            ID3D12Device*           pD3D12Device,
            ID3D12CommandQueue*     pD3D12Queue);

    ~D3D11on12Device();

    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    HRESULT STDMETHODCALLTYPE CreateWrappedResource(
            IUnknown*               pResource12,
      const D3D11_RESOURCE_FLAGS*   pResourceFlags,
            D3D12_RESOURCE_STATES   InputState,
            D3D12_RESOURCE_STATES   OutputState,
            REFIID                  riid,
            void**                  ppResource11);

    void STDMETHODCALLTYPE ReleaseWrappedResources(
            ID3D11Resource* const*  ppResources,
            UINT                    ResourceCount);

    void STDMETHODCALLTYPE AcquireWrappedResources(
            ID3D11Resource* const*  ppResources,
            UINT                    ResourceCount);

    bool Is11on12Device() const {
      return m_d3d12Device != nullptr;
    }

  private:

    D3D11DXGIDevice*        m_container;
    D3D11Device*            m_device;

    Com<ID3D12Device>       m_d3d12Device;
    Com<ID3D12CommandQueue> m_d3d12Queue;

  };

}
