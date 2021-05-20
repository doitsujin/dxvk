#include "d3d11_device.h"
#include "d3d11_buffer.h"
#include "d3d11_resource.h"
#include "d3d11_texture.h"
#include "d3d11_view_srv.h"

namespace dxvk {
  
  D3D11ShaderResourceView::D3D11ShaderResourceView(
          D3D11Device*                      pDevice,
          ID3D11Resource*                   pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC1* pDesc)
  : D3D11DeviceChild<ID3D11ShaderResourceView1>(pDevice),
    m_resource(pResource), m_desc(*pDesc), m_d3d10(this) {
    ResourceAddRefPrivate(m_resource);

    D3D11_COMMON_RESOURCE_DESC resourceDesc;
    GetCommonResourceDesc(pResource, &resourceDesc);

    // Basic view resource info
    m_info.pResource = pResource;
    m_info.Dimension = resourceDesc.Dim;
    m_info.BindFlags = resourceDesc.BindFlags;

    if (resourceDesc.Dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      auto buffer = static_cast<D3D11Buffer*>(pResource);

      // Move buffer description to a common struct to
      // avoid having to handle the two cases separately
      D3D11_BUFFEREX_SRV bufInfo;
      
      if (pDesc->ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX) {
        bufInfo.FirstElement = pDesc->BufferEx.FirstElement;
        bufInfo.NumElements  = pDesc->BufferEx.NumElements;
        bufInfo.Flags        = pDesc->BufferEx.Flags;
      } else if (pDesc->ViewDimension == D3D11_SRV_DIMENSION_BUFFER) {
        bufInfo.FirstElement = pDesc->Buffer.FirstElement;
        bufInfo.NumElements  = pDesc->Buffer.NumElements;
        bufInfo.Flags        = 0;
      } else {
        throw DxvkError("D3D11: Invalid view dimension for buffer SRV");
      }

      // Fill in buffer view info
      DxvkBufferViewCreateInfo viewInfo;

      if (bufInfo.Flags & D3D11_BUFFEREX_SRV_FLAG_RAW) {
        // Raw buffer view. We'll represent this as a
        // uniform texel buffer with UINT32 elements.
        viewInfo.format = VK_FORMAT_R32_UINT;
        viewInfo.rangeOffset = sizeof(uint32_t) * bufInfo.FirstElement;
        viewInfo.rangeLength = sizeof(uint32_t) * bufInfo.NumElements;
      } else if (pDesc->Format == DXGI_FORMAT_UNKNOWN) {
        // Structured buffer view
        viewInfo.format = VK_FORMAT_R32_UINT;
        viewInfo.rangeOffset = buffer->Desc()->StructureByteStride * bufInfo.FirstElement;
        viewInfo.rangeLength = buffer->Desc()->StructureByteStride * bufInfo.NumElements;
      } else {
        viewInfo.format = pDevice->LookupFormat(pDesc->Format, DXGI_VK_FORMAT_MODE_COLOR).Format;
        
        const DxvkFormatInfo* formatInfo = imageFormatInfo(viewInfo.format);
        viewInfo.rangeOffset = formatInfo->elementSize * bufInfo.FirstElement;
        viewInfo.rangeLength = formatInfo->elementSize * bufInfo.NumElements;
      }

      // Populate view info struct
      m_info.Buffer.Offset = viewInfo.rangeOffset;
      m_info.Buffer.Length = viewInfo.rangeLength;

      // Create underlying buffer view object
      m_bufferView = pDevice->GetDXVKDevice()->createBufferView(
        buffer->GetBuffer(), viewInfo);
    } else {
      auto texture = GetCommonTexture(pResource);
      auto formatInfo = pDevice->LookupFormat(pDesc->Format, texture->GetFormatMode());
      
      DxvkImageViewCreateInfo viewInfo;
      viewInfo.format  = formatInfo.Format;
      viewInfo.aspect  = formatInfo.Aspect;
      viewInfo.swizzle = formatInfo.Swizzle;
      viewInfo.usage   = VK_IMAGE_USAGE_SAMPLED_BIT;

      // Shaders expect the stencil value in the G component
      if (viewInfo.aspect == VK_IMAGE_ASPECT_STENCIL_BIT) {
        viewInfo.swizzle = VkComponentMapping {
          VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R,
          VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO };
      }
      
      switch (pDesc->ViewDimension) {
        case D3D11_SRV_DIMENSION_TEXTURE1D:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_1D;
          viewInfo.minLevel  = pDesc->Texture1D.MostDetailedMip;
          viewInfo.numLevels = pDesc->Texture1D.MipLevels;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 1;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
          viewInfo.minLevel  = pDesc->Texture1DArray.MostDetailedMip;
          viewInfo.numLevels = pDesc->Texture1DArray.MipLevels;
          viewInfo.minLayer  = pDesc->Texture1DArray.FirstArraySlice;
          viewInfo.numLayers = pDesc->Texture1DArray.ArraySize;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURE2D:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_2D;
          viewInfo.minLevel  = pDesc->Texture2D.MostDetailedMip;
          viewInfo.numLevels = pDesc->Texture2D.MipLevels;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 1;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
          viewInfo.minLevel  = pDesc->Texture2DArray.MostDetailedMip;
          viewInfo.numLevels = pDesc->Texture2DArray.MipLevels;
          viewInfo.minLayer  = pDesc->Texture2DArray.FirstArraySlice;
          viewInfo.numLayers = pDesc->Texture2DArray.ArraySize;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURE2DMS:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_2D;
          viewInfo.minLevel  = 0;
          viewInfo.numLevels = 1;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 1;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
          viewInfo.minLevel  = 0;
          viewInfo.numLevels = 1;
          viewInfo.minLayer  = pDesc->Texture2DMSArray.FirstArraySlice;
          viewInfo.numLayers = pDesc->Texture2DMSArray.ArraySize;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURE3D:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_3D;
          viewInfo.minLevel  = pDesc->Texture3D.MostDetailedMip;
          viewInfo.numLevels = pDesc->Texture3D.MipLevels;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 1;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURECUBE:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
          viewInfo.minLevel  = pDesc->TextureCube.MostDetailedMip;
          viewInfo.numLevels = pDesc->TextureCube.MipLevels;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 6;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
          viewInfo.minLevel  = pDesc->TextureCubeArray.MostDetailedMip;
          viewInfo.numLevels = pDesc->TextureCubeArray.MipLevels;
          viewInfo.minLayer  = pDesc->TextureCubeArray.First2DArrayFace;
          viewInfo.numLayers = pDesc->TextureCubeArray.NumCubes * 6;
          break;
          
        default:
          throw DxvkError("D3D11: Invalid view dimension for image SRV");
      }
      
      if (texture->GetPlaneCount() > 1)
        viewInfo.aspect = vk::getPlaneAspect(GetPlaneSlice(pDesc));

      // Populate view info struct
      m_info.Image.Aspects   = viewInfo.aspect;
      m_info.Image.MinLevel  = viewInfo.minLevel;
      m_info.Image.MinLayer  = viewInfo.minLayer;
      m_info.Image.NumLevels = viewInfo.numLevels;
      m_info.Image.NumLayers = viewInfo.numLayers;

      // Create the underlying image view object
      m_imageView = pDevice->GetDXVKDevice()->createImageView(texture->GetImage(), viewInfo);
    }
  }
  
  
  D3D11ShaderResourceView::~D3D11ShaderResourceView() {
    ResourceReleasePrivate(m_resource);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ShaderResourceView::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11View)
     || riid == __uuidof(ID3D11ShaderResourceView)
     || riid == __uuidof(ID3D11ShaderResourceView1)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10View)
     || riid == __uuidof(ID3D10ShaderResourceView)
     || riid == __uuidof(ID3D10ShaderResourceView1)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("D3D11ShaderResourceView::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11ShaderResourceView::GetResource(ID3D11Resource** ppResource) {
    *ppResource = ref(m_resource);
  }
  
  
  void STDMETHODCALLTYPE D3D11ShaderResourceView::GetDesc(D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc) {
    pDesc->Format            = m_desc.Format;
    pDesc->ViewDimension     = m_desc.ViewDimension;

    switch (m_desc.ViewDimension) {
      case D3D11_SRV_DIMENSION_UNKNOWN:
        break;

      case D3D11_SRV_DIMENSION_BUFFER:
        pDesc->Buffer = m_desc.Buffer;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE1D:
        pDesc->Texture1D = m_desc.Texture1D;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
        pDesc->Texture1DArray = m_desc.Texture1DArray;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE2D:
        pDesc->Texture2D.MostDetailedMip = m_desc.Texture2D.MostDetailedMip;
        pDesc->Texture2D.MipLevels       = m_desc.Texture2D.MipLevels;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
        pDesc->Texture2DArray.MostDetailedMip = m_desc.Texture2DArray.MostDetailedMip;
        pDesc->Texture2DArray.MipLevels       = m_desc.Texture2DArray.MipLevels;
        pDesc->Texture2DArray.FirstArraySlice = m_desc.Texture2DArray.FirstArraySlice;
        pDesc->Texture2DArray.ArraySize       = m_desc.Texture2DArray.ArraySize;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE2DMS:
        pDesc->Texture2DMS = m_desc.Texture2DMS;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
        pDesc->Texture2DMSArray = m_desc.Texture2DMSArray;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE3D:
        pDesc->Texture3D = m_desc.Texture3D;
        break;

      case D3D11_SRV_DIMENSION_TEXTURECUBE:
        pDesc->TextureCube = m_desc.TextureCube;
        break;

      case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
        pDesc->TextureCubeArray = m_desc.TextureCubeArray;
        break;

      case D3D11_SRV_DIMENSION_BUFFEREX:
        pDesc->BufferEx = m_desc.BufferEx;
        break;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11ShaderResourceView::GetDesc1(D3D11_SHADER_RESOURCE_VIEW_DESC1* pDesc) {
    *pDesc = m_desc;
  }
  
  
  HRESULT D3D11ShaderResourceView::GetDescFromResource(
          ID3D11Resource*                   pResource,
          D3D11_SHADER_RESOURCE_VIEW_DESC1* pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_BUFFER: {
        D3D11_BUFFER_DESC bufferDesc;
        static_cast<D3D11Buffer*>(pResource)->GetDesc(&bufferDesc);
        
        if (bufferDesc.MiscFlags == D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
          pDesc->Format              = DXGI_FORMAT_UNKNOWN;
          pDesc->ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
          pDesc->Buffer.FirstElement = 0;
          pDesc->Buffer.NumElements  = bufferDesc.ByteWidth / bufferDesc.StructureByteStride;
          return S_OK;
        }
      } return E_INVALIDARG;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.ArraySize == 1) {
          pDesc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
          pDesc->Texture1D.MostDetailedMip = 0;
          pDesc->Texture1D.MipLevels       = resourceDesc.MipLevels;
        } else {
          pDesc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
          pDesc->Texture1DArray.MostDetailedMip = 0;
          pDesc->Texture1DArray.MipLevels       = resourceDesc.MipLevels;
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
            pDesc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            pDesc->Texture2D.MostDetailedMip = 0;
            pDesc->Texture2D.MipLevels       = resourceDesc.MipLevels;
            pDesc->Texture2D.PlaneSlice      = 0;
          } else {
            pDesc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            pDesc->Texture2DArray.MostDetailedMip = 0;
            pDesc->Texture2DArray.MipLevels       = resourceDesc.MipLevels;
            pDesc->Texture2DArray.FirstArraySlice = 0;
            pDesc->Texture2DArray.ArraySize       = resourceDesc.ArraySize;
            pDesc->Texture2DArray.PlaneSlice      = 0;
          }
        } else {
          if (resourceDesc.ArraySize == 1) {
            pDesc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
          } else {
            pDesc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
            pDesc->Texture2DMSArray.FirstArraySlice = 0;
            pDesc->Texture2DMSArray.ArraySize       = resourceDesc.ArraySize;
          }
        }
      } return S_OK;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        D3D11_TEXTURE3D_DESC resourceDesc;
        static_cast<D3D11Texture3D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        pDesc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        pDesc->Texture3D.MostDetailedMip = 0;
        pDesc->Texture3D.MipLevels       = resourceDesc.MipLevels;
      } return S_OK;
      
      default:
        Logger::err(str::format(
          "D3D11: Unsupported dimension for shader resource view: ",
          resourceDim));
        return E_INVALIDARG;
    }
  }
  
  
  D3D11_SHADER_RESOURCE_VIEW_DESC1 D3D11ShaderResourceView::PromoteDesc(
    const D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc,
          UINT                              Plane) {
    D3D11_SHADER_RESOURCE_VIEW_DESC1 dstDesc;
    dstDesc.Format            = pDesc->Format;
    dstDesc.ViewDimension     = pDesc->ViewDimension;

    switch (pDesc->ViewDimension) {
      case D3D11_SRV_DIMENSION_UNKNOWN:
        break;

      case D3D11_SRV_DIMENSION_BUFFER:
        dstDesc.Buffer = pDesc->Buffer;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE1D:
        dstDesc.Texture1D = pDesc->Texture1D;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
        dstDesc.Texture1DArray = pDesc->Texture1DArray;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE2D:
        dstDesc.Texture2D.MostDetailedMip = pDesc->Texture2D.MostDetailedMip;
        dstDesc.Texture2D.MipLevels       = pDesc->Texture2D.MipLevels;
        dstDesc.Texture2D.PlaneSlice      = Plane;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
        dstDesc.Texture2DArray.MostDetailedMip = pDesc->Texture2DArray.MostDetailedMip;
        dstDesc.Texture2DArray.MipLevels       = pDesc->Texture2DArray.MipLevels;
        dstDesc.Texture2DArray.FirstArraySlice = pDesc->Texture2DArray.FirstArraySlice;
        dstDesc.Texture2DArray.ArraySize       = pDesc->Texture2DArray.ArraySize;
        dstDesc.Texture2DArray.PlaneSlice      = Plane;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE2DMS:
        dstDesc.Texture2DMS = pDesc->Texture2DMS;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
        dstDesc.Texture2DMSArray = pDesc->Texture2DMSArray;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE3D:
        dstDesc.Texture3D = pDesc->Texture3D;
        break;

      case D3D11_SRV_DIMENSION_TEXTURECUBE:
        dstDesc.TextureCube = pDesc->TextureCube;
        break;

      case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
        dstDesc.TextureCubeArray = pDesc->TextureCubeArray;
        break;

      case D3D11_SRV_DIMENSION_BUFFEREX:
        dstDesc.BufferEx = pDesc->BufferEx;
        break;
    }

    return dstDesc;
  }


  HRESULT D3D11ShaderResourceView::NormalizeDesc(
          ID3D11Resource*                   pResource,
          D3D11_SHADER_RESOURCE_VIEW_DESC1* pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t mipLevels = 0;
    uint32_t numLayers = 0;
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_BUFFER: {
        if (pDesc->ViewDimension != D3D11_SRV_DIMENSION_BUFFER
         && pDesc->ViewDimension != D3D11_SRV_DIMENSION_BUFFEREX) {
          Logger::err("D3D11: Incompatible view dimension for Buffer");
          return E_INVALIDARG;
        }
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURE1D
         && pDesc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURE1DARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture1D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        mipLevels = resourceDesc.MipLevels;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D11Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D
         && pDesc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2DARRAY
         && pDesc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2DMS
         && pDesc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY
         && pDesc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURECUBE
         && pDesc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURECUBEARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture2D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        mipLevels = resourceDesc.MipLevels;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        D3D11_TEXTURE3D_DESC resourceDesc;
        static_cast<D3D11Texture3D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURE3D) {
          Logger::err("D3D11: Incompatible view dimension for Texture3D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        mipLevels = resourceDesc.MipLevels;
        numLayers = 1;
      } break;
      
      default:
        return E_INVALIDARG;
    }
    
    if (pDesc->Format == DXGI_FORMAT_UNKNOWN)
      pDesc->Format = format;
    
    switch (pDesc->ViewDimension) {
      case D3D11_SRV_DIMENSION_BUFFER:
        if (pDesc->Buffer.NumElements == 0)
          return E_INVALIDARG;
        break;

      case D3D11_SRV_DIMENSION_BUFFEREX:
        if (pDesc->BufferEx.NumElements == 0)
          return E_INVALIDARG;
        break;

      case D3D11_SRV_DIMENSION_TEXTURE1D:
        if (pDesc->Texture1D.MipLevels > mipLevels - pDesc->Texture1D.MostDetailedMip)
          pDesc->Texture1D.MipLevels = mipLevels - pDesc->Texture1D.MostDetailedMip;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
        if (pDesc->Texture1DArray.MipLevels > mipLevels - pDesc->Texture1DArray.MostDetailedMip)
          pDesc->Texture1DArray.MipLevels = mipLevels - pDesc->Texture1DArray.MostDetailedMip;
        if (pDesc->Texture1DArray.ArraySize > numLayers - pDesc->Texture1DArray.FirstArraySlice)
          pDesc->Texture1DArray.ArraySize = numLayers - pDesc->Texture1DArray.FirstArraySlice;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURE2D:
        if (pDesc->Texture2D.MipLevels > mipLevels - pDesc->Texture2D.MostDetailedMip)
          pDesc->Texture2D.MipLevels = mipLevels - pDesc->Texture2D.MostDetailedMip;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
        if (pDesc->Texture2DArray.MipLevels > mipLevels - pDesc->Texture2DArray.MostDetailedMip)
          pDesc->Texture2DArray.MipLevels = mipLevels - pDesc->Texture2DArray.MostDetailedMip;
        if (pDesc->Texture2DArray.ArraySize > numLayers - pDesc->Texture2DArray.FirstArraySlice)
          pDesc->Texture2DArray.ArraySize = numLayers - pDesc->Texture2DArray.FirstArraySlice;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
        if (pDesc->Texture2DMSArray.ArraySize > numLayers - pDesc->Texture2DMSArray.FirstArraySlice)
          pDesc->Texture2DMSArray.ArraySize = numLayers - pDesc->Texture2DMSArray.FirstArraySlice;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURECUBE:
        if (pDesc->TextureCube.MipLevels > mipLevels - pDesc->TextureCube.MostDetailedMip)
          pDesc->TextureCube.MipLevels = mipLevels - pDesc->TextureCube.MostDetailedMip;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
        if (pDesc->TextureCubeArray.MipLevels > mipLevels - pDesc->TextureCubeArray.MostDetailedMip)
          pDesc->TextureCubeArray.MipLevels = mipLevels - pDesc->TextureCubeArray.MostDetailedMip;
        if (pDesc->TextureCubeArray.NumCubes > (numLayers - pDesc->TextureCubeArray.First2DArrayFace) / 6)
          pDesc->TextureCubeArray.NumCubes = (numLayers - pDesc->TextureCubeArray.First2DArrayFace) / 6;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURE3D:
        if (pDesc->Texture3D.MipLevels > mipLevels - pDesc->Texture3D.MostDetailedMip)
          pDesc->Texture3D.MipLevels = mipLevels - pDesc->Texture3D.MostDetailedMip;
        break;
      
      default:
        break;
    }
    
    return S_OK;
  }
  

  UINT D3D11ShaderResourceView::GetPlaneSlice(const D3D11_SHADER_RESOURCE_VIEW_DESC1* pDesc) {
    switch (pDesc->ViewDimension) {
      case D3D11_SRV_DIMENSION_TEXTURE2D:
        return pDesc->Texture2D.PlaneSlice;
      case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
        return pDesc->Texture2DArray.PlaneSlice;
      default:
        return 0;
    }
  }

}
