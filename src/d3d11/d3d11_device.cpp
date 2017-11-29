#include <cstring>

#include "d3d11_buffer.h"
#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  D3D11Device::D3D11Device(
          IDXGIDevicePrivate* dxgiDevice,
          D3D_FEATURE_LEVEL   featureLevel,
          UINT                featureFlags)
  : m_dxgiDevice  (dxgiDevice),
    m_featureLevel(featureLevel),
    m_featureFlags(featureFlags),
    m_dxvkDevice  (m_dxgiDevice->GetDXVKDevice()),
    m_dxvkAdapter (m_dxvkDevice->adapter()) {
    m_dxgiDevice->SetDeviceLayer(this);
    m_context = new D3D11DeviceContext(this, m_dxvkDevice);
  }
  
  
  D3D11Device::~D3D11Device() {
    m_dxgiDevice->SetDeviceLayer(nullptr);
  }
  
  
  HRESULT D3D11Device::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Device);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DevicePrivate);
    
    if (riid == __uuidof(IDXGIDevicePrivate)
     || riid == __uuidof(IDXGIDevice))
      return m_dxgiDevice->QueryInterface(riid, ppvObject);
    
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
    Logger::err("D3D11Device::CreateRenderTargetView: Not implemented");
    return E_NOTIMPL;
  }
  
  
  HRESULT D3D11Device::CreateDepthStencilView(
          ID3D11Resource*                   pResource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc,
          ID3D11DepthStencilView**          ppDepthStencilView) {
    Logger::err("D3D11Device::CreateRenderTargetView: Not implemented");
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
  
  
  HRESULT D3D11Device::WrapSwapChainBackBuffer(
    const Rc<DxvkImage>&          image,
    const DXGI_SWAP_CHAIN_DESC*   pSwapChainDesc,
          IUnknown**              ppInterface) {
    D3D11_TEXTURE2D_DESC desc;
    desc.Width              = pSwapChainDesc->BufferDesc.Width;
    desc.Height             = pSwapChainDesc->BufferDesc.Height;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = pSwapChainDesc->BufferDesc.Format;
    desc.SampleDesc         = pSwapChainDesc->SampleDesc;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_RENDER_TARGET
                            | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = 0;
    
    *ppInterface = ref(new D3D11Texture2D(this, desc, image));
    return S_OK;
  }
  
  
  HRESULT D3D11Device::FlushRenderingCommands() {
    m_context->Flush();
    return S_OK;
  }
  
  
  Rc<DxvkDevice> D3D11Device::GetDXVKDevice() {
    return m_dxvkDevice;
  }
  
  
  bool D3D11Device::CheckFeatureLevelSupport(
          D3D_FEATURE_LEVEL featureLevel) {
    return featureLevel <= D3D_FEATURE_LEVEL_11_0;
  }
  
}
