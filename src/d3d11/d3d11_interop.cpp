#include "d3d11_context_imm.h"
#include "d3d11_interop.h"
#include "d3d11_device.h"

#include "../dxvk/dxvk_adapter.h"
#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_instance.h"

namespace dxvk {
  
  D3D11VkInterop::D3D11VkInterop(
          IDXGIObject*          pContainer,
          D3D11Device*          pDevice)
  : m_container (pContainer),
    m_device    (pDevice) { }
  
  
  D3D11VkInterop::~D3D11VkInterop() {
    
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11VkInterop::AddRef() {
    return m_container->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11VkInterop::Release() {
    return m_container->Release();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11VkInterop::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }
  
  
  void STDMETHODCALLTYPE D3D11VkInterop::GetVulkanHandles(
          VkInstance*           pInstance,
          VkPhysicalDevice*     pPhysDev,
          VkDevice*             pDevice) {
    auto device   = m_device->GetDXVKDevice();
    auto adapter  = device->adapter();
    auto instance = device->instance();
    
    if (pDevice != nullptr)
      *pDevice = device->handle();
    
    if (pPhysDev != nullptr)
      *pPhysDev = adapter->handle();
    
    if (pInstance != nullptr)
      *pInstance = instance->handle();
  }
  
  
  void STDMETHODCALLTYPE D3D11VkInterop::GetSubmissionQueue(
          VkQueue*              pQueue,
          uint32_t*             pQueueFamilyIndex) {
    auto device = static_cast<D3D11Device*>(m_device)->GetDXVKDevice();
    DxvkDeviceQueue queue = device->queues().graphics;
    
    if (pQueue != nullptr)
      *pQueue = queue.queueHandle;
    
    if (pQueueFamilyIndex != nullptr)
      *pQueueFamilyIndex = queue.queueFamily;
  }
  
  
  void STDMETHODCALLTYPE D3D11VkInterop::TransitionSurfaceLayout(
          IDXGIVkInteropSurface*    pSurface,
    const VkImageSubresourceRange*  pSubresources,
          VkImageLayout             OldLayout,
          VkImageLayout             NewLayout) {
    Com<ID3D11DeviceContext> deviceContext = nullptr;
    m_device->GetImmediateContext(&deviceContext);
    
    auto immediateContext = static_cast<D3D11ImmediateContext*>(deviceContext.ptr());
    
    immediateContext->TransitionSurfaceLayout(
      pSurface, pSubresources, OldLayout, NewLayout);
  }
  
  
  void STDMETHODCALLTYPE D3D11VkInterop::FlushRenderingCommands() {
    Com<ID3D11DeviceContext> deviceContext = nullptr;
    m_device->GetImmediateContext(&deviceContext);
    
    auto immediateContext = static_cast<D3D11ImmediateContext*>(deviceContext.ptr());
    immediateContext->Flush();
    immediateContext->SynchronizeCsThread();
  }
  
  
  void STDMETHODCALLTYPE D3D11VkInterop::LockSubmissionQueue() {
    m_device->GetDXVKDevice()->lockSubmission();
  }
  
  
  void STDMETHODCALLTYPE D3D11VkInterop::ReleaseSubmissionQueue() {
    m_device->GetDXVKDevice()->unlockSubmission();
  }
  
}