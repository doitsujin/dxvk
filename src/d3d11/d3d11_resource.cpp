#include "d3d11_resource.h"

namespace dxvk {

  HRESULT GetCommonResourceDesc(
          ID3D11Resource*             pResource,
          D3D11_COMMON_RESOURCE_DESC* pDesc) {
    auto buffer   = GetCommonBuffer (pResource);
    auto texture  = GetCommonTexture(pResource);

    if (buffer != nullptr) {
      pDesc->Usage          = buffer->Desc()->Usage;
      pDesc->BindFlags      = buffer->Desc()->BindFlags;
      pDesc->CPUAccessFlags = buffer->Desc()->CPUAccessFlags;
      pDesc->MiscFlags      = buffer->Desc()->MiscFlags;
      return S_OK;
    } else if (texture != nullptr) {
      pDesc->Usage          = texture->Desc()->Usage;
      pDesc->BindFlags      = texture->Desc()->BindFlags;
      pDesc->CPUAccessFlags = texture->Desc()->CPUAccessFlags;
      pDesc->MiscFlags      = texture->Desc()->MiscFlags;
      return S_OK;
    } else {
      pDesc->Usage          = D3D11_USAGE_DEFAULT;
      pDesc->BindFlags      = 0;
      pDesc->CPUAccessFlags = 0;
      pDesc->MiscFlags      = 0;
      return E_INVALIDARG;
    }
  }


  BOOL CheckResourceBindFlags(
          ID3D11Resource*             pResource,
          UINT                        BindFlags) {
    D3D11_COMMON_RESOURCE_DESC desc;
    GetCommonResourceDesc(pResource, &desc);

    return (desc.BindFlags & BindFlags) == BindFlags;
  }


  BOOL CheckResourceViewFormatCompatibility(
          ID3D11Resource*             pResource,
          DXGI_FORMAT                 Format) {
    auto texture = GetCommonTexture(pResource);
    
    return texture != nullptr
      ? texture->CheckViewFormatCompatibility(Format)
      : true; /* for buffers */
  }

}