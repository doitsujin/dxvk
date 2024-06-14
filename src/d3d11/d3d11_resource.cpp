#include "d3d11_buffer.h"
#include "d3d11_texture.h"
#include "d3d11_resource.h"
#include "d3d11_context_imm.h"
#include "d3d11_device.h"

#include "../util/util_shared_res.h"

namespace dxvk {

  D3D11DXGIKeyedMutex::D3D11DXGIKeyedMutex(
          ID3D11Resource* pResource,
          D3D11Device*    pDevice)
  : m_resource(pResource),
    m_device(pDevice) {

    m_supported = m_device->GetDXVKDevice()->features().khrWin32KeyedMutex
               && m_device->GetDXVKDevice()->vkd()->wine_vkAcquireKeyedMutex != nullptr
               && m_device->GetDXVKDevice()->vkd()->wine_vkReleaseKeyedMutex != nullptr;
  }


  D3D11DXGIKeyedMutex::~D3D11DXGIKeyedMutex() {

  }


  ULONG STDMETHODCALLTYPE D3D11DXGIKeyedMutex::AddRef() {
    return m_resource->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D11DXGIKeyedMutex::Release() {
    return m_resource->Release();
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIKeyedMutex::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_resource->QueryInterface(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIKeyedMutex::GetPrivateData(
          REFGUID                 Name,
          UINT*                   pDataSize,
          void*                   pData) {
    return m_resource->GetPrivateData(Name, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIKeyedMutex::SetPrivateData(
          REFGUID                 Name,
          UINT                    DataSize,
    const void*                   pData) {
    return m_resource->SetPrivateData(Name, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIKeyedMutex::SetPrivateDataInterface(
          REFGUID                 Name,
    const IUnknown*               pUnknown) {
    return m_resource->SetPrivateDataInterface(Name, pUnknown);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIKeyedMutex::GetParent(
          REFIID                  riid,
          void**                  ppParent) {
    return GetDevice(riid, ppParent);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIKeyedMutex::GetDevice(
          REFIID                  riid,
          void**                  ppDevice) {
    Com<ID3D11Device> device;
    m_resource->GetDevice(&device);
    return device->QueryInterface(riid, ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIKeyedMutex::AcquireSync(
          UINT64                  Key,
          DWORD                   dwMilliseconds) {
    if (!m_supported) {
      if (!m_warned) {
        m_warned = true;
        Logger::err("D3D11DXGIKeyedMutex::AcquireSync: Not supported");
      }
      return S_OK;
    }

    D3D11CommonTexture* texture = GetCommonTexture(m_resource);
    Rc<DxvkDevice> dxvkDevice = m_device->GetDXVKDevice();

    VkResult vr = dxvkDevice->vkd()->wine_vkAcquireKeyedMutex(dxvkDevice->handle(), texture->GetImage()->memory().memory(), Key, dwMilliseconds);
    switch (vr) {
      case VK_SUCCESS: return S_OK;
      case VK_TIMEOUT: return WAIT_TIMEOUT;
      default:         return DXGI_ERROR_INVALID_CALL;
    }
  }

  HRESULT STDMETHODCALLTYPE D3D11DXGIKeyedMutex::ReleaseSync(
          UINT64                  Key) {
    if (!m_supported)
      return S_OK;

    D3D11CommonTexture* texture = GetCommonTexture(m_resource);
    Rc<DxvkDevice> dxvkDevice = m_device->GetDXVKDevice();

    {
      D3D11ImmediateContext* context = m_device->GetContext();
      D3D10Multithread& multithread = context->GetMultithread();
      static bool s_errorShown = false;

      if (!multithread.GetMultithreadProtected() && !std::exchange(s_errorShown, true))
        Logger::warn("D3D11DXGIKeyedMutex::ReleaseSync: Called without context locking enabled.");

      D3D10DeviceLock lock = context->LockContext();
      context->WaitForResource(texture->GetImage(), DxvkCsThread::SynchronizeAll, D3D11_MAP_READ_WRITE, 0);
    }

    return dxvkDevice->vkd()->wine_vkReleaseKeyedMutex(dxvkDevice->handle(), texture->GetImage()->memory().memory(), Key) == VK_SUCCESS
      ? S_OK
      : DXGI_ERROR_INVALID_CALL;
  }

  D3D11DXGIResource::D3D11DXGIResource(
          ID3D11Resource*         pResource,
          D3D11Device*            pDevice)
  : m_resource(pResource),
    m_keyedMutex(pResource, pDevice) {

  }


  D3D11DXGIResource::~D3D11DXGIResource() {

  }


  ULONG STDMETHODCALLTYPE D3D11DXGIResource::AddRef() {
    return m_resource->AddRef();
  }
  

  ULONG STDMETHODCALLTYPE D3D11DXGIResource::Release() {
    return m_resource->Release();
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_resource->QueryInterface(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::GetPrivateData(
          REFGUID                 Name,
          UINT*                   pDataSize,
          void*                   pData) {
    return m_resource->GetPrivateData(Name, pDataSize, pData);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::SetPrivateData(
          REFGUID                 Name,
          UINT                    DataSize,
    const void*                   pData) {
    return m_resource->SetPrivateData(Name, DataSize, pData);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::SetPrivateDataInterface(
          REFGUID                 Name,
    const IUnknown*               pUnknown) {
    return m_resource->SetPrivateDataInterface(Name, pUnknown);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::GetParent(
          REFIID                  riid,
          void**                  ppParent) {
    return GetDevice(riid, ppParent);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::GetDevice(
          REFIID                  riid,
          void**                  ppDevice) {
    Com<ID3D11Device> device;
    m_resource->GetDevice(&device);
    return device->QueryInterface(riid, ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::GetEvictionPriority(
          UINT*                   pEvictionPriority) {
    *pEvictionPriority = m_resource->GetEvictionPriority();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::GetSharedHandle(
          HANDLE*                 pSharedHandle) {
    auto texture = GetCommonTexture(m_resource);
    if (texture == nullptr || pSharedHandle == nullptr ||
        (texture->Desc()->MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE))
      return E_INVALIDARG;

    if (!(texture->Desc()->MiscFlags & (D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX))) {
      *pSharedHandle = NULL;
      return S_OK;
    }

    HANDLE kmtHandle = texture->GetImage()->sharedHandle();

    if (kmtHandle == INVALID_HANDLE_VALUE)
      return E_INVALIDARG;

    *pSharedHandle = kmtHandle;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::GetUsage(
          DXGI_USAGE*             pUsage) {
    D3D11_COMMON_RESOURCE_DESC desc;

    HRESULT hr = GetCommonResourceDesc(m_resource, &desc);

    if (FAILED(hr))
      return hr;
    
    DXGI_USAGE usage = desc.DxgiUsage;

    switch (desc.Usage) {
      case D3D11_USAGE_IMMUTABLE: usage |= DXGI_CPU_ACCESS_NONE;       break;
      case D3D11_USAGE_DEFAULT:   usage |= DXGI_CPU_ACCESS_NONE;       break;
      case D3D11_USAGE_DYNAMIC:   usage |= DXGI_CPU_ACCESS_DYNAMIC;    break;
      case D3D11_USAGE_STAGING:   usage |= DXGI_CPU_ACCESS_READ_WRITE; break;
    }

    // TODO add flags for swap chain back buffers
    if (desc.BindFlags & (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_CONSTANT_BUFFER))
      usage |= DXGI_USAGE_SHADER_INPUT;
    
    if (desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      usage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
    
    if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      usage |= DXGI_USAGE_UNORDERED_ACCESS;

    *pUsage = usage;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::SetEvictionPriority(
          UINT                    EvictionPriority) {
    m_resource->SetEvictionPriority(EvictionPriority);
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::CreateSharedHandle(
    const SECURITY_ATTRIBUTES*    pAttributes,
          DWORD                   dwAccess,
          LPCWSTR                 lpName,
          HANDLE*                 pHandle) {
    auto texture = GetCommonTexture(m_resource);
    if (pHandle) *pHandle = nullptr;
    if (texture == nullptr || pHandle == nullptr ||
        !(texture->Desc()->MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE))
      return E_INVALIDARG;

    if (lpName)
      Logger::warn("Naming shared resources not supported");

    HANDLE handle = texture->GetImage()->sharedHandle();

    if (handle == INVALID_HANDLE_VALUE)
      return E_INVALIDARG;

    *pHandle = handle;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::CreateSubresourceSurface(
          UINT                    index,
          IDXGISurface2**         ppSurface) {
    InitReturnPtr(ppSurface);
    Logger::err("D3D11DXGIResource::CreateSubresourceSurface: Stub");
    return E_NOTIMPL;
  }
  

  HRESULT D3D11DXGIResource::GetKeyedMutex(
          void **ppvObject) {
    auto texture = GetCommonTexture(m_resource);
    if (texture == nullptr || !(texture->Desc()->MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX))
      return E_NOINTERFACE;
    *ppvObject = ref(&m_keyedMutex);
    return S_OK;
  }


  HRESULT GetResource11on12Info(
          ID3D11Resource*             pResource,
          D3D11_ON_12_RESOURCE_INFO*  p11on12Info) {
    auto buffer   = GetCommonBuffer (pResource);
    auto texture  = GetCommonTexture(pResource);

    if (buffer != nullptr)
      *p11on12Info = buffer->Get11on12Info();
    else if (texture != nullptr)
      *p11on12Info = texture->Get11on12Info();
    else
      return E_INVALIDARG;

    if (p11on12Info->Resource == nullptr)
      return E_INVALIDARG;

    return S_OK;
  }


  HRESULT GetCommonResourceDesc(
          ID3D11Resource*             pResource,
          D3D11_COMMON_RESOURCE_DESC* pDesc) {
    auto buffer   = GetCommonBuffer (pResource);
    auto texture  = GetCommonTexture(pResource);

    if (buffer != nullptr) {
      pDesc->Dim            = D3D11_RESOURCE_DIMENSION_BUFFER;
      pDesc->Format         = DXGI_FORMAT_UNKNOWN;
      pDesc->Usage          = buffer->Desc()->Usage;
      pDesc->BindFlags      = buffer->Desc()->BindFlags;
      pDesc->CPUAccessFlags = buffer->Desc()->CPUAccessFlags;
      pDesc->MiscFlags      = buffer->Desc()->MiscFlags;
      pDesc->DxgiUsage      = 0;
      return S_OK;
    } else if (texture != nullptr) {
      pResource->GetType(&pDesc->Dim);
      pDesc->Format         = texture->Desc()->Format;
      pDesc->Usage          = texture->Desc()->Usage;
      pDesc->BindFlags      = texture->Desc()->BindFlags;
      pDesc->CPUAccessFlags = texture->Desc()->CPUAccessFlags;
      pDesc->MiscFlags      = texture->Desc()->MiscFlags;
      pDesc->DxgiUsage      = texture->GetDxgiUsage();
      return S_OK;
    } else {
      pDesc->Dim            = D3D11_RESOURCE_DIMENSION_UNKNOWN;
      pDesc->Format         = DXGI_FORMAT_UNKNOWN;
      pDesc->Usage          = D3D11_USAGE_DEFAULT;
      pDesc->BindFlags      = 0;
      pDesc->CPUAccessFlags = 0;
      pDesc->MiscFlags      = 0;
      pDesc->DxgiUsage      = 0;
      return E_INVALIDARG;
    }
  }


  Rc<DxvkPagedResource> GetPagedResource(
          ID3D11Resource*             pResource) {
    auto texture = GetCommonTexture(pResource);

    if (texture)
      return texture->GetImage();

    return static_cast<D3D11Buffer*>(pResource)->GetBuffer();
  }


  BOOL CheckResourceViewCompatibility(
          ID3D11Resource*             pResource,
          UINT                        BindFlags,
          DXGI_FORMAT                 Format,
          UINT                        Plane) {
    auto texture = GetCommonTexture(pResource);
    auto buffer  = GetCommonBuffer (pResource);
    
    return texture != nullptr
      ? texture->CheckViewCompatibility(BindFlags, Format, Plane)
      : buffer ->CheckViewCompatibility(BindFlags, Format);
  }


  HRESULT ResourceAddRefPrivate(ID3D11Resource* pResource, D3D11_RESOURCE_DIMENSION Type) {
    switch (Type) {
      case D3D11_RESOURCE_DIMENSION_BUFFER:    static_cast<D3D11Buffer*>   (pResource)->AddRefPrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: static_cast<D3D11Texture1D*>(pResource)->AddRefPrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: static_cast<D3D11Texture2D*>(pResource)->AddRefPrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: static_cast<D3D11Texture3D*>(pResource)->AddRefPrivate(); return S_OK;
      default: return E_INVALIDARG;
    }
  }
  

  HRESULT ResourceAddRefPrivate(ID3D11Resource* pResource) {
    D3D11_RESOURCE_DIMENSION dim;
    pResource->GetType(&dim);

    return ResourceAddRefPrivate(pResource, dim);
  }


  HRESULT ResourceReleasePrivate(ID3D11Resource* pResource, D3D11_RESOURCE_DIMENSION Type) {
    switch (Type) {
      case D3D11_RESOURCE_DIMENSION_BUFFER:    static_cast<D3D11Buffer*>   (pResource)->ReleasePrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: static_cast<D3D11Texture1D*>(pResource)->ReleasePrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: static_cast<D3D11Texture2D*>(pResource)->ReleasePrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: static_cast<D3D11Texture3D*>(pResource)->ReleasePrivate(); return S_OK;
      default: return E_INVALIDARG;
    }
  }


  HRESULT ResourceReleasePrivate(ID3D11Resource* pResource) {
    D3D11_RESOURCE_DIMENSION dim;
    pResource->GetType(&dim);

    return ResourceReleasePrivate(pResource, dim);
  }

}
