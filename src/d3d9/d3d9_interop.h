#pragma once

#include "d3d9_interfaces.h"
#include "d3d9_multithread.h"

namespace dxvk {

  class D3D9InterfaceEx;
  class D3D9CommonTexture;
  class D3D9DeviceEx;
  struct D3D9_COMMON_TEXTURE_DESC;

  class D3D9VkInteropInterface final : public ID3D9VkInteropInterface1 {

  public:

    D3D9VkInteropInterface(
            D3D9InterfaceEx*      pInterface);

    ~D3D9VkInteropInterface();

    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject);

    void STDMETHODCALLTYPE GetInstanceHandle(
            VkInstance*           pInstance);

    void STDMETHODCALLTYPE GetPhysicalDeviceHandle(
            UINT                  Adapter,
            VkPhysicalDevice*     pPhysicalDevice);

    HRESULT STDMETHODCALLTYPE GetInstanceExtensions(
            UINT*                 pExtensionCount,
      const char**                ppExtensions);

  private:

    D3D9InterfaceEx* m_interface;

  };

  class D3D9VkInteropTexture final : public ID3D9VkInteropTexture {

  public:

    D3D9VkInteropTexture(
            IUnknown*             pInterface,
            D3D9CommonTexture*    pTexture);

    ~D3D9VkInteropTexture();

    ULONG STDMETHODCALLTYPE AddRef();
    
    ULONG STDMETHODCALLTYPE Release();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject);

    HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(
            VkImage*              pHandle,
            VkImageLayout*        pLayout,
            VkImageCreateInfo*    pInfo);

    D3D9CommonTexture* GetCommonTexture() { return m_texture; }

  private:

    IUnknown*          m_interface;
    D3D9CommonTexture* m_texture;

  };

  class D3D9VkInteropDevice final : public ID3D9VkInteropDevice {

  public:

    D3D9VkInteropDevice(
            D3D9DeviceEx*         pInterface);

    ~D3D9VkInteropDevice();

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
            uint32_t*             pQueueIndex,
            uint32_t*             pQueueFamilyIndex);

    void STDMETHODCALLTYPE TransitionTextureLayout(
            ID3D9VkInteropTexture*    pTexture,
      const VkImageSubresourceRange*  pSubresources,
            VkImageLayout             OldLayout,
            VkImageLayout             NewLayout);

    void STDMETHODCALLTYPE FlushRenderingCommands();

    void STDMETHODCALLTYPE LockSubmissionQueue();

    void STDMETHODCALLTYPE ReleaseSubmissionQueue();

    void STDMETHODCALLTYPE LockDevice();
    
    void STDMETHODCALLTYPE UnlockDevice();

    bool STDMETHODCALLTYPE WaitForResource(
            IDirect3DResource9*  pResource,
            DWORD                MapFlags);

    HRESULT STDMETHODCALLTYPE CreateImage(
            const D3D9VkExtImageDesc* desc,
            IDirect3DResource9**      ppResult);

  private:

    template <typename ResourceType>
    HRESULT CreateTextureResource(
            const D3D9_COMMON_TEXTURE_DESC& desc,
            IDirect3DResource9**            ppResult);

    D3D9DeviceEx*  m_device;
    D3D9DeviceLock m_lock;

  };

}
