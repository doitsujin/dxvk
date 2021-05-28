#include "d3d11_device.h"
#include "d3d11_buffer.h"
#include "d3d11_resource.h"
#include "d3d11_texture.h"
#include "d3d11_view_rtv.h"

namespace dxvk {
  
  D3D11RenderTargetView::D3D11RenderTargetView(
          D3D11Device*                      pDevice,
          ID3D11Resource*                   pResource,
    const D3D11_RENDER_TARGET_VIEW_DESC1*   pDesc)
  : D3D11DeviceChild<ID3D11RenderTargetView1>(pDevice),
    m_resource(pResource), m_desc(*pDesc), m_d3d10(this) {
    ResourceAddRefPrivate(m_resource);

    auto texture = GetCommonTexture(pResource);

    D3D11_COMMON_RESOURCE_DESC resourceDesc;
    GetCommonResourceDesc(pResource, &resourceDesc);

    DXGI_VK_FORMAT_INFO formatInfo = pDevice->LookupFormat(
      pDesc->Format, DXGI_VK_FORMAT_MODE_COLOR);

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format  = formatInfo.Format;
    viewInfo.aspect  = imageFormatInfo(viewInfo.format)->aspectMask;
    viewInfo.swizzle = formatInfo.Swizzle;
    viewInfo.usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    switch (pDesc->ViewDimension) {
      case D3D11_RTV_DIMENSION_TEXTURE1D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_1D;
        viewInfo.minLevel   = pDesc->Texture1D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;
        
      case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        viewInfo.minLevel   = pDesc->Texture1DArray.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = pDesc->Texture1DArray.FirstArraySlice;
        viewInfo.numLayers  = pDesc->Texture1DArray.ArraySize;
        break;
        
      case D3D11_RTV_DIMENSION_TEXTURE2D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.minLevel   = pDesc->Texture2D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;
        
      case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.minLevel   = pDesc->Texture2DArray.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = pDesc->Texture2DArray.FirstArraySlice;
        viewInfo.numLayers  = pDesc->Texture2DArray.ArraySize;
        break;
        
      case D3D11_RTV_DIMENSION_TEXTURE2DMS:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.minLevel   = 0;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;
      
      case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.minLevel   = 0;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = pDesc->Texture2DMSArray.FirstArraySlice;
        viewInfo.numLayers  = pDesc->Texture2DMSArray.ArraySize;
        break;
      
      case D3D11_RTV_DIMENSION_TEXTURE3D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.minLevel   = pDesc->Texture3D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = pDesc->Texture3D.FirstWSlice;
        viewInfo.numLayers  = pDesc->Texture3D.WSize;
        break;
      
      default:
        throw DxvkError("D3D11: Invalid view dimension for RTV");
    }
    
    if (texture->GetPlaneCount() > 1)
      viewInfo.aspect = vk::getPlaneAspect(GetPlaneSlice(pDesc));

    // Normalize view type so that we won't accidentally
    // bind 2D array views and 2D views at the same time
    if (viewInfo.numLayers == 1) {
      if (viewInfo.type == VK_IMAGE_VIEW_TYPE_1D_ARRAY) viewInfo.type = VK_IMAGE_VIEW_TYPE_1D;
      if (viewInfo.type == VK_IMAGE_VIEW_TYPE_2D_ARRAY) viewInfo.type = VK_IMAGE_VIEW_TYPE_2D;
    }

    // Populate view info struct
    m_info.pResource = pResource;
    m_info.Dimension = resourceDesc.Dim;
    m_info.BindFlags = resourceDesc.BindFlags;
    m_info.Image.Aspects   = viewInfo.aspect;
    m_info.Image.MinLevel  = viewInfo.minLevel;
    m_info.Image.MinLayer  = viewInfo.minLayer;
    m_info.Image.NumLevels = viewInfo.numLevels;
    m_info.Image.NumLayers = viewInfo.numLayers;
    
    // Create the underlying image view object
    m_view = pDevice->GetDXVKDevice()->createImageView(texture->GetImage(), viewInfo);
  }
  
  
  D3D11RenderTargetView::~D3D11RenderTargetView() {
    ResourceReleasePrivate(m_resource);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11RenderTargetView::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11View)
     || riid == __uuidof(ID3D11RenderTargetView)
     || riid == __uuidof(ID3D11RenderTargetView1)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10View)
     || riid == __uuidof(ID3D10RenderTargetView)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("D3D11RenderTargetView::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11RenderTargetView::GetResource(ID3D11Resource** ppResource) {
    *ppResource = ref(m_resource);
  }
  
  
  void STDMETHODCALLTYPE D3D11RenderTargetView::GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc) {
    pDesc->Format            = m_desc.Format;
    pDesc->ViewDimension     = m_desc.ViewDimension;

    switch (m_desc.ViewDimension) {
      case D3D11_RTV_DIMENSION_UNKNOWN:
        break;

      case D3D11_RTV_DIMENSION_BUFFER:
        pDesc->Buffer = m_desc.Buffer;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE1D:
        pDesc->Texture1D = m_desc.Texture1D;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
        pDesc->Texture1DArray = m_desc.Texture1DArray;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2D:
        pDesc->Texture2D.MipSlice = m_desc.Texture2D.MipSlice;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
        pDesc->Texture2DArray.MipSlice        = m_desc.Texture2DArray.MipSlice;
        pDesc->Texture2DArray.FirstArraySlice = m_desc.Texture2DArray.FirstArraySlice;
        pDesc->Texture2DArray.ArraySize       = m_desc.Texture2DArray.ArraySize;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2DMS:
        pDesc->Texture2DMS = m_desc.Texture2DMS;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
        pDesc->Texture2DMSArray = m_desc.Texture2DMSArray;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE3D:
        pDesc->Texture3D = m_desc.Texture3D;
        break;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11RenderTargetView::GetDesc1(D3D11_RENDER_TARGET_VIEW_DESC1* pDesc) {
    *pDesc = m_desc;
  }
  
  
  HRESULT D3D11RenderTargetView::GetDescFromResource(
          ID3D11Resource*                   pResource,
          D3D11_RENDER_TARGET_VIEW_DESC1*   pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.ArraySize == 1) {
          pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
          pDesc->Texture1D.MipSlice = 0;
        } else {
          pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
          pDesc->Texture1DArray.MipSlice        = 0;
          pDesc->Texture1DArray.FirstArraySlice = 0;
          pDesc->Texture1DArray.ArraySize       = resourceDesc.ArraySize;
        }
      } return S_OK;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D11Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.SampleDesc.Count == 1) {
          if (resourceDesc.ArraySize == 1) {
            pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            pDesc->Texture2D.MipSlice   = 0;
            pDesc->Texture2D.PlaneSlice = 0;
          } else {
            pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            pDesc->Texture2DArray.MipSlice        = 0;
            pDesc->Texture2DArray.FirstArraySlice = 0;
            pDesc->Texture2DArray.ArraySize       = resourceDesc.ArraySize;
            pDesc->Texture2DArray.PlaneSlice      = 0;
          }
        } else {
          if (resourceDesc.ArraySize == 1) {
            pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
          } else {
            pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
            pDesc->Texture2DMSArray.FirstArraySlice = 0;
            pDesc->Texture2DMSArray.ArraySize       = resourceDesc.ArraySize;
          }
        }
      } return S_OK;
        
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        D3D11_TEXTURE3D_DESC resourceDesc;
        static_cast<D3D11Texture3D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format         = resourceDesc.Format;
        pDesc->ViewDimension  = D3D11_RTV_DIMENSION_TEXTURE3D;
        pDesc->Texture3D.MipSlice    = 0;
        pDesc->Texture3D.FirstWSlice = 0;
        pDesc->Texture3D.WSize       = resourceDesc.Depth;
      } return S_OK;
      
      default:
        Logger::err(str::format(
          "D3D11: Unsupported dimension for render target view: ",
          resourceDim));
        return E_INVALIDARG;
    }
  }
  
  
  D3D11_RENDER_TARGET_VIEW_DESC1 D3D11RenderTargetView::PromoteDesc(
    const D3D11_RENDER_TARGET_VIEW_DESC*    pDesc,
          UINT                              Plane) {
    D3D11_RENDER_TARGET_VIEW_DESC1 dstDesc;
    dstDesc.Format            = pDesc->Format;
    dstDesc.ViewDimension     = pDesc->ViewDimension;

    switch (pDesc->ViewDimension) {
      case D3D11_RTV_DIMENSION_UNKNOWN:
        break;

      case D3D11_RTV_DIMENSION_BUFFER:
        dstDesc.Buffer = pDesc->Buffer;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE1D:
        dstDesc.Texture1D = pDesc->Texture1D;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
        dstDesc.Texture1DArray = pDesc->Texture1DArray;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2D:
        dstDesc.Texture2D.MipSlice   = pDesc->Texture2D.MipSlice;
        dstDesc.Texture2D.PlaneSlice = Plane;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
        dstDesc.Texture2DArray.MipSlice        = pDesc->Texture2DArray.MipSlice;
        dstDesc.Texture2DArray.FirstArraySlice = pDesc->Texture2DArray.FirstArraySlice;
        dstDesc.Texture2DArray.ArraySize       = pDesc->Texture2DArray.ArraySize;
        dstDesc.Texture2DArray.PlaneSlice      = Plane;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2DMS:
        dstDesc.Texture2DMS = pDesc->Texture2DMS;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
        dstDesc.Texture2DMSArray = pDesc->Texture2DMSArray;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE3D:
        dstDesc.Texture3D = pDesc->Texture3D;
        break;
    }

    return dstDesc;
  }


  HRESULT D3D11RenderTargetView::NormalizeDesc(
          ID3D11Resource*                   pResource,
          D3D11_RENDER_TARGET_VIEW_DESC1*   pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t numLayers = 0;
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_BUFFER: {
        if (pDesc->ViewDimension != D3D11_RTV_DIMENSION_BUFFER) {
          Logger::err("D3D11: Incompatible view dimension for Buffer");
          return E_INVALIDARG;
        }
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_RTV_DIMENSION_TEXTURE1D
         && pDesc->ViewDimension != D3D11_RTV_DIMENSION_TEXTURE1DARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture1D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D11Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D
         && pDesc->ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DARRAY
         && pDesc->ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DMS
         && pDesc->ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture2D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        D3D11_TEXTURE3D_DESC resourceDesc;
        static_cast<D3D11Texture3D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_RTV_DIMENSION_TEXTURE3D) {
          Logger::err("D3D11: Incompatible view dimension for Texture3D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = std::max(resourceDesc.Depth >> pDesc->Texture3D.MipSlice, 1u);
      } break;
      
      default:
        return E_INVALIDARG;
    }
    
    if (pDesc->Format == DXGI_FORMAT_UNKNOWN)
      pDesc->Format = format;
    
    switch (pDesc->ViewDimension) {
      case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
        if (pDesc->Texture1DArray.ArraySize > numLayers - pDesc->Texture1DArray.FirstArraySlice)
          pDesc->Texture1DArray.ArraySize = numLayers - pDesc->Texture1DArray.FirstArraySlice;
        break;
      
      case D3D11_RTV_DIMENSION_TEXTURE2D:
        break;
      
      case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
        if (pDesc->Texture2DArray.ArraySize > numLayers - pDesc->Texture2DArray.FirstArraySlice)
          pDesc->Texture2DArray.ArraySize = numLayers - pDesc->Texture2DArray.FirstArraySlice;
        break;
      
      case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
        if (pDesc->Texture2DMSArray.ArraySize > numLayers - pDesc->Texture2DMSArray.FirstArraySlice)
          pDesc->Texture2DMSArray.ArraySize = numLayers - pDesc->Texture2DMSArray.FirstArraySlice;
        break;
      
      case D3D11_RTV_DIMENSION_TEXTURE3D:
        if (pDesc->Texture3D.WSize > numLayers - pDesc->Texture3D.FirstWSlice)
          pDesc->Texture3D.WSize = numLayers - pDesc->Texture3D.FirstWSlice;
        break;
      
      default:
        break;
    }
    
    return S_OK;
  }
  

  UINT D3D11RenderTargetView::GetPlaneSlice(const D3D11_RENDER_TARGET_VIEW_DESC1* pDesc) {
    switch (pDesc->ViewDimension) {
      case D3D11_RTV_DIMENSION_TEXTURE2D:
        return pDesc->Texture2D.PlaneSlice;
      case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
        return pDesc->Texture2DArray.PlaneSlice;
      default:
        return 0;
    }
  }

}
