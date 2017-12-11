#pragma once

#include <dxvk_device.h>

#include "dxgi_object.h"
#include "dxgi_interfaces.h"

namespace dxvk {
  
  template<typename... Base>
  class DxgiResource : public DxgiObject<Base...> {
    
  public:
    
    DxgiResource(
            IDXGIDevicePrivate* pDevice,
            UINT                usage)
    : m_device    (pDevice),
      m_usageFlags(usage) { }
    
    HRESULT GetDevice(REFIID riid, void** ppDevice) {
      return m_device->QueryInterface(riid, ppDevice);
    }
    
    HRESULT STDMETHODCALLTYPE GetSharedHandle(HANDLE *pSharedHandle) {
      Logger::err("DxgiResource::GetSharedHandle: Not implemented");
      return DXGI_ERROR_UNSUPPORTED;
    }
    
    HRESULT STDMETHODCALLTYPE GetUsage(DXGI_USAGE *pUsage) {
      if (pUsage == nullptr)
        return DXGI_ERROR_INVALID_CALL;
      
      *pUsage = m_usageFlags;
      return S_OK;
    }
    
    HRESULT STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) {
      m_evictionPriority = EvictionPriority;
      return S_OK;
    }
    
    HRESULT STDMETHODCALLTYPE GetEvictionPriority(UINT *pEvictionPriority) {
      if (pEvictionPriority == nullptr)
        return DXGI_ERROR_INVALID_CALL;
      
      *pEvictionPriority = m_evictionPriority;
      return S_OK;
    }
    
  private:
    
    Com<IDXGIDevicePrivate> m_device;
    
    UINT m_evictionPriority = DXGI_RESOURCE_PRIORITY_NORMAL;
    UINT m_usageFlags       = 0;
    
  };
  
  
  /**
   * \brief Image resource
   * 
   * Stores a DXVK image and provides a method to retrieve
   * it. D3D textures will be backed by an image resource.
   */
  class DxgiImageResource : public DxgiResource<IDXGIImageResourcePrivate> {
    using Base = DxgiResource<IDXGIImageResourcePrivate>;
  public:
    
    DxgiImageResource(
            IDXGIDevicePrivate*             pDevice,
      const Rc<DxvkImage>&                  image,
            UINT                            usageFlags);
    
    DxgiImageResource(
            IDXGIDevicePrivate*             pDevice,
      const dxvk::DxvkImageCreateInfo*      pCreateInfo,
            VkMemoryPropertyFlags           memoryFlags,
            UINT                            usageFlags);
    
    ~DxgiImageResource();
    
    HRESULT QueryInterface(
            REFIID riid,
            void **ppvObject) final;
    
    HRESULT GetParent(
            REFIID riid,
            void   **ppParent) final;
    
    Rc<DxvkImage> GetDXVKImage() final;
    
    void SetResourceLayer(
            IUnknown*         pLayer) final;
    
  private:
    
    Rc<DxvkImage> m_image;
    IUnknown*     m_layer = nullptr;
    
  };
  
  
  /**
   * \brief Buffer resource
   * 
   * Stores a DXVK buffer and provides a method to retrieve
   * it. D3D buffers will be backed by a buffer resource.
   */
  class DxgiBufferResource : public DxgiResource<IDXGIBufferResourcePrivate> {
    using Base = DxgiResource<IDXGIBufferResourcePrivate>;
  public:
    
    DxgiBufferResource(
            IDXGIDevicePrivate*             pDevice,
      const dxvk::DxvkBufferCreateInfo*     pCreateInfo,
            VkMemoryPropertyFlags           memoryFlags,
            UINT                            usageFlags);
    
    ~DxgiBufferResource();
    
    HRESULT QueryInterface(
            REFIID riid,
            void **ppvObject) final;
    
    HRESULT GetParent(
            REFIID riid,
            void   **ppParent) final;
    
    Rc<DxvkBuffer> GetDXVKBuffer() final;
    
    void SetResourceLayer(
            IUnknown*         pLayer) final;
    
  private:
    
    Rc<DxvkBuffer> m_buffer;
    IUnknown*      m_layer = nullptr;
    
  };

}


extern "C" {
  
  DLLEXPORT HRESULT __stdcall DXGICreateImageResourcePrivate(
          IDXGIDevicePrivate*             pDevice,
    const dxvk::DxvkImageCreateInfo*      pCreateInfo,
          VkMemoryPropertyFlags           memoryFlags,
          UINT                            usageFlags,
          IDXGIImageResourcePrivate**     ppResource);
  
  DLLEXPORT HRESULT __stdcall DXGICreateBufferResourcePrivate(
          IDXGIDevicePrivate*             pDevice,
    const dxvk::DxvkBufferCreateInfo*     pCreateInfo,
          VkMemoryPropertyFlags           memoryFlags,
          UINT                            usageFlags,
          IDXGIBufferResourcePrivate**    ppResource);
  
}