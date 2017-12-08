#include <cstring>

#include "d3d11_buffer.h"
#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_input_layout.h"
#include "d3d11_present.h"
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
     || riid == __uuidof(IDXGIDevicePrivate))
      return m_dxgiDevice->QueryInterface(riid, ppvObject);
    
    if (riid == __uuidof(IDXGIPresentDevicePrivate))
      return m_presentDevice->QueryInterface(riid, ppvObject);
    
    Logger::warn("D3D11Device::QueryInterface: Unknown interface query");
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
      info.access |= VK_ACCESS_SHADER_READ_BIT;
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
    
    if (pDesc->CPUAccessFlags != 0)
      info.tiling = VK_IMAGE_TILING_LINEAR;
    
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
    Logger::err("D3D11Device::CreateShaderResourceView: Not implemented");
    return E_NOTIMPL;
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
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    // Only 2D textures and 2D texture arrays are allowed
    if (resourceDim != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
      Logger::err("D3D11Device::CreateRenderTargetView: Unsupported resource type");
      return E_INVALIDARG;
    }
    
    // Make sure we can retrieve the image object
    auto texture = static_cast<D3D11Texture2D*>(pResource);
    
    // Image that we are going to create the view for
    const Rc<DxvkImage> image = texture->GetDXVKImage();
    
    // The view description is optional. If not defined, it
    // will use the resource's format and all subresources.
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    
    if (pDesc != nullptr) {
      desc = *pDesc;
    } else {
      D3D11_TEXTURE2D_DESC texDesc;
      texture->GetDesc(&texDesc);
      
      // Select the view dimension based on the
      // texture's array size and sample count.
      const std::array<D3D11_RTV_DIMENSION, 4> viewDims = {
        D3D11_RTV_DIMENSION_TEXTURE2D,
        D3D11_RTV_DIMENSION_TEXTURE2DARRAY,
        D3D11_RTV_DIMENSION_TEXTURE2DMS,
        D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY,
      };
      
      uint32_t viewDimIndex = 0;
      
      if (texDesc.ArraySize > 1)
        viewDimIndex |= 0x1;
      
      if (texDesc.SampleDesc.Count > 1)
        viewDimIndex |= 0x2;
      
      // Fill the correct union member
      desc.ViewDimension = viewDims.at(viewDimIndex);
      desc.Format = texDesc.Format;
      
      switch (desc.ViewDimension) {
        case D3D11_RTV_DIMENSION_TEXTURE2D:
          desc.Texture2D.MipSlice = 0;
          break;
          
        case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
          desc.Texture2DArray.MipSlice        = 0;
          desc.Texture2DArray.FirstArraySlice = 0;
          desc.Texture2DArray.ArraySize       = texDesc.ArraySize;
          break;
        
        case D3D11_RTV_DIMENSION_TEXTURE2DMS:
          break;
          
        case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
          desc.Texture2DMSArray.FirstArraySlice = 0;
          desc.Texture2DMSArray.ArraySize       = texDesc.ArraySize;
          break;
        
        default: 
          Logger::err("D3D11Device::CreateRenderTargetView: Internal error");
          return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
      }
    }
    
    // Fill in Vulkan image view info
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format = m_dxgiAdapter->LookupFormat(desc.Format).actual;
    viewInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    
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
          "D3D11Device::CreateRenderTargetView: pDesc->ViewDimension not supported: ",
          desc.ViewDimension));
        return E_INVALIDARG;
    }
    
    // Create the actual image view if requested
    if (ppRTView == nullptr)
      return S_OK;
    
    try {
      Rc<DxvkImageView> view = m_dxvkDevice->createImageView(image, viewInfo);
      *ppRTView = ref(new D3D11RenderTargetView(this, pResource, desc, view));
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
    Logger::err("D3D11Device::CreateDepthStencilView: Not implemented");
    return E_NOTIMPL;
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
    Logger::err("D3D11Device::CreateSamplerState: Not implemented");
    return E_NOTIMPL;
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
    Logger::err("D3D11Device::CheckMultisampleQualityLevels: Not implemented");
    return E_NOTIMPL;
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
    
    // TODO implement some sort of format info
    VkImageSubresourceRange subresources;
    subresources.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT
                                | VK_IMAGE_ASPECT_STENCIL_BIT;
    subresources.baseMipLevel   = 0;
    subresources.levelCount     = image->info().mipLevels;
    subresources.baseArrayLayer = 0;
    subresources.layerCount     = image->info().numLayers;
    m_resourceInitContext->initImage(image, subresources);
    
    if (pInitialData != nullptr)
      Logger::err("D3D11: InitTexture cannot upload image data yet");
    
    m_dxvkDevice->submitCommandList(
      m_resourceInitContext->endRecording(),
      nullptr, nullptr);
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
  
}
