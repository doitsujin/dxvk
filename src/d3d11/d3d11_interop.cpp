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
  
  
  void STDMETHODCALLTYPE D3D11VkInterop::GetSubmissionQueue1(
          VkQueue*              pQueue,
          uint32_t*             pQueueIndex,
          uint32_t*             pQueueFamilyIndex) {
    auto device = static_cast<D3D11Device*>(m_device)->GetDXVKDevice();
    DxvkDeviceQueue queue = device->queues().graphics;
    
    if (pQueue != nullptr)
      *pQueue = queue.queueHandle;
    
    if (pQueueIndex != nullptr)
      *pQueueIndex = queue.queueIndex;
    
    if (pQueueFamilyIndex != nullptr)
      *pQueueFamilyIndex = queue.queueFamily;
  }

  HRESULT STDMETHODCALLTYPE D3D11VkInterop::CreateTexture2DFromVkImage(
          const D3D11_TEXTURE2D_DESC1 *pDesc,
          VkImage vkImage,
          ID3D11Texture2D **ppTexture2D) {

    InitReturnPtr(ppTexture2D);

    if (!pDesc)
      return E_INVALIDARG;
    
    D3D11_COMMON_TEXTURE_DESC desc;
    desc.Width          = pDesc->Width;
    desc.Height         = pDesc->Height;
    desc.Depth          = 1;
    desc.MipLevels      = pDesc->MipLevels;
    desc.ArraySize      = pDesc->ArraySize;
    desc.Format         = pDesc->Format;
    desc.SampleDesc     = pDesc->SampleDesc;
    desc.Usage          = pDesc->Usage;
    desc.BindFlags      = pDesc->BindFlags;
    desc.CPUAccessFlags = pDesc->CPUAccessFlags;
    desc.MiscFlags      = pDesc->MiscFlags;
    desc.TextureLayout  = pDesc->TextureLayout;
    
    HRESULT hr = D3D11CommonTexture::NormalizeTextureProperties(&desc);

    if (FAILED(hr))
      return hr;
    
    if (!ppTexture2D)
      return S_FALSE;
    
    try {
      Com<D3D11Texture2D> texture = new D3D11Texture2D(m_device, &desc, 0, vkImage);
      *ppTexture2D = texture.ref();
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
}