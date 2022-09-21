#pragma once

#include "d3d9_interfaces.h"

namespace dxvk {

  class D3D9InterfaceEx;
  class D3D9CommonTexture;

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

}
