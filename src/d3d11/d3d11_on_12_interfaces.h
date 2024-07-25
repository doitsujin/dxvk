#pragma once

#include "../vulkan/vulkan_loader.h"

#include <d3d11on12.h>

MIDL_INTERFACE("39da4e09-bd1c-4198-9fae-86bbe3be41fd")
ID3D12DXVKInteropDevice : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetDXGIAdapter(
          REFIID                      iid,
          void**                      ppvObject) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetInstanceExtensions(
          UINT*                       pExtensionCount,
    const char**                      ppExtensions) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetDeviceExtensions(
          UINT*                       pExtensionCount,
    const char**                      ppExtensions) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetDeviceFeatures(
    const VkPhysicalDeviceFeatures2** ppFeatures) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetVulkanHandles(
          VkInstance*                 pVkInstance,
          VkPhysicalDevice*           pVkPhysicalDevice,
          VkDevice*                   pVkDevice) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetVulkanQueueInfo(
          ID3D12CommandQueue*         pCommandQueue,
          VkQueue*                    pVkQueue,
          UINT32*                     pVkQueueFamily) = 0;

  virtual void STDMETHODCALLTYPE GetVulkanImageLayout(
          ID3D12Resource*             pResource,
          D3D12_RESOURCE_STATES       State,
          VkImageLayout*              pVkLayout) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetVulkanResourceInfo(
          ID3D12Resource*             pResource,
          UINT64*                     pVkHandle,
          UINT64*                     pBufferOffset) = 0;

  virtual HRESULT STDMETHODCALLTYPE LockCommandQueue(
          ID3D12CommandQueue*         pCommandQueue) = 0;

  virtual HRESULT STDMETHODCALLTYPE UnlockCommandQueue(
          ID3D12CommandQueue*         pCommandQueue) = 0;

};

#ifndef _MSC_VER
__CRT_UUID_DECL(ID3D12DXVKInteropDevice, 0x39da4e09, 0xbd1c, 0x4198, 0x9f,0xae, 0x86,0xbb,0xe3,0xbe,0x41,0xfd)
#endif
