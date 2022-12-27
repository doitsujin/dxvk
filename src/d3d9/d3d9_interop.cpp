#include "d3d9_interop.h"
#include "d3d9_interface.h"
#include "d3d9_common_texture.h"
#include "d3d9_device.h"
#include "d3d9_texture.h"
#include "d3d9_buffer.h"

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

  ////////////////////////////////
  // Device Interop
  ///////////////////////////////

  D3D9VkInteropDevice::D3D9VkInteropDevice(
          D3D9DeviceEx*         pInterface)
    : m_device(pInterface) {

  }

  D3D9VkInteropDevice::~D3D9VkInteropDevice() {

  }

  ULONG STDMETHODCALLTYPE D3D9VkInteropDevice::AddRef() {
    return m_device->AddRef();
  }
  
  ULONG STDMETHODCALLTYPE D3D9VkInteropDevice::Release() {
    return m_device->Release();
  }
  
  HRESULT STDMETHODCALLTYPE D3D9VkInteropDevice::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::GetVulkanHandles(
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

  void STDMETHODCALLTYPE D3D9VkInteropDevice::GetSubmissionQueue(
          VkQueue*              pQueue,
          uint32_t*             pQueueIndex,
          uint32_t*             pQueueFamilyIndex) {
    auto device = m_device->GetDXVKDevice();
    DxvkDeviceQueue queue = device->queues().graphics;
    
    if (pQueue != nullptr)
      *pQueue = queue.queueHandle;
    
    if (pQueueIndex != nullptr)
      *pQueueIndex = queue.queueIndex;
    
    if (pQueueFamilyIndex != nullptr)
      *pQueueFamilyIndex = queue.queueFamily;
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::TransitionTextureLayout(
          ID3D9VkInteropTexture*    pTexture,
    const VkImageSubresourceRange*  pSubresources,
          VkImageLayout             OldLayout,
          VkImageLayout             NewLayout) {
    auto texture = static_cast<D3D9VkInteropTexture *>(pTexture)->GetCommonTexture();

    m_device->EmitCs([
      cImage        = texture->GetImage(),
      cSubresources = *pSubresources,
      cOldLayout    = OldLayout,
      cNewLayout    = NewLayout
    ] (DxvkContext* ctx) {
      ctx->transformImage(
        cImage, cSubresources,
        cOldLayout, cNewLayout);
    });
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::FlushRenderingCommands() {
    m_device->Flush();
    m_device->SynchronizeCsThread(DxvkCsThread::SynchronizeAll);
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::LockSubmissionQueue() {
    m_device->GetDXVKDevice()->lockSubmission();
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::ReleaseSubmissionQueue() {
    m_device->GetDXVKDevice()->unlockSubmission();
  }

  void STDMETHODCALLTYPE D3D9VkInteropDevice::LockDevice() {
    m_lock = m_device->LockDevice();
  }
  
  void STDMETHODCALLTYPE D3D9VkInteropDevice::UnlockDevice() {
    m_lock = D3D9DeviceLock();
  }

  static Rc<DxvkResource> GetDxvkResource(IDirect3DResource9 *pResource) {
    switch (pResource->GetType()) {
      case D3DRTYPE_SURFACE:       return static_cast<D3D9Surface*>     (pResource)->GetCommonTexture()->GetImage();
      // Does not inherit from IDirect3DResource9... lol.
      //case D3DRTYPE_VOLUME:        return static_cast<D3D9Volume*>      (pResource)->GetCommonTexture()->GetImage();
      case D3DRTYPE_TEXTURE:       return static_cast<D3D9Texture2D*>   (pResource)->GetCommonTexture()->GetImage();
      case D3DRTYPE_VOLUMETEXTURE: return static_cast<D3D9Texture3D*>   (pResource)->GetCommonTexture()->GetImage();
      case D3DRTYPE_CUBETEXTURE:   return static_cast<D3D9TextureCube*> (pResource)->GetCommonTexture()->GetImage();
      case D3DRTYPE_VERTEXBUFFER:  return static_cast<D3D9VertexBuffer*>(pResource)->GetCommonBuffer()->GetBuffer<D3D9_COMMON_BUFFER_TYPE_REAL>();
      case D3DRTYPE_INDEXBUFFER:   return static_cast<D3D9IndexBuffer*> (pResource)->GetCommonBuffer()->GetBuffer<D3D9_COMMON_BUFFER_TYPE_REAL>();
      default:                     return nullptr;
    }
  }

  bool STDMETHODCALLTYPE D3D9VkInteropDevice::WaitForResource(
          IDirect3DResource9*  pResource,
          DWORD                MapFlags) {
    return m_device->WaitForResource(GetDxvkResource(pResource), DxvkCsThread::SynchronizeAll, MapFlags);
  }

}
