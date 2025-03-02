#include "d3d11_context_imm.h"
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
    Com<ID3D12DXVKInteropDevice> interopDevice;
    m_d3d12Device->QueryInterface(__uuidof(ID3D12DXVKInteropDevice), reinterpret_cast<void**>(&interopDevice));

    D3D11_ON_12_RESOURCE_INFO info = { };
    info.InputState = InputState;
    info.OutputState = OutputState;
    info.IsWrappedResource = TRUE;

    // 11on12 technically allows importing D3D12 heaps as tile pools,
    // but we don't support importing sparse resources at this time.
    if (FAILED(pResource12->QueryInterface(__uuidof(ID3D12Resource), reinterpret_cast<void**>(&info.Resource)))) {
      Logger::err("D3D11on12Device::CreateWrappedResource: Resource not a valid D3D12 resource");
      return E_INVALIDARG;
    }

    // Query Vulkan resource handle and buffer offset as necessary
    if (FAILED(interopDevice->GetVulkanResourceInfo(info.Resource.ptr(), &info.VulkanHandle, &info.VulkanOffset))) {
      Logger::err("D3D11on12Device::CreateWrappedResource: Failed to retrieve Vulkan resource info");
      return E_INVALIDARG;
    }

    Com<ID3D11Resource> resource;
    D3D12_RESOURCE_DESC desc = info.Resource->GetDesc();

    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
      D3D11_BUFFER_DESC bufferDesc;

      if (FAILED(D3D11Buffer::GetDescFromD3D12(info.Resource.ptr(), pResourceFlags, &bufferDesc)))
        return E_INVALIDARG;

      resource = new D3D11Buffer(m_device, &bufferDesc, &info);
    } else {
      D3D11_COMMON_TEXTURE_DESC textureDesc;

      if (FAILED(D3D11CommonTexture::GetDescFromD3D12(info.Resource.ptr(), pResourceFlags, &textureDesc)))
        return E_INVALIDARG;

      switch (desc.Dimension) {
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
          resource = new D3D11Texture1D(m_device, &textureDesc, &info);
          break;

        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
          resource = new D3D11Texture2D(m_device, &textureDesc, &info, nullptr);
          break;

        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
          resource = new D3D11Texture3D(m_device, &textureDesc, &info);
          break;

        default:
          Logger::err("D3D11on12Device::CreateWrappedResource: Unhandled resource dimension");
          return E_INVALIDARG;
      }
    }

    return resource->QueryInterface(riid, ppResource11);
  }


  void STDMETHODCALLTYPE D3D11on12Device::ReleaseWrappedResources(
          ID3D11Resource* const*  ppResources,
          UINT                    ResourceCount) {
    Com<ID3D12DXVKInteropDevice> interopDevice;
    m_d3d12Device->QueryInterface(__uuidof(ID3D12DXVKInteropDevice), reinterpret_cast<void**>(&interopDevice));

    for (uint32_t i = 0; i < ResourceCount; i++) {
      D3D11_ON_12_RESOURCE_INFO info;

      if (FAILED(GetResource11on12Info(ppResources[i], &info)) || !info.IsWrappedResource) {
        Logger::warn("D3D11on12Device::ReleaseWrappedResources: Resource not a wrapped resource, skipping");
        continue;
      }

      VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
      interopDevice->GetVulkanImageLayout(info.Resource.ptr(), info.OutputState, &layout);
      m_device->GetContext()->Release11on12Resource(ppResources[i], layout);
    }
  }


  void STDMETHODCALLTYPE D3D11on12Device::AcquireWrappedResources(
          ID3D11Resource* const*  ppResources,
          UINT                    ResourceCount) {
    Com<ID3D12DXVKInteropDevice> interopDevice;
    m_d3d12Device->QueryInterface(__uuidof(ID3D12DXVKInteropDevice), reinterpret_cast<void**>(&interopDevice));

    for (uint32_t i = 0; i < ResourceCount; i++) {
      D3D11_ON_12_RESOURCE_INFO info;

      if (FAILED(GetResource11on12Info(ppResources[i], &info)) || !info.IsWrappedResource) {
        Logger::warn("D3D11on12Device::AcquireWrappedResources: Resource not a wrapped resource, skipping");
        continue;
      }

      VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
      interopDevice->GetVulkanImageLayout(info.Resource.ptr(), info.InputState, &layout);
      m_device->GetContext()->Acquire11on12Resource(ppResources[i], layout);
    }
  }


  HRESULT STDMETHODCALLTYPE D3D11on12Device::GetD3D12Device(
          REFIID                  riid,
          void**                  ppvDevice) {
    return m_d3d12Queue->GetDevice(riid, ppvDevice);
  }

}
