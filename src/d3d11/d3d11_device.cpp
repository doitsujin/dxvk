#include <algorithm>
#include <cstring>

#include "d3d11_buffer.h"
#include "d3d11_class_linkage.h"
#include "d3d11_context_def.h"
#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_input_layout.h"
#include "d3d11_present.h"
#include "d3d11_query.h"
#include "d3d11_sampler.h"
#include "d3d11_shader.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  D3D11Device::D3D11Device(
          IDXGIDevicePrivate* dxgiDevice,
          D3D_FEATURE_LEVEL   featureLevel,
          UINT                featureFlags)
  : m_dxgiDevice    (dxgiDevice),
    m_presentDevice (new D3D11PresentDevice()),
    m_featureLevel  (featureLevel),
    m_featureFlags  (featureFlags),
    m_dxvkDevice    (m_dxgiDevice->GetDXVKDevice()),
    m_dxvkAdapter   (m_dxvkDevice->adapter()),
    m_dxbcOptions   (m_dxvkDevice) {
    Com<IDXGIAdapter> adapter;
    
    if (FAILED(m_dxgiDevice->GetAdapter(&adapter))
     || FAILED(adapter->QueryInterface(__uuidof(IDXGIAdapterPrivate),
          reinterpret_cast<void**>(&m_dxgiAdapter))))
      throw DxvkError("D3D11Device: Failed to query adapter");
    
    m_dxgiDevice->SetDeviceLayer(this);
    m_presentDevice->SetDeviceLayer(this);
    
    m_context = new D3D11ImmediateContext(this, m_dxvkDevice);
    
    m_resourceInitContext = m_dxvkDevice->createContext();
    m_resourceInitContext->beginRecording(
      m_dxvkDevice->createCommandList());
    
    CreateCounterBuffer();
  }
  
  
  D3D11Device::~D3D11Device() {
    m_presentDevice->SetDeviceLayer(nullptr);
    m_dxgiDevice->SetDeviceLayer(nullptr);
    delete m_context;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Device);
    
    if (riid == __uuidof(IDXGIDevice)
     || riid == __uuidof(IDXGIDevice1)
     || riid == __uuidof(IDXGIDevicePrivate))
      return m_dxgiDevice->QueryInterface(riid, ppvObject);
    
    if (riid == __uuidof(IDXGIPresentDevicePrivate))
      return m_presentDevice->QueryInterface(riid, ppvObject);

    if (riid == __uuidof(ID3D11Debug))
      return E_NOINTERFACE;      

    //d56e2a4c-5127-8437-658a-98c5bb789498, from GTA V, no occurrences in Google
    if (riid == GUID{0xd56e2a4c,0x5127,0x8437,{0x65,0x8a,0x98,0xc5,0xbb,0x78,0x94,0x98}})
      return E_NOINTERFACE;
    
    Logger::warn("D3D11Device::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
    
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateBuffer(
    const D3D11_BUFFER_DESC*      pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Buffer**          ppBuffer) {
    if (ppBuffer == nullptr)
      return S_FALSE;
    
    try {
      const Com<D3D11Buffer> buffer
        = new D3D11Buffer(this, pDesc);
      
      this->InitBuffer(buffer.ptr(), pInitialData);
      *ppBuffer = buffer.ref();
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture1D(
    const D3D11_TEXTURE1D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture1D**       ppTexture1D) {
    D3D11_COMMON_TEXTURE_DESC desc;
    desc.Width          = pDesc->Width;
    desc.Height         = 1;
    desc.Depth          = 1;
    desc.MipLevels      = pDesc->MipLevels;
    desc.ArraySize      = pDesc->ArraySize;
    desc.Format         = pDesc->Format;
    desc.SampleDesc     = DXGI_SAMPLE_DESC { 1, 0 };
    desc.Usage          = pDesc->Usage;
    desc.BindFlags      = pDesc->BindFlags;
    desc.CPUAccessFlags = pDesc->CPUAccessFlags;
    desc.MiscFlags      = pDesc->MiscFlags;
    
    if (FAILED(D3D11CommonTexture::NormalizeTextureProperties(&desc)))
      return E_INVALIDARG;
    
    if (ppTexture1D == nullptr)
      return S_FALSE;
    
    try {
      const Com<D3D11Texture1D> texture = new D3D11Texture1D(this, &desc);
      this->InitTexture(texture->GetCommonTexture()->GetImage(), pInitialData);
      *ppTexture1D = texture.ref();
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture2D(
    const D3D11_TEXTURE2D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture2D**       ppTexture2D) {
    D3D11_COMMON_TEXTURE_DESC desc;
    desc.Width          = pDesc->Width;
    desc.Height         = pDesc->Height;
    desc.Depth          = 1;
    desc.MipLevels      = pDesc->MipLevels;
    desc.ArraySize      = pDesc->ArraySize;
    desc.Format         = pDesc->Format;
    desc.SampleDesc     = pDesc->SampleDesc;
    desc.Usage          = pDesc->Usage;
    desc.BindFlags      = pDesc->BindFlags;
    desc.CPUAccessFlags = pDesc->CPUAccessFlags;
    desc.MiscFlags      = pDesc->MiscFlags;
    
    if (FAILED(D3D11CommonTexture::NormalizeTextureProperties(&desc)))
      return E_INVALIDARG;
    
    if (ppTexture2D == nullptr)
      return S_FALSE;
    
    try {
      const Com<D3D11Texture2D> texture = new D3D11Texture2D(this, &desc);
      this->InitTexture(texture->GetCommonTexture()->GetImage(), pInitialData);
      *ppTexture2D = texture.ref();
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture3D(
    const D3D11_TEXTURE3D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture3D**       ppTexture3D) {
    D3D11_COMMON_TEXTURE_DESC desc;
    desc.Width          = pDesc->Width;
    desc.Height         = pDesc->Height;
    desc.Depth          = pDesc->Depth;
    desc.MipLevels      = pDesc->MipLevels;
    desc.ArraySize      = 1;
    desc.Format         = pDesc->Format;
    desc.SampleDesc     = DXGI_SAMPLE_DESC { 1, 0 };
    desc.Usage          = pDesc->Usage;
    desc.BindFlags      = pDesc->BindFlags;
    desc.CPUAccessFlags = pDesc->CPUAccessFlags;
    desc.MiscFlags      = pDesc->MiscFlags;
    
    if (FAILED(D3D11CommonTexture::NormalizeTextureProperties(&desc)))
      return E_INVALIDARG;
    
    if (ppTexture3D == nullptr)
      return S_FALSE;
      
    try {
      const Com<D3D11Texture3D> texture = new D3D11Texture3D(this, &desc);
      this->InitTexture(texture->GetCommonTexture()->GetImage(), pInitialData);
      *ppTexture3D = texture.ref();
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView(
          ID3D11Resource*                   pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc,
          ID3D11ShaderResourceView**        ppSRView) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    // The description is optional. If omitted, we'll create
    // a view that covers all subresources of the image.
    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    
    if (pDesc == nullptr) {
      if (FAILED(GetShaderResourceViewDescFromResource(pResource, &desc)))
        return E_INVALIDARG;
    } else {
      desc = *pDesc;
      
      if (FAILED(SetShaderResourceViewDescUnspecValues(pResource, &desc)))
        return E_INVALIDARG;
    }
    
    if (resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      auto resource = static_cast<D3D11Buffer*>(pResource);
      
      D3D11_BUFFER_DESC resourceDesc;
      resource->GetDesc(&resourceDesc);
      
      if ((resourceDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0) {
        Logger::warn("D3D11: Trying to create SRV for buffer without D3D11_BIND_SHADER_RESOURCE");
        return E_INVALIDARG;
      }
      
      DxvkBufferViewCreateInfo viewInfo;
      
      D3D11_BUFFEREX_SRV bufInfo;
      
      if (desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX) {
        bufInfo.FirstElement = desc.BufferEx.FirstElement;
        bufInfo.NumElements  = desc.BufferEx.NumElements;
        bufInfo.Flags        = desc.BufferEx.Flags;
      } else if (desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER) {
        bufInfo.FirstElement = desc.Buffer.FirstElement;
        bufInfo.NumElements  = desc.Buffer.NumElements;
        bufInfo.Flags        = 0;
      } else {
        Logger::err("D3D11Device: Invalid buffer view dimension");
        return E_INVALIDARG;
      }
      
      if (bufInfo.Flags & D3D11_BUFFEREX_SRV_FLAG_RAW) {
        // Raw buffer view. We'll represent this as a
        // uniform texel buffer with UINT32 elements.
        viewInfo.format = VK_FORMAT_R32_UINT;
        viewInfo.rangeOffset = sizeof(uint32_t) * bufInfo.FirstElement;
        viewInfo.rangeLength = sizeof(uint32_t) * bufInfo.NumElements;
      } else if (desc.Format == DXGI_FORMAT_UNKNOWN) {
        // Structured buffer view
        viewInfo.format = VK_FORMAT_R32_UINT;
        viewInfo.rangeOffset = resourceDesc.StructureByteStride * bufInfo.FirstElement;
        viewInfo.rangeLength = resourceDesc.StructureByteStride * bufInfo.NumElements;
      } else {
        // Typed buffer view - must use an uncompressed color format
        viewInfo.format = m_dxgiAdapter->LookupFormat(
          desc.Format, DxgiFormatMode::Color).format;
        
        const DxvkFormatInfo* formatInfo = imageFormatInfo(viewInfo.format);
        viewInfo.rangeOffset = formatInfo->elementSize * bufInfo.FirstElement;
        viewInfo.rangeLength = formatInfo->elementSize * bufInfo.NumElements;
        
        if (formatInfo->flags.test(DxvkFormatFlag::BlockCompressed)) {
          Logger::err("D3D11Device: Compressed formats for buffer views not supported");
          return E_INVALIDARG;
        }
      }
      
      if (ppSRView == nullptr)
        return S_FALSE;
      
      try {
        *ppSRView = ref(new D3D11ShaderResourceView(
          this, pResource, desc,
          m_dxvkDevice->createBufferView(
            resource->GetBufferSlice().buffer(), viewInfo)));
        return S_OK;
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_FAIL;
      }
    } else {
      const D3D11CommonTexture* textureInfo = GetCommonTexture(pResource);
      
      if ((textureInfo->Desc()->BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0) {
        Logger::warn("D3D11: Trying to create SRV for texture without D3D11_BIND_SHADER_RESOURCE");
        return E_INVALIDARG;
      }
      
      // Fill in the view info. The view type depends solely
      // on the view dimension field in the view description,
      // not on the resource type.
      const DxgiFormatInfo formatInfo = m_dxgiAdapter
        ->LookupFormat(desc.Format, textureInfo->GetFormatMode());
      
      DxvkImageViewCreateInfo viewInfo;
      viewInfo.format  = formatInfo.format;
      viewInfo.aspect  = formatInfo.aspect;
      viewInfo.swizzle = formatInfo.swizzle;
      
      switch (desc.ViewDimension) {
        case D3D11_SRV_DIMENSION_TEXTURE1D:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_1D;
          viewInfo.minLevel  = desc.Texture1D.MostDetailedMip;
          viewInfo.numLevels = desc.Texture1D.MipLevels;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 1;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
          viewInfo.minLevel  = desc.Texture1DArray.MostDetailedMip;
          viewInfo.numLevels = desc.Texture1DArray.MipLevels;
          viewInfo.minLayer  = desc.Texture1DArray.FirstArraySlice;
          viewInfo.numLayers = desc.Texture1DArray.ArraySize;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURE2D:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_2D;
          viewInfo.minLevel  = desc.Texture2D.MostDetailedMip;
          viewInfo.numLevels = desc.Texture2D.MipLevels;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 1;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
          viewInfo.minLevel  = desc.Texture2DArray.MostDetailedMip;
          viewInfo.numLevels = desc.Texture2DArray.MipLevels;
          viewInfo.minLayer  = desc.Texture2DArray.FirstArraySlice;
          viewInfo.numLayers = desc.Texture2DArray.ArraySize;
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
          viewInfo.minLayer  = desc.Texture2DMSArray.FirstArraySlice;
          viewInfo.numLayers = desc.Texture2DMSArray.ArraySize;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURE3D:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_3D;
          viewInfo.minLevel  = desc.Texture3D.MostDetailedMip;
          viewInfo.numLevels = desc.Texture3D.MipLevels;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 1;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURECUBE:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
          viewInfo.minLevel  = desc.TextureCube.MostDetailedMip;
          viewInfo.numLevels = desc.TextureCube.MipLevels;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 6;
          break;
          
        case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
          viewInfo.minLevel  = desc.TextureCubeArray.MostDetailedMip;
          viewInfo.numLevels = desc.TextureCubeArray.MipLevels;
          viewInfo.minLayer  = desc.TextureCubeArray.First2DArrayFace;
          viewInfo.numLayers = desc.TextureCubeArray.NumCubes * 6;
          break;
          
        default:
          Logger::err(str::format(
            "D3D11: View dimension not supported for SRV: ",
            desc.ViewDimension));
          return E_INVALIDARG;
      }
      
      if (ppSRView == nullptr)
        return S_FALSE;
      
      try {
        *ppSRView = ref(new D3D11ShaderResourceView(
          this, pResource, desc,
          m_dxvkDevice->createImageView(
            textureInfo->GetImage(),
            viewInfo)));
        return S_OK;
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_FAIL;
      }
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView(
          ID3D11Resource*                   pResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
          ID3D11UnorderedAccessView**       ppUAView) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    // The description is optional. If omitted, we'll create
    // a view that covers all subresources of the image.
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    
    if (pDesc == nullptr) {
      if (FAILED(GetUnorderedAccessViewDescFromResource(pResource, &desc)))
        return E_INVALIDARG;
    } else {
      desc = *pDesc;
      
      if (FAILED(SetUnorderedAccessViewDescUnspecValues(pResource, &desc)))
        return E_INVALIDARG;
    }
    
    if (resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      auto resource = static_cast<D3D11Buffer*>(pResource);
      
      D3D11_BUFFER_DESC resourceDesc;
      resource->GetDesc(&resourceDesc);
      
      if ((resourceDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) == 0) {
        Logger::warn("D3D11: Trying to create UAV for buffer without D3D11_BIND_UNORDERED_ACCESS");
        return E_INVALIDARG;
      }
      
      DxvkBufferViewCreateInfo viewInfo;
      
      if (desc.Buffer.Flags & D3D11_BUFFEREX_SRV_FLAG_RAW) {
        viewInfo.format      = VK_FORMAT_R32_UINT;
        viewInfo.rangeOffset = sizeof(uint32_t) * desc.Buffer.FirstElement;
        viewInfo.rangeLength = sizeof(uint32_t) * desc.Buffer.NumElements;
      } else if (desc.Format == DXGI_FORMAT_UNKNOWN) {
        viewInfo.format      = VK_FORMAT_R32_UINT;
        viewInfo.rangeOffset = resourceDesc.StructureByteStride * desc.Buffer.FirstElement;
        viewInfo.rangeLength = resourceDesc.StructureByteStride * desc.Buffer.NumElements;
      } else {
        // Typed buffer view - must use an uncompressed color format
        viewInfo.format = m_dxgiAdapter->LookupFormat(
          desc.Format, DxgiFormatMode::Color).format;
        
        const DxvkFormatInfo* formatInfo = imageFormatInfo(viewInfo.format);
        viewInfo.rangeOffset = formatInfo->elementSize * desc.Buffer.FirstElement;
        viewInfo.rangeLength = formatInfo->elementSize * desc.Buffer.NumElements;
        
        if (formatInfo->flags.test(DxvkFormatFlag::BlockCompressed)) {
          Logger::err("D3D11Device: Compressed formats for buffer views not supported");
          return E_INVALIDARG;
        }
      }
      
      if (ppUAView == nullptr)
        return S_FALSE;
      
      try {
        // Fetch a buffer slice for atomic
        // append/consume functionality.
        DxvkBufferSlice counterSlice;
        
        if (desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER))
          counterSlice = AllocateCounterSlice();
        
        *ppUAView = ref(new D3D11UnorderedAccessView(
          this, pResource, desc,
          m_dxvkDevice->createBufferView(
            resource->GetBufferSlice().buffer(), viewInfo),
          counterSlice));
        return S_OK;
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_FAIL;
      }
    } else {
      const D3D11CommonTexture* textureInfo = GetCommonTexture(pResource);
      
      if ((textureInfo->Desc()->BindFlags & D3D11_BIND_UNORDERED_ACCESS) == 0) {
        Logger::warn("D3D11: Trying to create UAV for texture without D3D11_BIND_UNORDERED_ACCESS");
        return E_INVALIDARG;
      }
      
      // Fill in the view info. The view type depends solely
      // on the view dimension field in the view description,
      // not on the resource type.
      const DxgiFormatInfo formatInfo = m_dxgiAdapter
        ->LookupFormat(desc.Format, textureInfo->GetFormatMode());
      
      DxvkImageViewCreateInfo viewInfo;
      viewInfo.format  = formatInfo.format;
      viewInfo.aspect  = formatInfo.aspect;
      viewInfo.swizzle = formatInfo.swizzle;
      
      switch (desc.ViewDimension) {
        case D3D11_UAV_DIMENSION_TEXTURE1D:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_1D;
          viewInfo.minLevel  = desc.Texture1D.MipSlice;
          viewInfo.numLevels = 1;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 1;
          break;
          
        case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
          viewInfo.minLevel  = desc.Texture1DArray.MipSlice;
          viewInfo.numLevels = 1;
          viewInfo.minLayer  = desc.Texture1DArray.FirstArraySlice;
          viewInfo.numLayers = desc.Texture1DArray.ArraySize;
          break;
          
        case D3D11_UAV_DIMENSION_TEXTURE2D:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_2D;
          viewInfo.minLevel  = desc.Texture2D.MipSlice;
          viewInfo.numLevels = 1;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 1;
          break;
          
        case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
          viewInfo.minLevel  = desc.Texture2DArray.MipSlice;
          viewInfo.numLevels = 1;
          viewInfo.minLayer  = desc.Texture2DArray.FirstArraySlice;
          viewInfo.numLayers = desc.Texture2DArray.ArraySize;
          break;
          
        case D3D11_UAV_DIMENSION_TEXTURE3D:
          // FIXME we actually have to map this to a
          // 2D array view in order to support W slices
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_3D;
          viewInfo.minLevel  = desc.Texture3D.MipSlice;
          viewInfo.numLevels = 1;
          viewInfo.minLayer  = 0;
          viewInfo.numLayers = 1;
          break;
          
        default:
          Logger::err(str::format(
            "D3D11: View dimension not supported for UAV: ",
            desc.ViewDimension));
          return E_INVALIDARG;
      }
      
      if (ppUAView == nullptr)
        return S_FALSE;
      
      try {
        *ppUAView = ref(new D3D11UnorderedAccessView(
          this, pResource, desc,
          m_dxvkDevice->createImageView(
            textureInfo->GetImage(),
            viewInfo),
          DxvkBufferSlice()));
        return S_OK;
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_FAIL;
      }
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView(
          ID3D11Resource*                   pResource,
    const D3D11_RENDER_TARGET_VIEW_DESC*    pDesc,
          ID3D11RenderTargetView**          ppRTView) {
    // Only 2D textures and 2D texture arrays are allowed
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    // The view description is optional. If not defined, it
    // will use the resource's format and all array layers.
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    
    if (pDesc == nullptr) {
      if (FAILED(GetRenderTargetViewDescFromResource(pResource, &desc)))
        return E_INVALIDARG;
    } else {
      desc = *pDesc;
      
      if (FAILED(SetRenderTargetViewDescUnspecValues(pResource, &desc)))
        return E_INVALIDARG;
    }
    
    // Retrieve the image that we are going to create the view for
    const D3D11CommonTexture* textureInfo = GetCommonTexture(pResource);
    
    if ((textureInfo->Desc()->BindFlags & D3D11_BIND_RENDER_TARGET) == 0) {
      Logger::warn("D3D11: Trying to create RTV for texture without D3D11_BIND_RENDER_TARGET");
      return E_INVALIDARG;
    }
    
    // Fill in Vulkan image view info
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format = m_dxgiAdapter->LookupFormat(desc.Format, DxgiFormatMode::Color).format;
    viewInfo.aspect = imageFormatInfo(viewInfo.format)->aspectMask;
    
    switch (desc.ViewDimension) {
      case D3D11_RTV_DIMENSION_TEXTURE1D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_1D;
        viewInfo.minLevel   = desc.Texture1D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;
        
      case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        viewInfo.minLevel   = desc.Texture1DArray.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = desc.Texture1DArray.FirstArraySlice;
        viewInfo.numLayers  = desc.Texture1DArray.ArraySize;
        break;
        
      case D3D11_RTV_DIMENSION_TEXTURE2D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.minLevel   = desc.Texture2D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;
        
      case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.minLevel   = desc.Texture2DArray.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = desc.Texture2DArray.FirstArraySlice;
        viewInfo.numLayers  = desc.Texture2DArray.ArraySize;
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
        viewInfo.minLayer   = desc.Texture2DMSArray.FirstArraySlice;
        viewInfo.numLayers  = desc.Texture2DMSArray.ArraySize;
        break;
      
      case D3D11_RTV_DIMENSION_TEXTURE3D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.minLevel   = desc.Texture3D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = desc.Texture3D.FirstWSlice;
        viewInfo.numLayers  = desc.Texture3D.WSize;
        break;
      
      default:
        Logger::err(str::format(
          "D3D11: pDesc->ViewDimension not supported for render target views: ",
          desc.ViewDimension));
        return E_INVALIDARG;
    }
    
    // Create the actual image view if requested
    if (ppRTView == nullptr)
      return S_FALSE;
    
    try {
      *ppRTView = ref(new D3D11RenderTargetView(
        this, pResource, desc,
        m_dxvkDevice->createImageView(
          textureInfo->GetImage(),
          viewInfo)));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilView(
          ID3D11Resource*                   pResource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc,
          ID3D11DepthStencilView**          ppDepthStencilView) {
    // Only 2D textures and 2D texture arrays are allowed
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    // The view description is optional. If not defined, it
    // will use the resource's format and all array layers.
    D3D11_DEPTH_STENCIL_VIEW_DESC desc;
    
    if (pDesc == nullptr) {
      if (FAILED(GetDepthStencilViewDescFromResource(pResource, &desc)))
        return E_INVALIDARG;
    } else {
      desc = *pDesc;
      
      if (FAILED(SetDepthStencilViewDescUnspecValues(pResource, &desc)))
        return E_INVALIDARG;
    }
    
    // Retrieve the image that we are going to create the view for
    const D3D11CommonTexture* textureInfo = GetCommonTexture(pResource);
    
    if ((textureInfo->Desc()->BindFlags & D3D11_BIND_DEPTH_STENCIL) == 0) {
      Logger::warn("D3D11: Trying to create DSV for texture without D3D11_BIND_DEPTH_STENCIL");
      return E_INVALIDARG;
    }
    
    // Fill in Vulkan image view info
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format = m_dxgiAdapter->LookupFormat(desc.Format, DxgiFormatMode::Depth).format;
    viewInfo.aspect = imageFormatInfo(viewInfo.format)->aspectMask;
    
    switch (desc.ViewDimension) {
      case D3D11_DSV_DIMENSION_TEXTURE1D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_1D;
        viewInfo.minLevel   = desc.Texture1D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;
        
      case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        viewInfo.minLevel   = desc.Texture1DArray.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = desc.Texture1DArray.FirstArraySlice;
        viewInfo.numLayers  = desc.Texture1DArray.ArraySize;
        break;
        
      case D3D11_DSV_DIMENSION_TEXTURE2D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.minLevel   = desc.Texture2D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;
        
      case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.minLevel   = desc.Texture2DArray.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = desc.Texture2DArray.FirstArraySlice;
        viewInfo.numLayers  = desc.Texture2DArray.ArraySize;
        break;
        
      case D3D11_DSV_DIMENSION_TEXTURE2DMS:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.minLevel   = 0;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.minLevel   = 0;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = desc.Texture2DMSArray.FirstArraySlice;
        viewInfo.numLayers  = desc.Texture2DMSArray.ArraySize;
        break;
      
      default:
        Logger::err(str::format(
          "D3D11: pDesc->ViewDimension not supported for depth-stencil views: ",
          desc.ViewDimension));
        return E_INVALIDARG;
    }
    
    // Create the actual image view if requested
    if (ppDepthStencilView == nullptr)
      return S_FALSE;
    
    try {
      *ppDepthStencilView = ref(new D3D11DepthStencilView(
        this, pResource, desc,
        m_dxvkDevice->createImageView(
          textureInfo->GetImage(),
          viewInfo)));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC*   pInputElementDescs,
          UINT                        NumElements,
    const void*                       pShaderBytecodeWithInputSignature,
          SIZE_T                      BytecodeLength,
          ID3D11InputLayout**         ppInputLayout) {
    try {
      DxbcReader dxbcReader(reinterpret_cast<const char*>(
        pShaderBytecodeWithInputSignature), BytecodeLength);
      DxbcModule dxbcModule(dxbcReader);
      
      const Rc<DxbcIsgn> inputSignature = dxbcModule.isgn();
      
      std::vector<DxvkVertexAttribute> attributes;
      std::vector<DxvkVertexBinding>   bindings;
      
      for (uint32_t i = 0; i < NumElements; i++) {
        const DxbcSgnEntry* entry = inputSignature->find(
          pInputElementDescs[i].SemanticName,
          pInputElementDescs[i].SemanticIndex);
        
        if (entry == nullptr) {
          Logger::debug(str::format(
            "D3D11Device: No such vertex shader semantic: ",
            pInputElementDescs[i].SemanticName,
            pInputElementDescs[i].SemanticIndex));
          continue;
        }
        
        // Create vertex input attribute description
        DxvkVertexAttribute attrib;
        attrib.location = entry->registerId;
        attrib.binding  = pInputElementDescs[i].InputSlot;
        attrib.format   = m_dxgiAdapter->LookupFormat(
          pInputElementDescs[i].Format, DxgiFormatMode::Color).format;
        attrib.offset   = pInputElementDescs[i].AlignedByteOffset;
        
        // The application may choose to let the implementation
        // generate the exact vertex layout. In that case we'll
        // pack attributes on the same binding in the order they
        // are declared, aligning each attribute to four bytes.
        if (attrib.offset == D3D11_APPEND_ALIGNED_ELEMENT) {
          attrib.offset = 0;
          
          for (uint32_t j = 1; j <= i; j++) {
            const DxvkVertexAttribute& prev = attributes.at(i - j);
            
            if (prev.binding == attrib.binding) {
              const DxvkFormatInfo* formatInfo = imageFormatInfo(prev.format);
              attrib.offset = align(prev.offset + formatInfo->elementSize, 4);
              break;
            }
          }
        }
        
        attributes.push_back(attrib);
        
        // Create vertex input binding description. The
        // stride is dynamic state in D3D11 and will be
        // set by D3D11DeviceContext::IASetVertexBuffers.
        DxvkVertexBinding binding;
        binding.binding   = pInputElementDescs[i].InputSlot;
        binding.fetchRate = pInputElementDescs[i].InstanceDataStepRate;
        binding.inputRate = pInputElementDescs[i].InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA
          ? VK_VERTEX_INPUT_RATE_INSTANCE
          : VK_VERTEX_INPUT_RATE_VERTEX;
        
        // Check if the binding was already defined. If so, the
        // parameters must be identical (namely, the input rate).
        bool bindingDefined = false;
        
        for (const auto& existingBinding : bindings) {
          if (binding.binding == existingBinding.binding) {
            bindingDefined = true;
            
            if (binding.inputRate != existingBinding.inputRate) {
              Logger::err(str::format(
                "D3D11Device: Conflicting input rate for binding ",
                binding.binding));
              return E_INVALIDARG;
            }
          }
        }
        
        if (!bindingDefined)
          bindings.push_back(binding);
      }
      
      // Check if there are any semantics defined in the
      // shader that are not included in the current input
      // layout.
      for (auto i = inputSignature->begin(); i != inputSignature->end(); i++) {
        bool found = i->systemValue != DxbcSystemValue::None;
        
        for (uint32_t j = 0; j < attributes.size() && !found; j++)
          found = attributes.at(j).location == i->registerId;
        
        if (!found) {
          Logger::warn(str::format(
            "D3D11Device: Vertex input '",
            i->semanticName, i->semanticIndex,
            "' not defined by input layout"));
        }
      }
      
      std::sort(bindings.begin(), bindings.end(),
        [] (const DxvkVertexBinding& a, const DxvkVertexBinding& b) {
          return a.binding < b.binding;
        });
      
      // Create the actual input layout object
      // if the application requests it.
      if (ppInputLayout != nullptr) {
        *ppInputLayout = ref(
          new D3D11InputLayout(this,
            attributes.size(),
            attributes.data(),
            bindings.size(),
            bindings.data()));
      }
      
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateVertexShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11VertexShader**        ppVertexShader) {
    D3D11ShaderModule module;
    
    if (FAILED(this->CreateShaderModule(&module,
        pShaderBytecode, BytecodeLength, pClassLinkage)))
      return E_INVALIDARG;
    
    if (ppVertexShader == nullptr)
      return S_FALSE;
    
    *ppVertexShader = ref(new D3D11VertexShader(
      this, std::move(module)));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11GeometryShader**      ppGeometryShader) {
    D3D11ShaderModule module;
    
    if (FAILED(this->CreateShaderModule(&module,
        pShaderBytecode, BytecodeLength, pClassLinkage)))
      return E_INVALIDARG;
    
    if (ppGeometryShader == nullptr)
      return S_FALSE;
    
    *ppGeometryShader = ref(new D3D11GeometryShader(
      this, std::move(module)));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShaderWithStreamOutput(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
    const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
          UINT                        NumEntries,
    const UINT*                       pBufferStrides,
          UINT                        NumStrides,
          UINT                        RasterizedStream,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11GeometryShader**      ppGeometryShader) {
    Logger::err("D3D11Device::CreateGeometryShaderWithStreamOutput: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreatePixelShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11PixelShader**         ppPixelShader) {
    D3D11ShaderModule module;
    
    if (FAILED(this->CreateShaderModule(&module,
        pShaderBytecode, BytecodeLength, pClassLinkage)))
      return E_INVALIDARG;
    
    if (ppPixelShader == nullptr)
      return S_FALSE;
    
    *ppPixelShader = ref(new D3D11PixelShader(
      this, std::move(module)));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateHullShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11HullShader**          ppHullShader) {
    D3D11ShaderModule module;
    
    if (FAILED(this->CreateShaderModule(&module,
        pShaderBytecode, BytecodeLength, pClassLinkage)))
      return E_INVALIDARG;
    
    if (ppHullShader == nullptr)
      return S_FALSE;
    
    *ppHullShader = ref(new D3D11HullShader(
      this, std::move(module)));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDomainShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11DomainShader**        ppDomainShader) {
    D3D11ShaderModule module;
    
    if (FAILED(this->CreateShaderModule(&module,
        pShaderBytecode, BytecodeLength, pClassLinkage)))
      return E_INVALIDARG;
    
    if (ppDomainShader == nullptr)
      return S_FALSE;
    
    *ppDomainShader = ref(new D3D11DomainShader(
      this, std::move(module)));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateComputeShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11ComputeShader**       ppComputeShader) {
    D3D11ShaderModule module;
    
    if (FAILED(this->CreateShaderModule(&module,
        pShaderBytecode, BytecodeLength, pClassLinkage)))
      return E_INVALIDARG;
    
    if (ppComputeShader == nullptr)
      return S_FALSE;
    
    *ppComputeShader = ref(new D3D11ComputeShader(
      this, std::move(module)));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateClassLinkage(ID3D11ClassLinkage** ppLinkage) {
    *ppLinkage = ref(new D3D11ClassLinkage(this));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateBlendState(
    const D3D11_BLEND_DESC*           pBlendStateDesc,
          ID3D11BlendState**          ppBlendState) {
    D3D11_BLEND_DESC desc;
    
    if (pBlendStateDesc != nullptr) {
      desc = *pBlendStateDesc;
    } else {
      desc.AlphaToCoverageEnable  = FALSE;
      desc.IndependentBlendEnable = FALSE;
      
      // 1-7 must be ignored if IndependentBlendEnable is disabled so
      // technically this is not needed, but since this structure is
      // going to be copied around we'll initialize it nonetheless
      for (uint32_t i = 0; i < 8; i++) {
        desc.RenderTarget[i].BlendEnable           = FALSE;
        desc.RenderTarget[i].SrcBlend              = D3D11_BLEND_ONE;
        desc.RenderTarget[i].DestBlend             = D3D11_BLEND_ZERO;
        desc.RenderTarget[i].BlendOp               = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[i].SrcBlendAlpha         = D3D11_BLEND_ONE;
        desc.RenderTarget[i].DestBlendAlpha        = D3D11_BLEND_ZERO;
        desc.RenderTarget[i].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
      }
    }
    
    if (ppBlendState != nullptr) {
      *ppBlendState = m_bsStateObjects.Create(this, desc);
      return S_OK;
    } return S_FALSE;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilState(
    const D3D11_DEPTH_STENCIL_DESC*   pDepthStencilDesc,
          ID3D11DepthStencilState**   ppDepthStencilState) {
    D3D11_DEPTH_STENCIL_DESC desc;
    
    if (pDepthStencilDesc != nullptr) {
      desc = *pDepthStencilDesc;
    } else {
      D3D11_DEPTH_STENCILOP_DESC stencilOp;
      stencilOp.StencilFunc        = D3D11_COMPARISON_ALWAYS;
      stencilOp.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
      stencilOp.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
      stencilOp.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
      
      desc.DepthEnable      = TRUE;
      desc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
      desc.DepthFunc        = D3D11_COMPARISON_LESS;
      desc.StencilEnable    = FALSE;
      desc.StencilReadMask  = D3D11_DEFAULT_STENCIL_READ_MASK;
      desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
      desc.FrontFace        = stencilOp;
      desc.BackFace         = stencilOp;
    }
    
    if (ppDepthStencilState != nullptr) {
      *ppDepthStencilState = m_dsStateObjects.Create(this, desc);
      return S_OK;
    } return S_FALSE;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateRasterizerState(
    const D3D11_RASTERIZER_DESC*      pRasterizerDesc,
          ID3D11RasterizerState**     ppRasterizerState) {
    D3D11_RASTERIZER_DESC desc;
    
    if (pRasterizerDesc != nullptr) {
      desc = *pRasterizerDesc;
    } else {
      desc.FillMode = D3D11_FILL_SOLID;
      desc.CullMode = D3D11_CULL_BACK;
      desc.FrontCounterClockwise = FALSE;
      desc.DepthBias = 0;
      desc.SlopeScaledDepthBias = 0.0f;
      desc.DepthBiasClamp = 0.0f;
      desc.DepthClipEnable = TRUE;
      desc.ScissorEnable = FALSE;
      desc.MultisampleEnable = FALSE;
      desc.AntialiasedLineEnable = FALSE;
    }
    
    if (ppRasterizerState != nullptr) {
      *ppRasterizerState = m_rsStateObjects.Create(this, desc);
      return S_OK;
    } return S_FALSE;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateSamplerState(
    const D3D11_SAMPLER_DESC*         pSamplerDesc,
          ID3D11SamplerState**        ppSamplerState) {
    HRESULT hr = D3D11SamplerState::ValidateDesc(pSamplerDesc);
    
    if (FAILED(hr))
      return hr;
    
    if (ppSamplerState == nullptr)
      return S_FALSE;
    
    try {
      *ppSamplerState = m_samplerObjects.Create(this, *pSamplerDesc);
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateQuery(
    const D3D11_QUERY_DESC*           pQueryDesc,
          ID3D11Query**               ppQuery) {
    if (pQueryDesc->Query != D3D11_QUERY_EVENT
     && pQueryDesc->Query != D3D11_QUERY_OCCLUSION
     && pQueryDesc->Query != D3D11_QUERY_TIMESTAMP
     && pQueryDesc->Query != D3D11_QUERY_TIMESTAMP_DISJOINT
     && pQueryDesc->Query != D3D11_QUERY_PIPELINE_STATISTICS
     && pQueryDesc->Query != D3D11_QUERY_OCCLUSION_PREDICATE) {
      Logger::warn(str::format("D3D11Query: Unsupported query type ", pQueryDesc->Query));
      return E_INVALIDARG;
    }
    
    if (ppQuery == nullptr)
      return S_FALSE;
    
    try {
      *ppQuery = ref(new D3D11Query(this, *pQueryDesc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreatePredicate(
    const D3D11_QUERY_DESC*           pPredicateDesc,
          ID3D11Predicate**           ppPredicate) {
    if (pPredicateDesc->Query != D3D11_QUERY_OCCLUSION_PREDICATE)
      return E_INVALIDARG;
    
    if (ppPredicate == nullptr)
      return S_FALSE;
    
    try {
      *ppPredicate = ref(new D3D11Query(this, *pPredicateDesc));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateCounter(
    const D3D11_COUNTER_DESC*         pCounterDesc,
          ID3D11Counter**             ppCounter) {
    Logger::err("D3D11Device::CreateCounter: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext(
          UINT                        ContextFlags,
          ID3D11DeviceContext**       ppDeferredContext) {
    *ppDeferredContext = ref(new D3D11DeferredContext(this, m_dxvkDevice, ContextFlags));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource(
          HANDLE      hResource,
          REFIID      ReturnedInterface,
          void**      ppResource) {
    Logger::err("D3D11Device::OpenSharedResource: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CheckFormatSupport(
          DXGI_FORMAT Format,
          UINT*       pFormatSupport) {
    return GetFormatSupportFlags(Format, pFormatSupport);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CheckMultisampleQualityLevels(
          DXGI_FORMAT Format,
          UINT        SampleCount,
          UINT*       pNumQualityLevels) {
    // There are many error conditions, so we'll just assume
    // that we will fail and return a non-zero value in case
    // the device does actually support the format.
    *pNumQualityLevels = 0;
    
    // We need to check whether the format is 
    VkFormat format = m_dxgiAdapter->LookupFormat(
      Format, DxgiFormatMode::Any).format;
    
    if (format == VK_FORMAT_UNDEFINED) {
      Logger::err(str::format("D3D11: Unsupported format: ", Format));
      return E_INVALIDARG;
    }
    
    // D3D may legally query non-power-of-two sample counts as well
    VkSampleCountFlagBits sampleCountFlag = VK_SAMPLE_COUNT_1_BIT;
    
    if (FAILED(GetSampleCount(SampleCount, &sampleCountFlag)))
      return E_INVALIDARG;
    
    // Check if the device supports the given combination of format
    // and sample count. D3D exposes the opaque concept of quality
    // levels to the application, we'll just define one such level.
    VkImageFormatProperties formatProps;
    
    VkResult status = m_dxvkAdapter->imageFormatProperties(
      format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_SAMPLED_BIT, 0, formatProps);
    
    if ((status == VK_SUCCESS) && (formatProps.sampleCounts & sampleCountFlag))
      *pNumQualityLevels = 1;
    return S_OK;
  }
  
  
  void STDMETHODCALLTYPE D3D11Device::CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo) {
    Logger::err("D3D11Device::CheckCounterInfo: Not implemented");
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CheckCounter(
    const D3D11_COUNTER_DESC* pDesc,
          D3D11_COUNTER_TYPE* pType,
          UINT*               pActiveCounters,
          LPSTR               szName,
          UINT*               pNameLength,
          LPSTR               szUnits,
          UINT*               pUnitsLength,
          LPSTR               szDescription,
          UINT*               pDescriptionLength) {
    Logger::err("D3D11Device::CheckCounter: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::CheckFeatureSupport(
          D3D11_FEATURE Feature,
          void*         pFeatureSupportData,
          UINT          FeatureSupportDataSize) {
    switch (Feature) {
      case D3D11_FEATURE_THREADING: {
        if (FeatureSupportDataSize != sizeof(D3D11_FEATURE_DATA_THREADING))
          return E_INVALIDARG;
        
        auto info = static_cast<D3D11_FEATURE_DATA_THREADING*>(pFeatureSupportData);
        info->DriverConcurrentCreates = TRUE;
        info->DriverCommandLists      = FALSE;
      } return S_OK;
      
      case D3D11_FEATURE_DOUBLES: {
        if (FeatureSupportDataSize != sizeof(D3D11_FEATURE_DATA_DOUBLES))
          return E_INVALIDARG;
        
        auto info = static_cast<D3D11_FEATURE_DATA_DOUBLES*>(pFeatureSupportData);
        info->DoublePrecisionFloatShaderOps = FALSE;
      } return S_OK;
      
      case D3D11_FEATURE_FORMAT_SUPPORT: {
        if (FeatureSupportDataSize != sizeof(D3D11_FEATURE_DATA_FORMAT_SUPPORT))
          return E_INVALIDARG;
        
        auto info = static_cast<D3D11_FEATURE_DATA_FORMAT_SUPPORT*>(pFeatureSupportData);
        return GetFormatSupportFlags(info->InFormat, &info->OutFormatSupport);
      } return S_OK;
      
      case D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS: {
        if (FeatureSupportDataSize != sizeof(D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS))
          return E_INVALIDARG;
        
        auto info = static_cast<D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS*>(pFeatureSupportData);
        info->ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x = TRUE;
      } return S_OK;
      
      default:
        Logger::err(str::format(
          "D3D11Device: CheckFeatureSupport: Unknown feature: ",
          Feature));
        return E_INVALIDARG;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::GetPrivateData(
          REFGUID guid, UINT* pDataSize, void* pData) {
    return m_dxgiDevice->GetPrivateData(guid, pDataSize, pData);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateData(
          REFGUID guid, UINT DataSize, const void* pData) {
    return m_dxgiDevice->SetPrivateData(guid, DataSize, pData);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateDataInterface(
          REFGUID guid, const IUnknown* pData) {
    return m_dxgiDevice->SetPrivateDataInterface(guid, pData);
  }
  
  
  D3D_FEATURE_LEVEL STDMETHODCALLTYPE D3D11Device::GetFeatureLevel() {
    return m_featureLevel;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Device::GetCreationFlags() {
    return m_featureFlags;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::GetDeviceRemovedReason() {
    static std::atomic<bool> s_errorShown = { false };
    
    if (!s_errorShown.exchange(true))
      Logger::warn("D3D11Device::GetDeviceRemovedReason: Stub");
    
    return S_OK;
  }
  
  
  void STDMETHODCALLTYPE D3D11Device::GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) {
    *ppImmediateContext = ref(m_context);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Device::SetExceptionMode(UINT RaiseFlags) {
    Logger::err("D3D11Device::SetExceptionMode: Not implemented");
    return E_NOTIMPL;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Device::GetExceptionMode() {
    Logger::err("D3D11Device::GetExceptionMode: Not implemented");
    return 0;
  }
  
  
  DxgiFormatInfo STDMETHODCALLTYPE D3D11Device::LookupFormat(
          DXGI_FORMAT           format,
          DxgiFormatMode        mode) const {
    return m_dxgiAdapter->LookupFormat(format, mode);
  }
  
  
  DxvkBufferSlice D3D11Device::AllocateCounterSlice() {
    std::lock_guard<std::mutex> lock(m_counterMutex);
    
    if (m_counterSlices.size() == 0)
      throw DxvkError("D3D11Device: Failed to allocate counter slice");
    
    uint32_t sliceId = m_counterSlices.back();
    m_counterSlices.pop_back();
    
    return DxvkBufferSlice(m_counterBuffer,
      sizeof(D3D11UavCounter) * sliceId,
      sizeof(D3D11UavCounter));
  }
  
  
  void D3D11Device::FreeCounterSlice(const DxvkBufferSlice& Slice) {
    std::lock_guard<std::mutex> lock(m_counterMutex);
    m_counterSlices.push_back(Slice.offset() / sizeof(D3D11UavCounter));
  }
  
  
  void D3D11Device::FlushInitContext() {
    LockResourceInitContext();
    if (m_resourceInitCommands != 0)
      SubmitResourceInitCommands();
    UnlockResourceInitContext(0);
  }
  
  
  VkPipelineStageFlags D3D11Device::GetEnabledShaderStages() const {
    VkPipelineStageFlags enabledShaderPipelineStages
      = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
      | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
      | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    
    if (m_dxvkDevice->features().geometryShader)
      enabledShaderPipelineStages |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    
    if (m_dxvkDevice->features().tessellationShader) {
      enabledShaderPipelineStages |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
                                  |  VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    }
    
    return enabledShaderPipelineStages;
  }
  
  
  bool D3D11Device::CheckFeatureLevelSupport(
    const Rc<DxvkAdapter>&  adapter,
          D3D_FEATURE_LEVEL featureLevel) {
    // We currently only support 11_0 interfaces
    if (featureLevel > GetMaxFeatureLevel())
      return false;
    
    // Check whether all features are supported
    const VkPhysicalDeviceFeatures features
      = GetDeviceFeatures(adapter, featureLevel);
    
    if (!adapter->checkFeatureSupport(features))
      return false;
    
    // TODO also check for required limits
    return true;
  }
  
  
  VkPhysicalDeviceFeatures D3D11Device::GetDeviceFeatures(
    const Rc<DxvkAdapter>&  adapter,
          D3D_FEATURE_LEVEL featureLevel) {
    VkPhysicalDeviceFeatures supported = adapter->features();
    VkPhysicalDeviceFeatures enabled   = {};
    
    if (featureLevel >= D3D_FEATURE_LEVEL_9_1) {
      enabled.depthClamp                            = VK_TRUE;
      enabled.depthBiasClamp                        = VK_TRUE;
      enabled.fillModeNonSolid                      = VK_TRUE;
      enabled.pipelineStatisticsQuery               = supported.pipelineStatisticsQuery;
      enabled.sampleRateShading                     = VK_TRUE;
      enabled.samplerAnisotropy                     = VK_TRUE;
      enabled.shaderClipDistance                    = VK_TRUE;
      enabled.shaderCullDistance                    = VK_TRUE;
      enabled.robustBufferAccess                    = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_9_2) {
      enabled.occlusionQueryPrecise                 = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_9_3) {
      enabled.multiViewport                         = VK_TRUE;
      enabled.independentBlend                      = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_10_0) {
      enabled.fullDrawIndexUint32                   = VK_TRUE;
      enabled.fragmentStoresAndAtomics              = VK_TRUE;
      enabled.geometryShader                        = VK_TRUE;
      enabled.logicOp                               = supported.logicOp;
      enabled.shaderImageGatherExtended             = VK_TRUE;
      enabled.textureCompressionBC                  = VK_TRUE;
      enabled.vertexPipelineStoresAndAtomics        = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_10_1) {
      enabled.dualSrcBlend                          = VK_TRUE;
      enabled.imageCubeArray                        = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_11_0) {
      enabled.shaderFloat64                         = supported.shaderFloat64;
      enabled.shaderInt64                           = supported.shaderInt64;
      enabled.tessellationShader                    = VK_TRUE;
      enabled.shaderStorageImageReadWithoutFormat   = VK_TRUE;
      enabled.shaderStorageImageWriteWithoutFormat  = VK_TRUE;
    }
    
    return enabled;
  }
  
  
  HRESULT D3D11Device::CreateShaderModule(
          D3D11ShaderModule*      pShaderModule,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
          ID3D11ClassLinkage*     pClassLinkage) {
    if (pClassLinkage != nullptr)
      Logger::warn("D3D11Device::CreateShaderModule: Class linkage not supported");
    
    try {
      *pShaderModule = D3D11ShaderModule(
        &m_dxbcOptions, this, pShaderBytecode, BytecodeLength);
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
  
  
  void D3D11Device::InitBuffer(
          D3D11Buffer*                pBuffer,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    const DxvkBufferSlice bufferSlice
      = pBuffer->GetBufferSlice();
    
    if (pInitialData != nullptr && pInitialData->pSysMem != nullptr) {
      LockResourceInitContext();
      
      m_resourceInitContext->updateBuffer(
        bufferSlice.buffer(),
        bufferSlice.offset(),
        bufferSlice.length(),
        pInitialData->pSysMem);
      
      UnlockResourceInitContext(1);
    }
  }
  
  
  void D3D11Device::InitTexture(
    const Rc<DxvkImage>&              image,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    const DxvkFormatInfo* formatInfo = imageFormatInfo(image->info().format);
    
    if (pInitialData != nullptr && pInitialData->pSysMem != nullptr) {
      LockResourceInitContext();
      
      // pInitialData is an array that stores an entry for
      // every single subresource. Since we will define all
      // subresources, this counts as initialization.
      VkImageSubresourceLayers subresourceLayers;
      subresourceLayers.aspectMask     = formatInfo->aspectMask;
      subresourceLayers.mipLevel       = 0;
      subresourceLayers.baseArrayLayer = 0;
      subresourceLayers.layerCount     = 1;
      
      for (uint32_t layer = 0; layer < image->info().numLayers; layer++) {
        for (uint32_t level = 0; level < image->info().mipLevels; level++) {
          subresourceLayers.baseArrayLayer = layer;
          subresourceLayers.mipLevel       = level;
          
          const uint32_t id = D3D11CalcSubresource(
            level, layer, image->info().mipLevels);
          
          m_resourceInitContext->updateImage(
            image, subresourceLayers,
            VkOffset3D { 0, 0, 0 },
            image->mipLevelExtent(level),
            pInitialData[id].pSysMem,
            pInitialData[id].SysMemPitch,
            pInitialData[id].SysMemSlicePitch);
        }
      }
      
      const uint32_t subresourceCount =
        image->info().numLayers * image->info().mipLevels;
      UnlockResourceInitContext(subresourceCount);
    } else {
      LockResourceInitContext();
      
      // While the Microsoft docs state that resource contents are
      // undefined if no initial data is provided, some applications
      // expect a resource to be pre-cleared. We can only do that
      // for non-compressed images, but that should be fine.
      VkImageSubresourceRange subresources;
      subresources.aspectMask     = formatInfo->aspectMask;
      subresources.baseMipLevel   = 0;
      subresources.levelCount     = image->info().mipLevels;
      subresources.baseArrayLayer = 0;
      subresources.layerCount     = image->info().numLayers;
      
      const DxvkFormatInfo* formatInfo = imageFormatInfo(image->info().format);
      
      if (formatInfo->flags.test(DxvkFormatFlag::BlockCompressed)) {
        m_resourceInitContext->initImage(
          image, subresources);
      } else {
        if (subresources.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT) {
          m_resourceInitContext->clearColorImage(
            image, VkClearColorValue(), subresources);
        } else {
          VkClearDepthStencilValue value;
          value.depth   = 1.0f;
          value.stencil = 0;
          
          m_resourceInitContext->clearDepthStencilImage(
            image, value, subresources);
        }
      }
      
      UnlockResourceInitContext(1);
    }
  }
  
  
  HRESULT D3D11Device::GetShaderResourceViewDescFromResource(
          ID3D11Resource*                   pResource,
          D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc) {
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
          } else {
            pDesc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            pDesc->Texture2DArray.MostDetailedMip = 0;
            pDesc->Texture2DArray.MipLevels       = resourceDesc.MipLevels;
            pDesc->Texture2DArray.FirstArraySlice = 0;
            pDesc->Texture2DArray.ArraySize       = resourceDesc.ArraySize;
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
  
  
  HRESULT D3D11Device::GetUnorderedAccessViewDescFromResource(
          ID3D11Resource*                   pResource,
          D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_BUFFER: {
        D3D11_BUFFER_DESC bufferDesc;
        static_cast<D3D11Buffer*>(pResource)->GetDesc(&bufferDesc);
        
        if (bufferDesc.MiscFlags == D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
          pDesc->Format              = DXGI_FORMAT_UNKNOWN;
          pDesc->ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
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
          pDesc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
          pDesc->Texture1D.MipSlice = 0;
        } else {
          pDesc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
          pDesc->Texture1DArray.MipSlice        = 0;
          pDesc->Texture1DArray.FirstArraySlice = 0;
          pDesc->Texture1DArray.ArraySize       = resourceDesc.ArraySize;
        }
      } return S_OK;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D11Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.ArraySize == 1) {
          pDesc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
          pDesc->Texture2D.MipSlice = 0;
        } else {
          pDesc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
          pDesc->Texture2DArray.MipSlice        = 0;
          pDesc->Texture2DArray.FirstArraySlice = 0;
          pDesc->Texture2DArray.ArraySize       = resourceDesc.ArraySize;
        }
      } return S_OK;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        D3D11_TEXTURE3D_DESC resourceDesc;
        static_cast<D3D11Texture3D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format        = resourceDesc.Format;
        pDesc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
        pDesc->Texture3D.MipSlice = 0;
      } return S_OK;
      
      default:
        Logger::err(str::format(
          "D3D11: Unsupported dimension for unordered access view: ",
          resourceDim));
        return E_INVALIDARG;
    }
  }
  
  
  HRESULT D3D11Device::GetRenderTargetViewDescFromResource(
          ID3D11Resource*                   pResource,
          D3D11_RENDER_TARGET_VIEW_DESC*    pDesc) {
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
            pDesc->Texture2D.MipSlice = 0;
          } else {
            pDesc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            pDesc->Texture2DArray.MipSlice        = 0;
            pDesc->Texture2DArray.FirstArraySlice = 0;
            pDesc->Texture2DArray.ArraySize       = resourceDesc.ArraySize;
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
  
  
  HRESULT D3D11Device::GetDepthStencilViewDescFromResource(
          ID3D11Resource*                   pResource,
          D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.ArraySize == 1) {
          pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
          pDesc->Texture1D.MipSlice = 0;
        } else {
          pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
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
            pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            pDesc->Texture2D.MipSlice = 0;
          } else {
            pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            pDesc->Texture2DArray.MipSlice        = 0;
            pDesc->Texture2DArray.FirstArraySlice = 0;
            pDesc->Texture2DArray.ArraySize       = resourceDesc.ArraySize;
          }
        } else {
          if (resourceDesc.ArraySize == 1) {
            pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
          } else {
            pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
            pDesc->Texture2DMSArray.FirstArraySlice = 0;
            pDesc->Texture2DMSArray.ArraySize       = resourceDesc.ArraySize;
          }
        }
      } return S_OK;
        
      default:
        Logger::err(str::format(
          "D3D11: Unsupported dimension for depth stencil view: ",
          resourceDim));
        return E_INVALIDARG;
    }
  }
  
  
  HRESULT D3D11Device::SetShaderResourceViewDescUnspecValues(
          ID3D11Resource*                   pResource,
          D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc) {
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
      case D3D11_SRV_DIMENSION_TEXTURE1D:
        if (pDesc->Texture1D.MipLevels == D3D11_DXVK_USE_REMAINING_LEVELS)
          pDesc->Texture1D.MipLevels = mipLevels - pDesc->Texture1D.MostDetailedMip;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
        if (pDesc->Texture1DArray.MipLevels == D3D11_DXVK_USE_REMAINING_LEVELS)
          pDesc->Texture1DArray.MipLevels = mipLevels - pDesc->Texture1DArray.MostDetailedMip;
        if (pDesc->Texture1DArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture1DArray.ArraySize = numLayers - pDesc->Texture1DArray.FirstArraySlice;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURE2D:
        if (pDesc->Texture2D.MipLevels == D3D11_DXVK_USE_REMAINING_LEVELS)
          pDesc->Texture2D.MipLevels = mipLevels - pDesc->Texture2D.MostDetailedMip;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
        if (pDesc->Texture2DArray.MipLevels == D3D11_DXVK_USE_REMAINING_LEVELS)
          pDesc->Texture2DArray.MipLevels = mipLevels - pDesc->Texture2DArray.MostDetailedMip;
        if (pDesc->Texture2DArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture2DArray.ArraySize = numLayers - pDesc->Texture2DArray.FirstArraySlice;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
        if (pDesc->Texture2DMSArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture2DMSArray.ArraySize = numLayers - pDesc->Texture2DMSArray.FirstArraySlice;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURECUBE:
        if (pDesc->TextureCube.MipLevels == D3D11_DXVK_USE_REMAINING_LEVELS)
          pDesc->TextureCube.MipLevels = mipLevels - pDesc->TextureCube.MostDetailedMip;
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
        if (pDesc->TextureCubeArray.MipLevels == D3D11_DXVK_USE_REMAINING_LEVELS)
          pDesc->TextureCubeArray.MipLevels = mipLevels - pDesc->TextureCubeArray.MostDetailedMip;
        if (pDesc->TextureCubeArray.NumCubes == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->TextureCubeArray.NumCubes = (numLayers - pDesc->TextureCubeArray.First2DArrayFace / 6);
        break;
      
      case D3D11_SRV_DIMENSION_TEXTURE3D:
        if (pDesc->Texture3D.MipLevels == D3D11_DXVK_USE_REMAINING_LEVELS)
          pDesc->Texture3D.MipLevels = mipLevels - pDesc->Texture3D.MostDetailedMip;
        break;
      
      default:
        break;
    }
    
    return S_OK;
  }
  
  
  HRESULT D3D11Device::SetUnorderedAccessViewDescUnspecValues(
          ID3D11Resource*                   pResource,
          D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t numLayers = 0;
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_BUFFER: {
        if (pDesc->ViewDimension != D3D11_UAV_DIMENSION_BUFFER) {
          Logger::err("D3D11: Incompatible view dimension for Buffer");
          return E_INVALIDARG;
        }
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE1D
         && pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE1DARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture1D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D11Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE2D
         && pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE2DARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture2D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        D3D11_TEXTURE3D_DESC resourceDesc;
        static_cast<D3D11Texture3D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE3D) {
          Logger::err("D3D11: Incompatible view dimension for Texture3D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.Depth >> pDesc->Texture3D.MipSlice;
      } break;
      
      default:
        return E_INVALIDARG;
    }
    
    if (pDesc->Format == DXGI_FORMAT_UNKNOWN)
      pDesc->Format = format;
    
    switch (pDesc->ViewDimension) {
      case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
        if (pDesc->Texture1DArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture1DArray.ArraySize = numLayers - pDesc->Texture1DArray.FirstArraySlice;
        break;
      
      case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
        if (pDesc->Texture2DArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture2DArray.ArraySize = numLayers - pDesc->Texture2DArray.FirstArraySlice;
        break;
      
      case D3D11_UAV_DIMENSION_TEXTURE3D:
        if (pDesc->Texture3D.WSize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture3D.WSize = numLayers - pDesc->Texture3D.FirstWSlice;
        break;
      
      default:
        break;
    }
    
    return S_OK;
  }
  
  
  HRESULT D3D11Device::SetRenderTargetViewDescUnspecValues(
          ID3D11Resource*                   pResource,
          D3D11_RENDER_TARGET_VIEW_DESC*    pDesc) {
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
        numLayers = resourceDesc.Depth;
      } break;
      
      default:
        return E_INVALIDARG;
    }
    
    if (pDesc->Format == DXGI_FORMAT_UNKNOWN)
      pDesc->Format = format;
    
    switch (pDesc->ViewDimension) {
      case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
        if (pDesc->Texture1DArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture1DArray.ArraySize = numLayers - pDesc->Texture1DArray.FirstArraySlice;
        break;
      
      case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
        if (pDesc->Texture2DArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture2DArray.ArraySize = numLayers - pDesc->Texture2DArray.FirstArraySlice;
        break;
      
      case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
        if (pDesc->Texture2DMSArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture2DMSArray.ArraySize = numLayers - pDesc->Texture2DMSArray.FirstArraySlice;
        break;
      
      case D3D11_RTV_DIMENSION_TEXTURE3D:
        if (pDesc->Texture3D.WSize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture3D.WSize = numLayers - pDesc->Texture3D.FirstWSlice;
        break;
      
      default:
        break;
    }
    
    return S_OK;
  }
  
  
  HRESULT D3D11Device::SetDepthStencilViewDescUnspecValues(
          ID3D11Resource*                   pResource,
          D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t numLayers = 0;
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE1D
         && pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE1DARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture1D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D11Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2D
         && pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2DARRAY
         && pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2DMS
         && pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture2D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      default:
        return E_INVALIDARG;
    }
    
    if (pDesc->Format == DXGI_FORMAT_UNKNOWN)
      pDesc->Format = format;
    
    switch (pDesc->ViewDimension) {
      case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
        if (pDesc->Texture1DArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture1DArray.ArraySize = numLayers - pDesc->Texture1DArray.FirstArraySlice;
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
        if (pDesc->Texture2DArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture2DArray.ArraySize = numLayers - pDesc->Texture2DArray.FirstArraySlice;
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
        if (pDesc->Texture2DMSArray.ArraySize == D3D11_DXVK_USE_REMAINING_LAYERS)
          pDesc->Texture2DMSArray.ArraySize = numLayers - pDesc->Texture2DMSArray.FirstArraySlice;
        break;
      
      default:
        break;
    }
    
    return S_OK;
  }
  
  
  HRESULT D3D11Device::GetFormatSupportFlags(DXGI_FORMAT Format, UINT* pFlags) const {
    const VkFormat fmt = m_dxgiAdapter->LookupFormat(Format, DxgiFormatMode::Any).format;
    const VkFormatProperties fmtInfo = m_dxvkAdapter->formatProperties(fmt);
    
    if (fmt == VK_FORMAT_UNDEFINED)
      return E_FAIL;
    
    UINT flags = 0;
    
    if (fmtInfo.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT)
      flags |= D3D11_FORMAT_SUPPORT_BUFFER;
    
    if (fmtInfo.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)
      flags |= D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER;
    
    if (Format == DXGI_FORMAT_R16_UINT || Format == DXGI_FORMAT_R32_UINT)
      flags |= D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER;
    
    // TODO implement stream output
    // D3D11_FORMAT_SUPPORT_SO_BUFFER
    
    if (fmtInfo.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
      flags |= D3D11_FORMAT_SUPPORT_TEXTURE1D
            |  D3D11_FORMAT_SUPPORT_TEXTURE2D
            |  D3D11_FORMAT_SUPPORT_TEXTURE3D
            |  D3D11_FORMAT_SUPPORT_TEXTURECUBE
            |  D3D11_FORMAT_SUPPORT_SHADER_LOAD
            |  D3D11_FORMAT_SUPPORT_SHADER_GATHER
            |  D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON
            |  D3D11_FORMAT_SUPPORT_SHADER_SAMPLE
            |  D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON
            |  D3D11_FORMAT_SUPPORT_MIP
            |  D3D11_FORMAT_SUPPORT_MIP_AUTOGEN
            |  D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE
            |  D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT;
    }
    
    if (fmtInfo.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
      flags |= D3D11_FORMAT_SUPPORT_RENDER_TARGET;
    
    if (fmtInfo.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
      flags |= D3D11_FORMAT_SUPPORT_BLENDABLE;
    
    if (fmtInfo.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
      flags |= D3D11_FORMAT_SUPPORT_DEPTH_STENCIL;
    
    if (fmtInfo.optimalTilingFeatures)
      flags |= D3D11_FORMAT_SUPPORT_CPU_LOCKABLE;
    
    if ((fmtInfo.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT)
     || (fmtInfo.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
      flags |= D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW;
    
    // FIXME implement properly. This would require a VkSurface.
    if (Format == DXGI_FORMAT_R8G8B8A8_UNORM
     || Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
     || Format == DXGI_FORMAT_B8G8R8A8_UNORM
     || Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
     || Format == DXGI_FORMAT_R16G16B16A16_FLOAT
     || Format == DXGI_FORMAT_R10G10B10A2_UNORM
     || Format == DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM)
      flags |= D3D11_FORMAT_SUPPORT_DISPLAY;
    
    // Query multisampling info
    VkImageFormatProperties imgInfo;
    
    VkResult status = m_dxvkAdapter->imageFormatProperties(fmt,
      VK_IMAGE_TYPE_2D,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      0, imgInfo);
    
    if (status == VK_SUCCESS && imgInfo.sampleCounts > VK_SAMPLE_COUNT_1_BIT) {
      flags |= D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET
            |  D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD;
    }
    
    *pFlags = flags;
    return S_OK;
  }
  
  
  void D3D11Device::CreateCounterBuffer() {
    const uint32_t MaxCounterStructs = 1 << 16;
    
    // The counter buffer is used as a storage buffer
    DxvkBufferCreateInfo info;
    info.size       = MaxCounterStructs * sizeof(D3D11UavCounter);
    info.usage      = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                    | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                    | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.stages     = VK_PIPELINE_STAGE_TRANSFER_BIT
                    | GetEnabledShaderStages();
    info.access     = VK_ACCESS_TRANSFER_READ_BIT
                    | VK_ACCESS_TRANSFER_WRITE_BIT
                    | VK_ACCESS_SHADER_READ_BIT
                    | VK_ACCESS_SHADER_WRITE_BIT;
    m_counterBuffer = m_dxvkDevice->createBuffer(
      info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    // Init the counter struct allocator as well
    m_counterSlices.resize(MaxCounterStructs);
    
    for (uint32_t i = 0; i < MaxCounterStructs; i++)
      m_counterSlices[i] = MaxCounterStructs - i - 1;
  }
  
  
  void D3D11Device::LockResourceInitContext() {
    m_resourceInitMutex.lock();
  }
  
  
  void D3D11Device::UnlockResourceInitContext(uint64_t CommandCount) {
    m_resourceInitCommands += CommandCount;
    
    if (m_resourceInitCommands >= InitCommandThreshold)
      SubmitResourceInitCommands();
    
    m_resourceInitMutex.unlock();
  }
  
  
  void D3D11Device::SubmitResourceInitCommands() {
    m_dxvkDevice->submitCommandList(
      m_resourceInitContext->endRecording(),
      nullptr, nullptr);
    
    m_resourceInitContext->beginRecording(
      m_dxvkDevice->createCommandList());
    
    m_resourceInitCommands = 0;
  }
  
  
  D3D_FEATURE_LEVEL D3D11Device::GetMaxFeatureLevel() {
    static const std::array<std::pair<std::string, D3D_FEATURE_LEVEL>, 6> s_featureLevels = {{
      { "11_0", D3D_FEATURE_LEVEL_11_0 },
      { "10_1", D3D_FEATURE_LEVEL_10_1 },
      { "10_0", D3D_FEATURE_LEVEL_10_0 },
      { "9_3",  D3D_FEATURE_LEVEL_9_3  },
      { "9_2",  D3D_FEATURE_LEVEL_9_2  },
      { "9_1",  D3D_FEATURE_LEVEL_9_1  },
    }};
    
    const std::string maxLevel = env::getEnvVar(L"DXVK_FEATURE_LEVEL");
    
    auto entry = std::find_if(s_featureLevels.begin(), s_featureLevels.end(),
      [&] (const std::pair<std::string, D3D_FEATURE_LEVEL>& pair) {
        return pair.first == maxLevel;
      });
    
    return entry != s_featureLevels.end()
      ? entry->second
      : D3D_FEATURE_LEVEL_11_0;
    
  }
  
}
