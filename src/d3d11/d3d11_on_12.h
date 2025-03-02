#pragma once

#include "d3d11_on_12_interfaces.h"

#include "../util/log/log.h"

/**
 * \brief Declaration of the ID3D11On12Device1 interface
 *
 * Various different headers that we need to be compatible with
 * can't seem to agree on the signature of GetD3D12Device, and
 * older wine/mingw headers don't support this interface at all.
 */
MIDL_INTERFACE("bdb64df4-ea2f-4c70-b861-aaab1258bb5d")
ID3D11On12Device1_DXVK : public ID3D11On12Device {
  virtual HRESULT STDMETHODCALLTYPE GetD3D12Device(
          REFIID                  riid,
          void**                  ppvDevice) = 0;
};


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


  class D3D11on12Device : public ID3D11On12Device1_DXVK {

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

    HRESULT STDMETHODCALLTYPE GetD3D12Device(
            REFIID                  riid,
            void**                  ppvDevice);

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

#ifndef _MSC_VER
__CRT_UUID_DECL(ID3D11On12Device1_DXVK, 0xbdb64df4,0xea2f,0x4c70,0xb8,0x61,0xaa,0xab,0x12,0x58,0xbb,0x5d);
#endif
