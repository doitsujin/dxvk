#pragma once

#include "../dxgi/dxgi_interfaces.h"

#include "d3d11_include.h"

namespace dxvk {

  class D3D11Device;
  
  class D3D11VkInterop : public ComObject<IDXGIVkInteropDevice1> {
    
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
    
    void STDMETHODCALLTYPE GetSubmissionQueue1(
            VkQueue*              pQueue,
            uint32_t*             pQueueIndex,
            uint32_t*             pQueueFamilyIndex);
    
    HRESULT STDMETHODCALLTYPE CreateTexture2DFromVkImage(
            const D3D11_TEXTURE2D_DESC1* pDesc,
            VkImage                      vkImage,
            ID3D11Texture2D**            ppTexture2D);
    
  private:
    
    IDXGIObject* m_container;
    D3D11Device* m_device;
    
  };
  
}