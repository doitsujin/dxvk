#pragma once

#include "../dxgi/dxgi_interfaces.h"

#include "d3d11_include.h"

namespace dxvk {

  class D3D11Device;
  
  class D3D11VkInterop : public ComObject<IDXGIVkInteropDevice> {
    
  public:
    
    D3D11VkInterop(
            IDXGIObject*          pContainer,
            D3D11Device*          pDevice);

    ~D3D11VkInterop();
    
    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject);
    
    void STDMETHODCALLTYPE GetVulkanHandles(
            VkInstance*           pInstance,
            VkPhysicalDevice*     pPhysDev,
            VkDevice*             pDevice);
    
    void STDMETHODCALLTYPE GetSubmissionQueue(
            VkQueue*              pQueue,
            uint32_t*             pQueueFamilyIndex);
    
    void STDMETHODCALLTYPE TransitionSurfaceLayout(
            IDXGIVkInteropSurface*    pSurface,
      const VkImageSubresourceRange*  pSubresources,
            VkImageLayout             OldLayout,
            VkImageLayout             NewLayout);
    
    void STDMETHODCALLTYPE FlushRenderingCommands();
    
    void STDMETHODCALLTYPE LockSubmissionQueue();
    
    void STDMETHODCALLTYPE ReleaseSubmissionQueue();
    
  private:
    
    IDXGIObject* m_container;
    D3D11Device* m_device;
    
  };
  
}