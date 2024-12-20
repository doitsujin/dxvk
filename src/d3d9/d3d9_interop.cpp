#include "d3d9_interop.h"
#include "d3d9_interface.h"
#include "d3d9_common_texture.h"
#include "d3d9_device.h"
#include "d3d9_texture.h"
#include "d3d9_buffer.h"
#include "d3d9_initializer.h"

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

  HRESULT STDMETHODCALLTYPE D3D9VkInteropInterface::GetInstanceExtensions(
          UINT* pExtensionCount,
    const char** ppExtensions) {
    if (pExtensionCount == nullptr)
      return D3DERR_INVALIDCALL;

    const DxvkNameList& extensions = m_interface->GetInstance()->extensionNameList();

    if (ppExtensions == nullptr) {
      *pExtensionCount = extensions.count();
      return D3D_OK;
    }

    // Write 
    UINT count = 0;
    UINT maxCount = *pExtensionCount;
    for (uint32_t i = 0; i < extensions.count() && i < maxCount; i++) {
      ppExtensions[i] = extensions.name(i);
      count++;
    }

    *pExtensionCount = count;
    return (count < maxCount) ? D3DERR_MOREDATA : D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9VkInteropInterface::GetDeviceCreateInfo(
        UINT                        Adapter,
        D3D9VkDeviceCreateInfo**    ppCreateInfo) {
    if (unlikely(ppCreateInfo == nullptr))
      return D3DERR_INVALIDCALL;
    InitReturnPtr(ppCreateInfo);

    D3D9VkDeviceCreateInfo* pCreateInfo = new D3D9VkDeviceCreateInfo;

    auto* adapter = m_interface->GetAdapter(Adapter);
    if (unlikely(adapter == nullptr))
      return D3DERR_INVALIDCALL;

    auto dxvkAdapter = adapter->GetDXVKAdapter();

    pCreateInfo->features = D3D9DeviceEx::GetDeviceFeatures(dxvkAdapter);

    DxvkDeviceCreateExtInfo extInfo;
    DxvkDeviceCreateQueueInfo queueInfo;
    bool success = dxvkAdapter->getDeviceCreateInfo(
      m_interface->GetInstance(),
      pCreateInfo->info,
      pCreateInfo->features,
      extInfo,
      queueInfo);

    if (!success) {
      delete pCreateInfo;
      return D3DERR_INVALIDCALL;
    }

    // Queue family indices
    pCreateInfo->graphicsQueueFamily = queueInfo.queueFamilies.graphics;
    pCreateInfo->transferQueueFamily = queueInfo.queueFamilies.transfer;
    pCreateInfo->sparseQueueFamily   = queueInfo.queueFamilies.sparse;

    // Queue create infos
    const size_t queueCount = queueInfo.queueInfos.size();
    pCreateInfo->pQueueCreateInfos = queueCount ? new VkDeviceQueueCreateInfo[queueCount] : nullptr;
    for (size_t i = 0; i < queueCount; i++) {
      pCreateInfo->pQueueCreateInfos[i] = queueInfo.queueInfos[i];
    }
    pCreateInfo->info.pQueueCreateInfos     = pCreateInfo->pQueueCreateInfos;
    pCreateInfo->info.queueCreateInfoCount  = queueCount;

    // Extension names
    const uint32_t extCount = extInfo.extensionNameList.count();
    pCreateInfo->ppEnabledExtensionNames = extCount > 0 ? new const char*[extCount] : nullptr;
    for (uint32_t i = 0; i < extCount; i++) {
      const char* nameStr = extInfo.extensionNameList.name(i);
      size_t nameLen = std::strlen(nameStr);
      char* name = new char[nameLen + 1];
			std::strncpy(name, nameStr, nameLen);
      name[nameLen] = '\0';
      pCreateInfo->ppEnabledExtensionNames[i] = name;
    }
    pCreateInfo->info.ppEnabledExtensionNames = pCreateInfo->ppEnabledExtensionNames;
    pCreateInfo->info.enabledExtensionCount   = extCount;

    *ppCreateInfo = pCreateInfo;
    return D3D_OK;
  }

  void STDMETHODCALLTYPE D3D9VkInteropInterface::FreeDeviceCreateInfo(
          D3D9VkDeviceCreateInfo* pCreateInfo) {
    if (!pCreateInfo)
      return;

    if (pCreateInfo->ppEnabledExtensionNames != nullptr) {
      for (uint32_t i = 0; i < pCreateInfo->info.enabledExtensionCount; i++) {
        delete pCreateInfo->ppEnabledExtensionNames[i];
      }
      
      delete[] pCreateInfo->ppEnabledExtensionNames;
    }

    if (pCreateInfo->pQueueCreateInfos != nullptr)
      delete[] pCreateInfo->pQueueCreateInfos;
    delete pCreateInfo;
  }

  HRESULT STDMETHODCALLTYPE D3D9VkInteropInterface::ImportDevice(
          UINT                        Adapter,
          D3DDEVTYPE                  DeviceType,
          HWND                        hFocusWindow,
          DWORD                       BehaviorFlags,
          D3DPRESENT_PARAMETERS*      pPresentationParameters,
          D3DDISPLAYMODEEX*           pFullscreenDisplayMode,
          D3D9VkDeviceImportInfo*     pInfo,
          IDirect3DDevice9Ex**        ppReturnedDevice) {
    InitReturnPtr(ppReturnedDevice);

    if (unlikely(ppReturnedDevice        == nullptr
              || pPresentationParameters == nullptr))
      return D3DERR_INVALIDCALL;

    // Creating a device with D3DCREATE_PUREDEVICE only works in conjunction
    // with D3DCREATE_HARDWARE_VERTEXPROCESSING on native drivers.
    if (unlikely(BehaviorFlags & D3DCREATE_PUREDEVICE &&
               !(BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING)))
      return D3DERR_INVALIDCALL;

    HRESULT hr;
    // Black Desert creates a D3DDEVTYPE_NULLREF device and
    // expects it be created despite passing invalid parameters.
    if (likely(DeviceType != D3DDEVTYPE_NULLREF)) {
      hr = m_interface->ValidatePresentationParameters(pPresentationParameters);

      if (unlikely(FAILED(hr)))
        return hr;
    }

    auto* adapter = m_interface->GetAdapter(Adapter);

    if (adapter == nullptr)
      return D3DERR_INVALIDCALL;

    auto dxvkAdapter = adapter->GetDXVKAdapter();

    DxvkDeviceImportInfo info;
    info.device         = pInfo->device;
    info.queue          = pInfo->graphicsQueue;
    info.queueFamily    = pInfo->graphicsQueueFamily;
    info.extensionCount = pInfo->extensionCount;
    info.extensionNames = pInfo->extensionNames;
    info.features       = pInfo->features;
    info.queueCallback  = pInfo->queueLockCallback;

    try {
      auto dxvkDevice = dxvkAdapter->importDevice(m_interface->GetInstance(), info);

      auto* device = new D3D9DeviceEx(
        m_interface,
        adapter,
        DeviceType,
        hFocusWindow,
        BehaviorFlags,
        dxvkDevice);

      hr = device->InitialReset(pPresentationParameters, pFullscreenDisplayMode);

      if (unlikely(FAILED(hr)))
        return hr;

      *ppReturnedDevice = ref(device);
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_NOTAVAILABLE;
    }

    return D3D_OK;
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

  static Rc<DxvkPagedResource> GetDxvkResource(IDirect3DResource9 *pResource) {
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
    return m_device->WaitForResource(*GetDxvkResource(pResource), DxvkCsThread::SynchronizeAll, MapFlags);
  }

  HRESULT STDMETHODCALLTYPE D3D9VkInteropDevice::CreateImage(
          const D3D9VkExtImageDesc* params,
          IDirect3DResource9**      ppResult) {
    InitReturnPtr(ppResult);

    if (unlikely(ppResult == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(params == nullptr))
      return D3DERR_INVALIDCALL;

    /////////////////////////////
    // Image desc validation

    // Cannot create a volume by itself, use D3DRTYPE_VOLUMETEXTURE
    if (unlikely(params->Type == D3DRTYPE_VOLUME))
      return D3DERR_INVALIDCALL;

    // Only allowed: SURFACE, TEXTURE, CUBETEXTURE, VOLUMETEXTURE
    if (unlikely(params->Type < D3DRTYPE_SURFACE || params->Type > D3DRTYPE_CUBETEXTURE))
      return D3DERR_INVALIDCALL;

    // Only volume textures can have depth > 1
    if (unlikely(params->Type != D3DRTYPE_VOLUMETEXTURE && params->Depth > 1))
      return D3DERR_INVALIDCALL;

    if (params->Type == D3DRTYPE_SURFACE) {
      // Surfaces can only have 1 mip level
      if (unlikely(params->MipLevels > 1))
        return D3DERR_INVALIDCALL;

      if (unlikely(params->MultiSample > D3DMULTISAMPLE_16_SAMPLES))
        return D3DERR_INVALIDCALL;
    } else {
      // Textures can't be multisampled
      if (unlikely(params->MultiSample != D3DMULTISAMPLE_NONE))
        return D3DERR_INVALIDCALL;
    }

    D3D9_COMMON_TEXTURE_DESC desc;
    desc.Width              = params->Width;
    desc.Height             = params->Height;
    desc.Depth              = params->Depth;
    desc.ArraySize          = params->Type == D3DRTYPE_CUBETEXTURE ? 6 : 1;
    desc.MipLevels          = params->MipLevels;
    desc.Usage              = params->Usage;
    desc.Format             = EnumerateFormat(params->Format);
    desc.Pool               = params->Pool;
    desc.Discard            = params->Discard;
    desc.MultiSample        = params->MultiSample;
    desc.MultisampleQuality = params->MultiSampleQuality;
    desc.IsBackBuffer       = FALSE;
    desc.IsAttachmentOnly   = params->IsAttachmentOnly;
    desc.IsLockable         = params->IsLockable;
    desc.ImageUsage         = params->ImageUsage;
    
    D3DRESOURCETYPE textureType = params->Type == D3DRTYPE_SURFACE ? D3DRTYPE_TEXTURE : params->Type;

    if (FAILED(D3D9CommonTexture::NormalizeTextureProperties(m_device, textureType, &desc)))
      return D3DERR_INVALIDCALL;

    switch (params->Type) {
      case D3DRTYPE_SURFACE:
        return CreateTextureResource<D3D9Surface>(desc, ppResult);

      case D3DRTYPE_TEXTURE:
        return CreateTextureResource<D3D9Texture2D>(desc, ppResult);

      case D3DRTYPE_VOLUMETEXTURE:
        return CreateTextureResource<D3D9Texture3D>(desc, ppResult);

      case D3DRTYPE_CUBETEXTURE:
        return CreateTextureResource<D3D9TextureCube>(desc, ppResult);

      default:
        return D3DERR_INVALIDCALL;
    }
  }

  template <typename ResourceType>
  HRESULT D3D9VkInteropDevice::CreateTextureResource(
          const D3D9_COMMON_TEXTURE_DESC& desc,
          IDirect3DResource9**            ppResult) {
    try {
      const Com<ResourceType> texture = new ResourceType(m_device, &desc);
      m_device->m_initializer->InitTexture(texture->GetCommonTexture());
      *ppResult = texture.ref();

      if (desc.Pool == D3DPOOL_DEFAULT)
        m_device->m_losableResourceCounter++;

      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_OUTOFVIDEOMEMORY;
    }
  }

}
