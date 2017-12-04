#include <cstring>

#include "d3d11_buffer.h"
#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_present.h"
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
    Logger::err("D3D11Device::CreateBuffer: Not implemented");
    return E_NOTIMPL;
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
    Logger::err("D3D11Device::CreateTexture2D: Not implemented");
    return E_NOTIMPL;
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
    Logger::err("D3D11Device::CreateInputLayout: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateVertexShader(
    const void*                       pShaderBytecode,
          SIZE_T                      BytecodeLength,
          ID3D11ClassLinkage*         pClassLinkage,
          ID3D11VertexShader**        ppVertexShader) {
    Logger::err("D3D11Device::CreateVertexShader: Not implemented");
    return E_NOTIMPL;
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
    Logger::err("D3D11Device::CreatePixelShader: Not implemented");
    return E_NOTIMPL;
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
    Logger::err("D3D11Device::CreateRasterizerState: Not implemented");
    return E_NOTIMPL;
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
      enabled.alphaToOne                      = VK_TRUE;
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
  
}
