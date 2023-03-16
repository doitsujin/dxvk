#include "d3d11_device.h"
#include "d3d11_on_12.h"

namespace dxvk {

  D3D11on12Device::D3D11on12Device(
          D3D11DXGIDevice*        pContainer,
          D3D11Device*            pDevice,
          ID3D12Device*           pD3D12Device,
          ID3D12CommandQueue*     pD3D12Queue)
  : m_container   (pContainer),
    m_device      (pDevice),
    m_d3d12Device (pD3D12Device),
    m_d3d12Queue  (pD3D12Queue) {

  }


  D3D11on12Device::~D3D11on12Device() {

  }


  ULONG STDMETHODCALLTYPE D3D11on12Device::AddRef() {
    return m_container->AddRef();
  }
  

  ULONG STDMETHODCALLTYPE D3D11on12Device::Release() {
    return m_container->Release();
  }
  

  HRESULT STDMETHODCALLTYPE D3D11on12Device::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11on12Device::CreateWrappedResource(
          IUnknown*               pResource12,
    const D3D11_RESOURCE_FLAGS*   pResourceFlags,
          D3D12_RESOURCE_STATES   InputState,
          D3D12_RESOURCE_STATES   OutputState,
          REFIID                  riid,
          void**                  ppResource11) {
    Logger::err("D3D11on12Device::CreateWrappedResource: Stub");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11on12Device::ReleaseWrappedResources(
          ID3D11Resource* const*  ppResources,
          UINT                    ResourceCount) {
    Logger::err("D3D11on12Device::ReleaseWrappedResources: Stub");
  }


  void STDMETHODCALLTYPE D3D11on12Device::AcquireWrappedResources(
          ID3D11Resource* const*  ppResources,
          UINT                    ResourceCount) {
    Logger::err("D3D11on12Device::AcquireWrappedResources: Stub");
  }

}
