#include <cstring>

#include "d3d11_buffer.h"
#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_input_layout.h"
#include "d3d11_present.h"
#include "d3d11_sampler.h"
#include "d3d11_shader.h"
#include "d3d11_texture.h"
#include "d3d11_view.h"

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
    m_dxvkAdapter   (m_dxvkDevice->adapter()) {
    Com<IDXGIAdapter> adapter;
    
    if (FAILED(m_dxgiDevice->GetAdapter(&adapter))
     || FAILED(adapter->QueryInterface(__uuidof(IDXGIAdapterPrivate),
          reinterpret_cast<void**>(&m_dxgiAdapter))))
      throw DxvkError("D3D11Device: Failed to query adapter");
    
    m_dxgiDevice->SetDeviceLayer(this);
    m_presentDevice->SetDeviceLayer(this);
    
    m_context = new D3D11DeviceContext(this, m_dxvkDevice);
    m_resourceInitContext = m_dxvkDevice->createContext();
  }
  
  
  D3D11Device::~D3D11Device() {
    m_presentDevice->SetDeviceLayer(nullptr);
    m_dxgiDevice->SetDeviceLayer(nullptr);
  }
  
  
  HRESULT D3D11Device::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Device);
    
    if (riid == __uuidof(IDXGIDevice)
     || riid == __uuidof(IDXGIDevice1)
     || riid == __uuidof(IDXGIDevicePrivate))
      return m_dxgiDevice->QueryInterface(riid, ppvObject);
    
    if (riid == __uuidof(IDXGIPresentDevicePrivate))
      return m_presentDevice->QueryInterface(riid, ppvObject);
    
    Logger::warn("D3D11Device::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
    
  
  HRESULT D3D11Device::CreateBuffer(
    const D3D11_BUFFER_DESC*      pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Buffer**          ppBuffer) {
    
    // Gather usage information
    DxvkBufferCreateInfo  info;
    info.size   = pDesc->ByteWidth;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT;
    
    if (pDesc->BindFlags & D3D11_BIND_VERTEX_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_INDEX_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_INDEX_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_CONSTANT_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      info.stages |= GetEnabledShaderStages();
      info.access |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_SHADER_RESOURCE) {
      info.usage  |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                  |  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
      info.stages |= GetEnabledShaderStages();
      info.access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_STREAM_OUTPUT) {
      Logger::err("D3D11Device::CreateBuffer: D3D11_BIND_STREAM_OUTPUT not supported");
      return E_INVALIDARG;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
      info.usage  |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                  |  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                  |  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      info.access |= VK_ACCESS_SHADER_READ_BIT
                  |  VK_ACCESS_SHADER_WRITE_BIT;
    }
    
    if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_WRITE_BIT;
    }
    
    if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_READ) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_READ_BIT;
    }
    
    if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) {
      info.usage  |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
      info.access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    
    if (ppBuffer != nullptr) {
      Com<IDXGIBufferResourcePrivate> buffer;
      
      HRESULT hr = DXGICreateBufferResourcePrivate(
        m_dxgiDevice.ptr(), &info,
        GetMemoryFlagsForUsage(pDesc->Usage), 0,
        &buffer);
      
      if (FAILED(hr))
        return hr;
      
      *ppBuffer = ref(new D3D11Buffer(
        this, buffer.ptr(), *pDesc));
      
      InitBuffer(buffer.ptr(), pInitialData);
    }
    
    return S_OK;
  }
  
  
  HRESULT D3D11Device::CreateTexture1D(
    const D3D11_TEXTURE1D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture1D**       ppTexture1D) {
    Logger::err("D3D11Device::CreateTexture1D: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateTexture2D(
    const D3D11_TEXTURE2D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture2D**       ppTexture2D) {
    DxvkImageCreateInfo info;
    info.type           = VK_IMAGE_TYPE_2D;
    info.format         = m_dxgiAdapter->LookupFormat(pDesc->Format).actual;
    info.flags          = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    info.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
    info.extent.width   = pDesc->Width;
    info.extent.height  = pDesc->Height;
    info.extent.depth   = 1;
    info.numLayers      = pDesc->ArraySize;
    info.mipLevels      = pDesc->MipLevels;
    info.usage          = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.stages         = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access         = VK_ACCESS_TRANSFER_READ_BIT
                        | VK_ACCESS_TRANSFER_WRITE_BIT;
    info.tiling         = VK_IMAGE_TILING_OPTIMAL;
    info.layout         = VK_IMAGE_LAYOUT_GENERAL;
    
    if (FAILED(GetSampleCount(pDesc->SampleDesc.Count, &info.sampleCount))) {
      Logger::err(str::format("D3D11: Invalid sample count: ", pDesc->SampleDesc.Count));
      return E_INVALIDARG;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_SHADER_RESOURCE) {
      info.usage  |= VK_IMAGE_USAGE_SAMPLED_BIT;
      info.stages |= GetEnabledShaderStages();
      info.access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_RENDER_TARGET) {
      info.usage  |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      info.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      info.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                  |  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_DEPTH_STENCIL) {
      info.usage  |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      info.stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                  |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      info.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                  |  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
      info.usage  |= VK_IMAGE_USAGE_STORAGE_BIT;
      info.stages |= GetEnabledShaderStages();
      info.access |= VK_ACCESS_SHADER_READ_BIT
                  |  VK_ACCESS_SHADER_WRITE_BIT;
    }
    
    if (pDesc->CPUAccessFlags != 0) {
      info.tiling  = VK_IMAGE_TILING_LINEAR;
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      
      if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
        info.access |= VK_ACCESS_HOST_WRITE_BIT;
      
      if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_READ)
        info.access |= VK_ACCESS_HOST_READ_BIT;
    }
    
    if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
      info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    
    if (ppTexture2D != nullptr) {
      Com<IDXGIImageResourcePrivate> image;
      
      HRESULT hr = DXGICreateImageResourcePrivate(
        m_dxgiDevice.ptr(), &info,
        GetMemoryFlagsForUsage(pDesc->Usage), 0,
        &image);
      
      if (FAILED(hr))
        return hr;
      
      *ppTexture2D = ref(new D3D11Texture2D(
        this, image.ptr(), *pDesc));
      
      InitTexture(image.ptr(), pInitialData);
    }
    
    return S_OK;
  }
  
  
  HRESULT D3D11Device::CreateTexture3D(
    const D3D11_TEXTURE3D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
          ID3D11Texture3D**       ppTexture3D) {
    Logger::err("D3D11Device::CreateTexture3D: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateShaderResourceView(
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
    }
    
    if (resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      Logger::err("D3D11: Shader resource views for buffers not yet supported");
      return E_INVALIDARG;
    } else {
      // Retrieve the image that we are going to create a view for
      Com<IDXGIImageResourcePrivate> imageResource;
      
      if (FAILED(pResource->QueryInterface(
            __uuidof(IDXGIImageResourcePrivate),
            reinterpret_cast<void**>(&imageResource))))
        return E_INVALIDARG;
      
      // Fill in the view info. The view type depends solely
      // on the view dimension field in the view description,
      // not on the resource type.
      DxvkImageViewCreateInfo viewInfo;
      viewInfo.format = m_dxgiAdapter->LookupFormat(desc.Format).actual;
      viewInfo.aspect = imageFormatInfo(viewInfo.format)->aspectMask;
      
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
          viewInfo.type      = VK_IMAGE_VIEW_TYPE_CUBE;
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
        return S_OK;
      
      try {
        *ppSRView = ref(new D3D11ShaderResourceView(
          this, pResource, desc, nullptr,
          m_dxvkDevice->createImageView(
            imageResource->GetDXVKImage(),
            viewInfo)));
        return S_OK;
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_FAIL;
      }
    }
  }
  
  
  HRESULT D3D11Device::CreateUnorderedAccessView(
          ID3D11Resource*                   pResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
          ID3D11UnorderedAccessView**       ppUAView) {
    Logger::err("D3D11Device::CreateUnorderedAccessView: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateRenderTargetView(
          ID3D11Resource*                   pResource,
    const D3D11_RENDER_TARGET_VIEW_DESC*    pDesc,
          ID3D11RenderTargetView**          ppRTView) {
    // Only 2D textures and 2D texture arrays are allowed
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    if (resourceDim != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
      Logger::err("D3D11: Unsupported resource type for render target views");
      return E_INVALIDARG;
    }
    
    // The view description is optional. If not defined, it
    // will use the resource's format and all array layers.
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    
    if (pDesc == nullptr) {
      if (FAILED(GetRenderTargetViewDescFromResource(pResource, &desc)))
        return E_INVALIDARG;
    } else {
      desc = *pDesc;
    }
    
    // Retrieve the image that we are going to create the view for
    Com<IDXGIImageResourcePrivate> imageResource;
    
    if (FAILED(pResource->QueryInterface(
        __uuidof(IDXGIImageResourcePrivate),
        reinterpret_cast<void**>(&imageResource))))
      return E_INVALIDARG;
    
    // Fill in Vulkan image view info
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format = m_dxgiAdapter->LookupFormat(desc.Format).actual;
    viewInfo.aspect = imageFormatInfo(viewInfo.format)->aspectMask;
    
    switch (desc.ViewDimension) {
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
        viewInfo.minLayer   = desc.Texture2DArray.FirstArraySlice;
        viewInfo.numLayers  = desc.Texture2DArray.ArraySize;
        break;
      
      default:
        Logger::err(str::format(
          "D3D11: pDesc->ViewDimension not supported for render target views: ",
          desc.ViewDimension));
        return E_INVALIDARG;
    }
    
    // Create the actual image view if requested
    if (ppRTView == nullptr)
      return S_OK;
    
    try {
      *ppRTView = ref(new D3D11RenderTargetView(
        this, pResource, desc, nullptr,
        m_dxvkDevice->createImageView(
          imageResource->GetDXVKImage(),
          viewInfo)));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
  }
  
  
  HRESULT D3D11Device::CreateDepthStencilView(
          ID3D11Resource*                   pResource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc,
          ID3D11DepthStencilView**          ppDepthStencilView) {
    // Only 2D textures and 2D texture arrays are allowed
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    if (resourceDim != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
      Logger::err("D3D11: Unsupported resource type for depth-stencil views");
      return E_INVALIDARG;
    }
    
    // The view description is optional. If not defined, it
    // will use the resource's format and all array layers.
    D3D11_DEPTH_STENCIL_VIEW_DESC desc;
    
    if (pDesc == nullptr) {
      if (FAILED(GetDepthStencilViewDescFromResource(pResource, &desc)))
        return E_INVALIDARG;
    } else {
      desc = *pDesc;
    }
    
    // Retrieve the image that we are going to create the view for
    Com<IDXGIImageResourcePrivate> imageResource;
    
    if (FAILED(pResource->QueryInterface(
        __uuidof(IDXGIImageResourcePrivate),
        reinterpret_cast<void**>(&imageResource))))
      return E_INVALIDARG;
    
    // Fill in Vulkan image view info
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format = m_dxgiAdapter->LookupFormat(desc.Format).actual;
    viewInfo.aspect = imageFormatInfo(viewInfo.format)->aspectMask;
    
    switch (desc.ViewDimension) {
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
        viewInfo.minLayer   = desc.Texture2DArray.FirstArraySlice;
        viewInfo.numLayers  = desc.Texture2DArray.ArraySize;
        break;
      
      default:
        Logger::err(str::format(
          "D3D11: pDesc->ViewDimension not supported for depth-stencil views: ",
          desc.ViewDimension));
        return E_INVALIDARG;
    }
    
    // Create the actual image view if requested
    if (ppDepthStencilView == nullptr)
      return S_OK;
    
    try {
      *ppDepthStencilView = ref(new D3D11DepthStencilView(
        this, pResource, desc, nullptr,
        m_dxvkDevice->createImageView(
          imageResource->GetDXVKImage(),
          viewInfo)));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
  }
  
  
  HRESULT D3D11Device::CreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC*   pInputElementDescs,
          UINT                        NumElements,
    const void*                       pShaderBytecodeWithInputSignature,
          SIZE_T                      BytecodeLength,
          ID3D11InputLayout**         ppInputLayout) {
    try {
      DxbcReader dxbcReader(reinterpret_cast<const char*>(
        pShaderBytecodeWithInputSignature), BytecodeLength);
      DxbcModule dxbcModule(dxbcReader);
      
      Rc<DxbcIsgn> inputSignature = dxbcModule.isgn();
      
      std::vector<DxvkVertexAttribute> attributes;
      std::vector<DxvkVertexBinding>   bindings;
      
      for (uint32_t i = 0; i < NumElements; i++) {
        const DxbcSgnEntry* entry = inputSignature->find(
          pInputElementDescs[i].SemanticName,
          pInputElementDescs[i].SemanticIndex);
        
        if (entry == nullptr) {
          Logger::err(str::format(
            "D3D11Device::CreateInputLayout: No such semantic: ",
            pInputElementDescs[i].SemanticName,
            pInputElementDescs[i].SemanticIndex));
          return E_INVALIDARG;
        }
        
        // Create vertex input attribute description
        DxvkVertexAttribute attrib;
        attrib.location = entry->registerId;
        attrib.binding  = pInputElementDescs[i].InputSlot;
        attrib.format   = m_dxgiAdapter->LookupFormat(
          pInputElementDescs[i].Format).actual;
        attrib.offset   = pInputElementDescs[i].AlignedByteOffset;
        
        // TODO implement D3D11_APPEND_ALIGNED_ELEMENT
        if (attrib.offset == D3D11_APPEND_ALIGNED_ELEMENT) {
          Logger::err("D3D11Device::CreateInputLayout: D3D11_APPEND_ALIGNED_ELEMENT not supported yet");
          return E_INVALIDARG;
        }
        
        attributes.push_back(attrib);
        
        // Create vertex input binding description. The
        // stride is dynamic state in D3D11 and will be
        // set by D3D11DeviceContext::IASetVertexBuffers.
        DxvkVertexBinding binding;
        binding.binding   = pInputElementDescs[i].InputSlot;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        if (pInputElementDescs[i].InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA) {
          binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
          
          if (pInputElementDescs[i].InstanceDataStepRate != 1) {
            Logger::err(str::format(
              "D3D11Device::CreateInputLayout: Unsupported instance data step rate: ",
              pInputElementDescs[i].InstanceDataStepRate));
          }
        }
        
        // Check if the binding was already defined. If so, the
        // parameters must be identical (namely, the input rate).
        bool bindingDefined = false;
        
        for (const auto& existingBinding : bindings) {
          if (binding.binding == existingBinding.binding) {
            bindingDefined = true;
            
            if (binding.inputRate != existingBinding.inputRate) {
              Logger::err(str::format(
                "D3D11Device::CreateInputLayout: Conflicting input rate for binding ",
                binding.binding));
              return E_INVALIDARG;
            }
          }
        }
        
        if (!bindingDefined)
          bindings.push_back(binding);
      }
      
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
      return E_INVALIDARG;
    }
  }
  
  
  HRESULT D3D11Device::CreateVertexShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11VertexShader**        ppVertexShader) {
    D3D11ShaderModule module;
    
    if (FAILED(this->CreateShaderModule(&module,
        pShaderBytecode, BytecodeLength, pClassLinkage)))
      return E_INVALIDARG;
    
    if (ppVertexShader != nullptr) {
      *ppVertexShader = ref(new D3D11VertexShader(
        this, std::move(module)));
    }
    
    return S_OK;
  }
  
  
  HRESULT D3D11Device::CreateGeometryShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11GeometryShader**      ppGeometryShader) {
    Logger::err("D3D11Device::CreateGeometryShader: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateGeometryShaderWithStreamOutput(
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
  
  
  HRESULT D3D11Device::CreatePixelShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11PixelShader**         ppPixelShader) {
    D3D11ShaderModule module;
    
    if (FAILED(this->CreateShaderModule(&module,
        pShaderBytecode, BytecodeLength, pClassLinkage)))
      return E_INVALIDARG;
    
    if (ppPixelShader != nullptr) {
      *ppPixelShader = ref(new D3D11PixelShader(
        this, std::move(module)));
    }
    
    return S_OK;
  }
  
  
  HRESULT D3D11Device::CreateHullShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11HullShader**          ppHullShader) {
    Logger::err("D3D11Device::CreateHullShader: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateDomainShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11DomainShader**        ppDomainShader) {
    Logger::err("D3D11Device::CreateDomainShader: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateComputeShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11ComputeShader**       ppComputeShader) {
    Logger::err("D3D11Device::CreateComputeShader: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateClassLinkage(ID3D11ClassLinkage** ppLinkage) {
    Logger::err("D3D11Device::CreateClassLinkage: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateBlendState(
    const D3D11_BLEND_DESC*           pBlendStateDesc,
          ID3D11BlendState**          ppBlendState) {
    Logger::err("D3D11Device::CreateBlendState: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateDepthStencilState(
    const D3D11_DEPTH_STENCIL_DESC*   pDepthStencilDesc,
          ID3D11DepthStencilState**   ppDepthStencilState) {
    Logger::err("D3D11Device::CreateDepthStencilState: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateRasterizerState(
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
    
    if (ppRasterizerState != nullptr)
      *ppRasterizerState = m_rsStateObjects.Create(this, desc);
    return S_OK;
  }
  
  
  HRESULT D3D11Device::CreateSamplerState(
    const D3D11_SAMPLER_DESC*         pSamplerDesc,
          ID3D11SamplerState**        ppSamplerState) {
    DxvkSamplerCreateInfo info;
    
    // While D3D11_FILTER is technically an enum, its value bits
    // can be used to decode the filter properties more efficiently.
    const uint32_t filterBits = static_cast<uint32_t>(pSamplerDesc->Filter);
    
    info.magFilter      = (filterBits & 0x04) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    info.minFilter      = (filterBits & 0x10) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    info.mipmapMode     = (filterBits & 0x01) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.useAnisotropy  = (filterBits & 0x40) ? VK_TRUE : VK_FALSE;
    info.compareToDepth = (filterBits & 0x80) ? VK_TRUE : VK_FALSE;
    
    // Check for any unknown flags
    if (filterBits & 0xFFFFFF2A) {
      Logger::err(str::format("D3D11: Unsupported filter bits: ", filterBits));
      return E_INVALIDARG;
    }
    
    // Set up the remaining properties, which are
    // stored directly in the sampler description
    info.mipmapLodBias = pSamplerDesc->MipLODBias;
    info.mipmapLodMin  = pSamplerDesc->MinLOD;
    info.mipmapLodMax  = pSamplerDesc->MaxLOD;
    info.maxAnisotropy = pSamplerDesc->MaxAnisotropy;
    info.addressModeU  = DecodeAddressMode(pSamplerDesc->AddressU);
    info.addressModeV  = DecodeAddressMode(pSamplerDesc->AddressV);
    info.addressModeW  = DecodeAddressMode(pSamplerDesc->AddressW);
    info.compareOp     = DecodeCompareOp(pSamplerDesc->ComparisonFunc);
    info.borderColor   = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    info.usePixelCoord = VK_FALSE;
    
    if (info.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
     || info.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
     || info.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
      Logger::warn("D3D11: Border color not supported yet");
    
    // Create sampler object if the application requests it
    if (ppSamplerState == nullptr)
      return S_OK;
    
    try {
      *ppSamplerState = ref(new D3D11SamplerState(this,
        *pSamplerDesc, m_dxvkDevice->createSampler(info)));
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return S_OK;
    }
  }
  
  
  HRESULT D3D11Device::CreateQuery(
    const D3D11_QUERY_DESC*           pQueryDesc,
          ID3D11Query**               ppQuery) {
    Logger::err("D3D11Device::CreateQuery: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreatePredicate(
    const D3D11_QUERY_DESC*           pPredicateDesc,
          ID3D11Predicate**           ppPredicate) {
    Logger::err("D3D11Device::CreatePredicate: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateCounter(
    const D3D11_COUNTER_DESC*         pCounterDesc,
          ID3D11Counter**             ppCounter) {
    Logger::err("D3D11Device::CreateCounter: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateDeferredContext(
          UINT                        ContextFlags,
          ID3D11DeviceContext**       ppDeferredContext) {
    Logger::err("D3D11Device::CreateDeferredContext: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::OpenSharedResource(
          HANDLE      hResource,
          REFIID      ReturnedInterface,
          void**      ppResource) {
    Logger::err("D3D11Device::OpenSharedResource: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CheckFormatSupport(
          DXGI_FORMAT Format,
          UINT*       pFormatSupport) {
    Logger::err("D3D11Device::CheckFormatSupport: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CheckMultisampleQualityLevels(
          DXGI_FORMAT Format,
          UINT        SampleCount,
          UINT*       pNumQualityLevels) {
    // There are many error conditions, so we'll just assume
    // that we will fail and return a non-zero value in case
    // the device does actually support the format.
    *pNumQualityLevels = 0;
    
    // We need to check whether the format is 
    VkFormat format = m_dxgiAdapter->LookupFormat(Format).actual;
    
    if (format == VK_FORMAT_UNDEFINED) {
      Logger::err(str::format("D3D11: Unsupported format: ", Format));
      return E_INVALIDARG;
    }
    
    // D3D may legally query non-power-of-two sample counts as well
    VkSampleCountFlagBits sampleCountFlag = VK_SAMPLE_COUNT_1_BIT;
    
    if (FAILED(GetSampleCount(SampleCount, &sampleCountFlag)))
      return S_OK;
    
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
  
  
  void D3D11Device::CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo) {
    Logger::err("D3D11Device::CheckCounterInfo: Not implemented");
  }
  
  
  HRESULT D3D11Device::CheckCounter(
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
  
  
  HRESULT D3D11Device::CheckFeatureSupport(
          D3D11_FEATURE Feature,
          void*         pFeatureSupportData,
          UINT          FeatureSupportDataSize) {
    Logger::err("D3D11Device::CheckFeatureSupport: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::GetPrivateData(
          REFGUID guid, UINT* pDataSize, void* pData) {
    return m_dxgiDevice->GetPrivateData(guid, pDataSize, pData);
  }
  
  
  HRESULT D3D11Device::SetPrivateData(
          REFGUID guid, UINT DataSize, const void* pData) {
    return m_dxgiDevice->SetPrivateData(guid, DataSize, pData);
  }
  
  
  HRESULT D3D11Device::SetPrivateDataInterface(
          REFGUID guid, const IUnknown* pData) {
    return m_dxgiDevice->SetPrivateDataInterface(guid, pData);
  }
  
  
  D3D_FEATURE_LEVEL D3D11Device::GetFeatureLevel() {
    return m_featureLevel;
  }
  
  
  UINT D3D11Device::GetCreationFlags() {
    return m_featureFlags;
  }
  
  
  HRESULT D3D11Device::GetDeviceRemovedReason() {
    Logger::err("D3D11Device::GetDeviceRemovedReason: Not implemented");
    return E_NOTIMPL;
  }
  
  
  void D3D11Device::GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) {
    *ppImmediateContext = m_context.ref();
  }
  
  
  HRESULT D3D11Device::SetExceptionMode(UINT RaiseFlags) {
    Logger::err("D3D11Device::SetExceptionMode: Not implemented");
    return E_NOTIMPL;
  }
  
  
  UINT D3D11Device::GetExceptionMode() {
    Logger::err("D3D11Device::GetExceptionMode: Not implemented");
    return 0;
  }
  
  
  bool D3D11Device::CheckFeatureLevelSupport(
    const Rc<DxvkAdapter>&  adapter,
          D3D_FEATURE_LEVEL featureLevel) {
    // We currently only support 11_0 interfaces
    if (featureLevel > D3D_FEATURE_LEVEL_11_0)
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
    VkPhysicalDeviceFeatures enabled;
    std::memset(&enabled, 0, sizeof(enabled));
    
    if (featureLevel >= D3D_FEATURE_LEVEL_9_1) {
      enabled.depthClamp                      = VK_TRUE;
      enabled.depthBiasClamp                  = VK_TRUE;
      enabled.depthBounds                     = VK_TRUE;
      enabled.fillModeNonSolid                = VK_TRUE;
      enabled.pipelineStatisticsQuery         = supported.pipelineStatisticsQuery;
      enabled.samplerAnisotropy               = VK_TRUE;
      enabled.shaderClipDistance              = VK_TRUE;
      enabled.shaderCullDistance              = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_9_2) {
      enabled.occlusionQueryPrecise           = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_9_3) {
      enabled.multiViewport                   = VK_TRUE;
      enabled.independentBlend                = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_10_0) {
      enabled.fullDrawIndexUint32             = VK_TRUE;
      enabled.fragmentStoresAndAtomics        = VK_TRUE;
      enabled.geometryShader                  = VK_TRUE;
      enabled.logicOp                         = supported.logicOp;
      enabled.shaderImageGatherExtended       = VK_TRUE;
      enabled.textureCompressionBC            = VK_TRUE;
      enabled.vertexPipelineStoresAndAtomics  = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_10_1) {
      enabled.imageCubeArray                  = VK_TRUE;
    }
    
    if (featureLevel >= D3D_FEATURE_LEVEL_11_0) {
      enabled.shaderFloat64                   = supported.shaderFloat64;
      enabled.shaderInt64                     = supported.shaderInt64;
      enabled.tessellationShader              = VK_TRUE;
      enabled.variableMultisampleRate         = VK_TRUE;
    }
    
    return enabled;
  }
  
  
  HRESULT D3D11Device::CreateShaderModule(
          D3D11ShaderModule*      pShaderModule,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
          ID3D11ClassLinkage*     pClassLinkage) {
    if (pClassLinkage != nullptr) {
      Logger::err("D3D11Device::CreateShaderModule: Class linkage not supported");
      return E_INVALIDARG;
    }
    
    try {
      *pShaderModule = D3D11ShaderModule(
        this, pShaderBytecode, BytecodeLength);
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
  }
  
  
  void D3D11Device::InitBuffer(
          IDXGIBufferResourcePrivate* pBuffer,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    const Rc<DxvkBuffer> buffer = pBuffer->GetDXVKBuffer();
    
    if (pInitialData != nullptr) {
      std::lock_guard<std::mutex> lock(m_resourceInitMutex);;
      m_resourceInitContext->beginRecording(
        m_dxvkDevice->createCommandList());
      m_resourceInitContext->updateBuffer(
        buffer, 0, buffer->info().size,
        pInitialData->pSysMem);
      m_dxvkDevice->submitCommandList(
        m_resourceInitContext->endRecording(),
        nullptr, nullptr);
    }
  }
  
  
  void D3D11Device::InitTexture(
          IDXGIImageResourcePrivate*  pImage,
    const D3D11_SUBRESOURCE_DATA*     pInitialData) {
    std::lock_guard<std::mutex> lock(m_resourceInitMutex);;
    m_resourceInitContext->beginRecording(
      m_dxvkDevice->createCommandList());
    
    const Rc<DxvkImage> image = pImage->GetDXVKImage();
    
    VkImageSubresourceRange subresources;
    subresources.aspectMask     = imageFormatInfo(image->info().format)->aspectMask;
    subresources.baseMipLevel   = 0;
    subresources.levelCount     = image->info().mipLevels;
    subresources.baseArrayLayer = 0;
    subresources.layerCount     = image->info().numLayers;
    m_resourceInitContext->initImage(image, subresources);
    
    if (pInitialData != nullptr) {
      VkImageSubresourceLayers subresourceLayers;
      subresourceLayers.aspectMask     = subresources.aspectMask;
      subresourceLayers.mipLevel       = 0;
      subresourceLayers.baseArrayLayer = 0;
      subresourceLayers.layerCount     = subresources.layerCount;
      
      m_resourceInitContext->updateImage(
        image, subresourceLayers,
        VkOffset3D { 0, 0, 0 },
        image->info().extent,
        pInitialData->pSysMem,
        pInitialData->SysMemPitch,
        pInitialData->SysMemSlicePitch);
    }
    
    m_dxvkDevice->submitCommandList(
      m_resourceInitContext->endRecording(),
      nullptr, nullptr);
  }
  
  
  HRESULT D3D11Device::GetShaderResourceViewDescFromResource(
          ID3D11Resource*                   pResource,
          D3D11_SHADER_RESOURCE_VIEW_DESC*  pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    switch (resourceDim) {
      
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
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::GetRenderTargetViewDescFromResource(
          ID3D11Resource*                   pResource,
          D3D11_RENDER_TARGET_VIEW_DESC*    pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    switch (resourceDim) {
      
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
  
  
  HRESULT D3D11Device::GetSampleCount(UINT Count, VkSampleCountFlagBits* pCount) const {
    switch (Count) {
      case  1: *pCount = VK_SAMPLE_COUNT_1_BIT;  return S_OK;
      case  2: *pCount = VK_SAMPLE_COUNT_2_BIT;  return S_OK;
      case  4: *pCount = VK_SAMPLE_COUNT_4_BIT;  return S_OK;
      case  8: *pCount = VK_SAMPLE_COUNT_8_BIT;  return S_OK;
      case 16: *pCount = VK_SAMPLE_COUNT_16_BIT; return S_OK;
    }
    
    return E_INVALIDARG;
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
  
  
  VkMemoryPropertyFlags D3D11Device::GetMemoryFlagsForUsage(D3D11_USAGE usage) const {
    VkMemoryPropertyFlags memoryFlags = 0;
    
    switch (usage) {
      case D3D11_USAGE_DEFAULT:
      case D3D11_USAGE_IMMUTABLE:
        memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
      
      case D3D11_USAGE_DYNAMIC:
        memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
      
      case D3D11_USAGE_STAGING:
        memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                    |  VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    }
    
    return memoryFlags;
  }
  
  
  VkSamplerAddressMode D3D11Device::DecodeAddressMode(
          D3D11_TEXTURE_ADDRESS_MODE  mode) const {
    switch (mode) {
      case D3D11_TEXTURE_ADDRESS_WRAP:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        
      case D3D11_TEXTURE_ADDRESS_MIRROR:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      
      case D3D11_TEXTURE_ADDRESS_CLAMP:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        
      case D3D11_TEXTURE_ADDRESS_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        
      case D3D11_TEXTURE_ADDRESS_MIRROR_ONCE:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
      
      default:
        Logger::err(str::format("D3D11: Unsupported address mode: ", mode));
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
  }
  
  
  VkCompareOp D3D11Device::DecodeCompareOp(
          D3D11_COMPARISON_FUNC mode) const {
    switch (mode) {
      case D3D11_COMPARISON_NEVER:
        return VK_COMPARE_OP_NEVER;
        
      case D3D11_COMPARISON_LESS:
        return VK_COMPARE_OP_LESS;
        
      case D3D11_COMPARISON_EQUAL:
        return VK_COMPARE_OP_EQUAL;
        
      case D3D11_COMPARISON_LESS_EQUAL:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
        
      case D3D11_COMPARISON_GREATER:
        return VK_COMPARE_OP_GREATER;
        
      case D3D11_COMPARISON_NOT_EQUAL:
        return VK_COMPARE_OP_NOT_EQUAL;
        
      case D3D11_COMPARISON_GREATER_EQUAL:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
        
      case D3D11_COMPARISON_ALWAYS:
        return VK_COMPARE_OP_ALWAYS;
        
      default:
        Logger::err(str::format("D3D11: Unsupported compare op: ", mode));
        return VK_COMPARE_OP_ALWAYS;
    }
  }
  
}
