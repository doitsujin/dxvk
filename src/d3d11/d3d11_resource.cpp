#include "d3d11_buffer.h"
#include "d3d11_texture.h"
#include "d3d11_resource.h"

namespace dxvk {

  D3D11DXGIResource::D3D11DXGIResource(
          ID3D11Resource*         pResource)
  : m_resource(pResource) {

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
    InitReturnPtr(pSharedHandle);
    Logger::err("D3D11DXGIResource::GetSharedHandle: Stub");
    return E_NOTIMPL;
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
    InitReturnPtr(pHandle);
    Logger::err("D3D11DXGIResource::CreateSharedHandle: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGIResource::CreateSubresourceSurface(
          UINT                    index,
          IDXGISurface2**         ppSurface) {
    InitReturnPtr(ppSurface);
    Logger::err("D3D11DXGIResource::CreateSubresourceSurface: Stub");
    return E_NOTIMPL;
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


  HRESULT ResourceAddRefPrivate(ID3D11Resource* pResource) {
    D3D11_RESOURCE_DIMENSION dim;
    pResource->GetType(&dim);

    switch (dim) {
      case D3D11_RESOURCE_DIMENSION_BUFFER:    static_cast<D3D11Buffer*>   (pResource)->AddRefPrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: static_cast<D3D11Texture1D*>(pResource)->AddRefPrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: static_cast<D3D11Texture2D*>(pResource)->AddRefPrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: static_cast<D3D11Texture3D*>(pResource)->AddRefPrivate(); return S_OK;
      default: return E_INVALIDARG;
    }
  }
  

  HRESULT ResourceReleasePrivate(ID3D11Resource* pResource) {
    D3D11_RESOURCE_DIMENSION dim;
    pResource->GetType(&dim);

    switch (dim) {
      case D3D11_RESOURCE_DIMENSION_BUFFER:    static_cast<D3D11Buffer*>   (pResource)->ReleasePrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: static_cast<D3D11Texture1D*>(pResource)->ReleasePrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: static_cast<D3D11Texture2D*>(pResource)->ReleasePrivate(); return S_OK;
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: static_cast<D3D11Texture3D*>(pResource)->ReleasePrivate(); return S_OK;
      default: return E_INVALIDARG;
    }
  }

}