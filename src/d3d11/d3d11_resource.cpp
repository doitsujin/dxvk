#include "d3d11_resource.h"

namespace dxvk {

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
      return S_OK;
    } else if (texture != nullptr) {
      pResource->GetType(&pDesc->Dim);
      pDesc->Format         = texture->Desc()->Format;
      pDesc->Usage          = texture->Desc()->Usage;
      pDesc->BindFlags      = texture->Desc()->BindFlags;
      pDesc->CPUAccessFlags = texture->Desc()->CPUAccessFlags;
      pDesc->MiscFlags      = texture->Desc()->MiscFlags;
      return S_OK;
    } else {
      pDesc->Dim            = D3D11_RESOURCE_DIMENSION_UNKNOWN;
      pDesc->Format         = DXGI_FORMAT_UNKNOWN;
      pDesc->Usage          = D3D11_USAGE_DEFAULT;
      pDesc->BindFlags      = 0;
      pDesc->CPUAccessFlags = 0;
      pDesc->MiscFlags      = 0;
      return E_INVALIDARG;
    }
  }


  BOOL CheckResourceViewCompatibility(
          ID3D11Resource*             pResource,
          UINT                        BindFlags,
          DXGI_FORMAT                 Format) {
    auto texture = GetCommonTexture(pResource);
    auto buffer  = GetCommonBuffer (pResource);
    
    return texture != nullptr
      ? texture->CheckViewCompatibility(BindFlags, Format)
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