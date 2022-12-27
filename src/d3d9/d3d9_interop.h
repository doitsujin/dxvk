#pragma once

#include "d3d9_interfaces.h"
#include "d3d9_multithread.h"

namespace dxvk {

  class D3D9InterfaceEx;
  class D3D9CommonTexture;
  class D3D9DeviceEx;

  class D3D9VkInteropInterface final : public ID3D9VkInteropInterface {

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

  private:

    D3D9DeviceEx*  m_device;
    D3D9DeviceLock m_lock;

  };

}
