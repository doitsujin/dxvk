#include "d3d9_interop.h"
#include "d3d9_interface.h"
#include "d3d9_common_texture.h"

namespace dxvk {

  ////////////////////////////////
  // Interface Interop
  ///////////////////////////////

  D3D9VkInteropInterface::D3D9VkInteropInterface(
          D3D9InterfaceEx*      pInterface)
    : m_interface(pInterface) {

  }

  D3D9VkInteropInterface::~D3D9VkInteropInterface() {

  }

  ULONG STDMETHODCALLTYPE D3D9VkInteropInterface::AddRef() {
    return m_interface->AddRef();
  }
  
  ULONG STDMETHODCALLTYPE D3D9VkInteropInterface::Release() {
    return m_interface->Release();
  }
  
  HRESULT STDMETHODCALLTYPE D3D9VkInteropInterface::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_interface->QueryInterface(riid, ppvObject);
  }

  void STDMETHODCALLTYPE D3D9VkInteropInterface::GetInstanceHandle(
          VkInstance*           pInstance) {
    if (pInstance != nullptr)
      *pInstance = m_interface->GetInstance()->handle();
  }

  void STDMETHODCALLTYPE D3D9VkInteropInterface::GetPhysicalDeviceHandle(
          UINT                  Adapter,
          VkPhysicalDevice*     pPhysicalDevice) {
    if (pPhysicalDevice != nullptr) {
      D3D9Adapter* adapter = m_interface->GetAdapter(Adapter);
      *pPhysicalDevice = adapter ? adapter->GetDXVKAdapter()->handle() : nullptr;
    }
  }

  ////////////////////////////////
  // Texture Interop
  ///////////////////////////////

  D3D9VkInteropTexture::D3D9VkInteropTexture(
          IUnknown*             pInterface,
          D3D9CommonTexture*    pTexture)
    : m_interface(pInterface)
    , m_texture  (pTexture) {

  }

  D3D9VkInteropTexture::~D3D9VkInteropTexture() {

  }

  ULONG STDMETHODCALLTYPE D3D9VkInteropTexture::AddRef() {
    return m_interface->AddRef();
  }
  
  ULONG STDMETHODCALLTYPE D3D9VkInteropTexture::Release() {
    return m_interface->Release();
  }
  
  HRESULT STDMETHODCALLTYPE D3D9VkInteropTexture::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_interface->QueryInterface(riid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE D3D9VkInteropTexture::GetVulkanImageInfo(
          VkImage*              pHandle,
          VkImageLayout*        pLayout,
          VkImageCreateInfo*    pInfo) {
    const Rc<DxvkImage> image = m_texture->GetImage();
    const DxvkImageCreateInfo& info = image->info();
    
    if (pHandle != nullptr)
      *pHandle = image->handle();
    
    if (pLayout != nullptr)
      *pLayout = info.layout;
    
    if (pInfo != nullptr) {
      // We currently don't support any extended structures
      if (pInfo->sType != VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
       || pInfo->pNext != nullptr)
        return D3DERR_INVALIDCALL;
      
      pInfo->flags          = 0;
      pInfo->imageType      = info.type;
      pInfo->format         = info.format;
      pInfo->extent         = info.extent;
      pInfo->mipLevels      = info.mipLevels;
      pInfo->arrayLayers    = info.numLayers;
      pInfo->samples        = info.sampleCount;
      pInfo->tiling         = info.tiling;
      pInfo->usage          = info.usage;
      pInfo->sharingMode    = VK_SHARING_MODE_EXCLUSIVE;
      pInfo->queueFamilyIndexCount = 0;
      pInfo->initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    
    return S_OK;
  }

}
