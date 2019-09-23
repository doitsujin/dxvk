#include "d3d10_device.h"

#include "../d3d11/d3d11_device.h"
#include "../d3d11/d3d11_context_imm.h"

namespace dxvk {
  
  D3D10Device::D3D10Device(
          D3D11Device*                      pDevice,
          D3D11ImmediateContext*            pContext)
  : m_device(pDevice), m_context(pContext) {
  }

  
  D3D10Device::~D3D10Device() {

  }
  

  HRESULT STDMETHODCALLTYPE D3D10Device::QueryInterface(
          REFIID                            riid,
          void**                            ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }


  ULONG STDMETHODCALLTYPE D3D10Device::AddRef() {
    return m_device->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D10Device::Release() {
    return m_device->Release();
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::GetPrivateData(
          REFGUID                           guid,
          UINT*                             pDataSize,
          void*                             pData) {
    return m_device->GetPrivateData(guid, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::SetPrivateData(
          REFGUID                           guid,
          UINT                              DataSize,
    const void*                             pData) {
    return m_device->SetPrivateData(guid, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::SetPrivateDataInterface(
          REFGUID                           guid,
    const IUnknown*                         pData) {
    return m_device->SetPrivateDataInterface(guid, pData);
  }
  

  HRESULT STDMETHODCALLTYPE D3D10Device::GetDeviceRemovedReason() {
    return m_device->GetDeviceRemovedReason();
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::SetExceptionMode(
          UINT                              RaiseFlags) {
    return m_device->SetExceptionMode(RaiseFlags);
  }


  UINT STDMETHODCALLTYPE D3D10Device::GetExceptionMode() {
    return m_device->GetExceptionMode();
  }


  D3D10_FEATURE_LEVEL1 STDMETHODCALLTYPE D3D10Device::GetFeatureLevel() {
    return D3D10_FEATURE_LEVEL1(m_device->GetFeatureLevel());
  }


  void STDMETHODCALLTYPE D3D10Device::ClearState() {
    m_context->ClearState();
  }


  void STDMETHODCALLTYPE D3D10Device::Flush() {
    m_context->Flush();
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateBuffer(
    const D3D10_BUFFER_DESC*                pDesc,
    const D3D10_SUBRESOURCE_DATA*           pInitialData,
          ID3D10Buffer**                    ppBuffer) {
    InitReturnPtr(ppBuffer);

    if (pDesc == nullptr)
      return E_INVALIDARG;

    D3D11_BUFFER_DESC d3d11Desc;
    d3d11Desc.ByteWidth       = pDesc->ByteWidth;
    d3d11Desc.Usage           = D3D11_USAGE(pDesc->Usage);
    d3d11Desc.BindFlags       = pDesc->BindFlags;
    d3d11Desc.CPUAccessFlags  = pDesc->CPUAccessFlags;
    d3d11Desc.MiscFlags       = ConvertD3D10ResourceFlags(pDesc->MiscFlags);
    d3d11Desc.StructureByteStride = 0;

    ID3D11Buffer* d3d11Buffer = nullptr;
    HRESULT hr = m_device->CreateBuffer(&d3d11Desc,
      reinterpret_cast<const D3D11_SUBRESOURCE_DATA*>(pInitialData),
      ppBuffer != nullptr ? &d3d11Buffer : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppBuffer = static_cast<D3D11Buffer*>(d3d11Buffer)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture1D(
    const D3D10_TEXTURE1D_DESC*             pDesc,
    const D3D10_SUBRESOURCE_DATA*           pInitialData,
          ID3D10Texture1D**                 ppTexture1D) {
    InitReturnPtr(ppTexture1D);

    if (pDesc == nullptr)
      return E_INVALIDARG;

    D3D11_TEXTURE1D_DESC d3d11Desc;
    d3d11Desc.Width           = pDesc->Width;
    d3d11Desc.MipLevels       = pDesc->MipLevels;
    d3d11Desc.ArraySize       = pDesc->ArraySize;
    d3d11Desc.Format          = pDesc->Format;
    d3d11Desc.Usage           = D3D11_USAGE(pDesc->Usage);
    d3d11Desc.BindFlags       = pDesc->BindFlags;
    d3d11Desc.CPUAccessFlags  = pDesc->CPUAccessFlags;
    d3d11Desc.MiscFlags       = ConvertD3D10ResourceFlags(pDesc->MiscFlags);

    ID3D11Texture1D* d3d11Texture1D = nullptr;
    HRESULT hr = m_device->CreateTexture1D(&d3d11Desc,
      reinterpret_cast<const D3D11_SUBRESOURCE_DATA*>(pInitialData),
      ppTexture1D != nullptr ? &d3d11Texture1D : nullptr);
    
    if (hr != S_OK)
      return hr;

    *ppTexture1D = static_cast<D3D11Texture1D*>(d3d11Texture1D)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture2D(
    const D3D10_TEXTURE2D_DESC*             pDesc,
    const D3D10_SUBRESOURCE_DATA*           pInitialData,
          ID3D10Texture2D**                 ppTexture2D) {
    InitReturnPtr(ppTexture2D);

    if (pDesc == nullptr)
      return E_INVALIDARG;

    D3D11_TEXTURE2D_DESC d3d11Desc;
    d3d11Desc.Width           = pDesc->Width;
    d3d11Desc.Height          = pDesc->Height;
    d3d11Desc.MipLevels       = pDesc->MipLevels;
    d3d11Desc.ArraySize       = pDesc->ArraySize;
    d3d11Desc.Format          = pDesc->Format;
    d3d11Desc.SampleDesc      = pDesc->SampleDesc;
    d3d11Desc.Usage           = D3D11_USAGE(pDesc->Usage);
    d3d11Desc.BindFlags       = pDesc->BindFlags;
    d3d11Desc.CPUAccessFlags  = pDesc->CPUAccessFlags;
    d3d11Desc.MiscFlags       = ConvertD3D10ResourceFlags(pDesc->MiscFlags);

    ID3D11Texture2D* d3d11Texture2D = nullptr;
    HRESULT hr = m_device->CreateTexture2D(&d3d11Desc,
      reinterpret_cast<const D3D11_SUBRESOURCE_DATA*>(pInitialData),
      ppTexture2D != nullptr ? &d3d11Texture2D : nullptr);

    if (hr != S_OK)
      return hr;

    *ppTexture2D = static_cast<D3D11Texture2D*>(d3d11Texture2D)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture3D(
    const D3D10_TEXTURE3D_DESC*             pDesc,
    const D3D10_SUBRESOURCE_DATA*           pInitialData,
          ID3D10Texture3D**                 ppTexture3D) {
    InitReturnPtr(ppTexture3D);

    if (pDesc == nullptr)
      return E_INVALIDARG;

    D3D11_TEXTURE3D_DESC d3d11Desc;
    d3d11Desc.Width           = pDesc->Width;
    d3d11Desc.Height          = pDesc->Height;
    d3d11Desc.Depth           = pDesc->Depth;
    d3d11Desc.MipLevels       = pDesc->MipLevels;
    d3d11Desc.Format          = pDesc->Format;
    d3d11Desc.Usage           = D3D11_USAGE(pDesc->Usage);
    d3d11Desc.BindFlags       = pDesc->BindFlags;
    d3d11Desc.CPUAccessFlags  = pDesc->CPUAccessFlags;
    d3d11Desc.MiscFlags       = ConvertD3D10ResourceFlags(pDesc->MiscFlags);

    ID3D11Texture3D* d3d11Texture3D = nullptr;
    HRESULT hr = m_device->CreateTexture3D(&d3d11Desc,
      reinterpret_cast<const D3D11_SUBRESOURCE_DATA*>(pInitialData),
      ppTexture3D != nullptr ? &d3d11Texture3D : nullptr);

    if (hr != S_OK)
      return hr;

    *ppTexture3D = static_cast<D3D11Texture3D*>(d3d11Texture3D)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView(
          ID3D10Resource*                   pResource,
    const D3D10_SHADER_RESOURCE_VIEW_DESC*  pDesc,
          ID3D10ShaderResourceView**        ppSRView) {
    InitReturnPtr(ppSRView);

    if (!pResource)
      return E_INVALIDARG;

    Com<ID3D11Resource> d3d11Resource;
    GetD3D11Resource(pResource, &d3d11Resource);

    ID3D11ShaderResourceView* d3d11Srv = nullptr;
    HRESULT hr = m_device->CreateShaderResourceView(d3d11Resource.ptr(),
      reinterpret_cast<const D3D11_SHADER_RESOURCE_VIEW_DESC*>(pDesc),
      ppSRView ? &d3d11Srv : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppSRView = static_cast<D3D11ShaderResourceView*>(d3d11Srv)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView1(
          ID3D10Resource*                   pResource,
    const D3D10_SHADER_RESOURCE_VIEW_DESC1* pDesc,
          ID3D10ShaderResourceView1**       ppSRView) {
    InitReturnPtr(ppSRView);

    if (!pResource)
      return E_INVALIDARG;

    Com<ID3D11Resource> d3d11Resource;
    GetD3D11Resource(pResource, &d3d11Resource);

    ID3D11ShaderResourceView* d3d11View = nullptr;
    HRESULT hr = m_device->CreateShaderResourceView(d3d11Resource.ptr(),
      reinterpret_cast<const D3D11_SHADER_RESOURCE_VIEW_DESC*>(pDesc),
      ppSRView ? &d3d11View : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppSRView = static_cast<D3D11ShaderResourceView*>(d3d11View)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateRenderTargetView(
          ID3D10Resource*                   pResource,
    const D3D10_RENDER_TARGET_VIEW_DESC*    pDesc,
          ID3D10RenderTargetView**          ppRTView) {
    InitReturnPtr(ppRTView);

    if (!pResource)
      return E_INVALIDARG;

    Com<ID3D11Resource> d3d11Resource;
    GetD3D11Resource(pResource, &d3d11Resource);

    ID3D11RenderTargetView* d3d11View = nullptr;
    HRESULT hr = m_device->CreateRenderTargetView(d3d11Resource.ptr(),
      reinterpret_cast<const D3D11_RENDER_TARGET_VIEW_DESC*>(pDesc),
      ppRTView ? &d3d11View : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppRTView = static_cast<D3D11RenderTargetView*>(d3d11View)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilView(
          ID3D10Resource*                   pResource,
    const D3D10_DEPTH_STENCIL_VIEW_DESC*    pDesc,
          ID3D10DepthStencilView**          ppDepthStencilView) {
    InitReturnPtr(ppDepthStencilView);

    if (!pResource)
      return E_INVALIDARG;

    Com<ID3D11Resource> d3d11Resource;
    GetD3D11Resource(pResource, &d3d11Resource);

    // D3D10 doesn't have the Flags member, so we have
    // to convert the structure. pDesc can be nullptr.
    D3D11_DEPTH_STENCIL_VIEW_DESC d3d11Desc;

    if (pDesc != nullptr) {
      d3d11Desc.ViewDimension         = D3D11_DSV_DIMENSION(pDesc->ViewDimension);
      d3d11Desc.Format                = pDesc->Format;
      d3d11Desc.Flags                 = 0;

      switch (pDesc->ViewDimension) {
        case D3D10_DSV_DIMENSION_UNKNOWN:
          break;
        
        case D3D10_DSV_DIMENSION_TEXTURE1D:
          d3d11Desc.Texture1D.MipSlice               = pDesc->Texture1D.MipSlice;
          break;
        
        case D3D10_DSV_DIMENSION_TEXTURE1DARRAY:
          d3d11Desc.Texture1DArray.MipSlice          = pDesc->Texture1DArray.MipSlice;
          d3d11Desc.Texture1DArray.FirstArraySlice   = pDesc->Texture1DArray.FirstArraySlice;
          d3d11Desc.Texture1DArray.ArraySize         = pDesc->Texture1DArray.ArraySize;
          break;
        
        case D3D10_DSV_DIMENSION_TEXTURE2D:
          d3d11Desc.Texture2D.MipSlice               = pDesc->Texture2D.MipSlice;
          break;
        
        case D3D10_DSV_DIMENSION_TEXTURE2DARRAY:
          d3d11Desc.Texture2DArray.MipSlice          = pDesc->Texture2DArray.MipSlice;
          d3d11Desc.Texture2DArray.FirstArraySlice   = pDesc->Texture2DArray.FirstArraySlice;
          d3d11Desc.Texture2DArray.ArraySize         = pDesc->Texture2DArray.ArraySize;
          break;
        
        case D3D10_DSV_DIMENSION_TEXTURE2DMS:
          break;

        case D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY:
          d3d11Desc.Texture2DMSArray.FirstArraySlice = pDesc->Texture2DMSArray.FirstArraySlice;
          d3d11Desc.Texture2DMSArray.ArraySize       = pDesc->Texture2DMSArray.ArraySize;
          break;
      }
    }

    ID3D11DepthStencilView* d3d11View = nullptr;
    HRESULT hr = m_device->CreateDepthStencilView(
      d3d11Resource.ptr(), pDesc ? &d3d11Desc : nullptr,
      ppDepthStencilView ? &d3d11View : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppDepthStencilView = static_cast<D3D11DepthStencilView*>(d3d11View)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateInputLayout(
    const D3D10_INPUT_ELEMENT_DESC*         pInputElementDescs,
          UINT                              NumElements,
    const void*                             pShaderBytecodeWithInputSignature,
          SIZE_T                            BytecodeLength,
          ID3D10InputLayout**               ppInputLayout) {
    InitReturnPtr(ppInputLayout);

    static_assert(sizeof(D3D10_INPUT_ELEMENT_DESC) ==
                  sizeof(D3D11_INPUT_ELEMENT_DESC));

    ID3D11InputLayout* d3d11InputLayout = nullptr;
    HRESULT hr = m_device->CreateInputLayout(
      reinterpret_cast<const D3D11_INPUT_ELEMENT_DESC*>(pInputElementDescs),
      NumElements, pShaderBytecodeWithInputSignature, BytecodeLength,
      ppInputLayout ? &d3d11InputLayout : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppInputLayout = static_cast<D3D11InputLayout*>(d3d11InputLayout)->GetD3D10Iface();
    return hr;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateVertexShader(
    const void*                             pShaderBytecode,
          SIZE_T                            BytecodeLength,
          ID3D10VertexShader**              ppVertexShader) {
    InitReturnPtr(ppVertexShader);

    ID3D11VertexShader* d3d11Shader = nullptr;
    
    HRESULT hr = m_device->CreateVertexShader(
      pShaderBytecode, BytecodeLength, nullptr,
      ppVertexShader ? &d3d11Shader : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppVertexShader = static_cast<D3D11VertexShader*>(d3d11Shader)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShader(
    const void*                             pShaderBytecode,
          SIZE_T                            BytecodeLength,
          ID3D10GeometryShader**            ppGeometryShader) {
    InitReturnPtr(ppGeometryShader);

    ID3D11GeometryShader* d3d11Shader = nullptr;

    HRESULT hr = m_device->CreateGeometryShader(
      pShaderBytecode, BytecodeLength, nullptr,
      ppGeometryShader ? &d3d11Shader : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppGeometryShader = static_cast<D3D11GeometryShader*>(d3d11Shader)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShaderWithStreamOutput(
    const void*                             pShaderBytecode,
          SIZE_T                            BytecodeLength,
    const D3D10_SO_DECLARATION_ENTRY*       pSODeclaration,
          UINT                              NumEntries,
          UINT                              OutputStreamStride,
          ID3D10GeometryShader**            ppGeometryShader) {
    InitReturnPtr(ppGeometryShader);

    std::vector<D3D11_SO_DECLARATION_ENTRY> d3d11Entries(NumEntries);

    for (uint32_t i = 0; i < NumEntries; i++) {
      d3d11Entries[i].Stream          = 0;
      d3d11Entries[i].SemanticName    = pSODeclaration[i].SemanticName;
      d3d11Entries[i].SemanticIndex   = pSODeclaration[i].SemanticIndex;
      d3d11Entries[i].StartComponent  = pSODeclaration[i].StartComponent;
      d3d11Entries[i].ComponentCount  = pSODeclaration[i].ComponentCount;
      d3d11Entries[i].OutputSlot      = pSODeclaration[i].OutputSlot;
    }

    ID3D11GeometryShader* d3d11Shader = nullptr;

    HRESULT hr = m_device->CreateGeometryShaderWithStreamOutput(
      pShaderBytecode, BytecodeLength,
      d3d11Entries.data(),
      d3d11Entries.size(),
      &OutputStreamStride, 1,
      D3D11_SO_NO_RASTERIZED_STREAM, nullptr,
      ppGeometryShader ? &d3d11Shader : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppGeometryShader = static_cast<D3D11GeometryShader*>(d3d11Shader)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreatePixelShader(
    const void*                             pShaderBytecode,
          SIZE_T                            BytecodeLength,
          ID3D10PixelShader**               ppPixelShader) {
    InitReturnPtr(ppPixelShader);

    ID3D11PixelShader* d3d11Shader = nullptr;

    HRESULT hr = m_device->CreatePixelShader(
      pShaderBytecode, BytecodeLength, nullptr,
      ppPixelShader ? &d3d11Shader : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppPixelShader = static_cast<D3D11PixelShader*>(d3d11Shader)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState(
    const D3D10_BLEND_DESC*                 pBlendStateDesc,
          ID3D10BlendState**                ppBlendState) {
    InitReturnPtr(ppBlendState);

    D3D11_BLEND_DESC d3d11Desc;

    if (pBlendStateDesc != nullptr) {
      d3d11Desc.AlphaToCoverageEnable   = pBlendStateDesc->AlphaToCoverageEnable;
      d3d11Desc.IndependentBlendEnable  = TRUE;

      for (uint32_t i = 0; i < 8; i++) {
        d3d11Desc.RenderTarget[i].BlendEnable           = pBlendStateDesc->BlendEnable[i];
        d3d11Desc.RenderTarget[i].SrcBlend              = D3D11_BLEND   (pBlendStateDesc->SrcBlend);
        d3d11Desc.RenderTarget[i].DestBlend             = D3D11_BLEND   (pBlendStateDesc->DestBlend);
        d3d11Desc.RenderTarget[i].BlendOp               = D3D11_BLEND_OP(pBlendStateDesc->BlendOp);
        d3d11Desc.RenderTarget[i].SrcBlendAlpha         = D3D11_BLEND   (pBlendStateDesc->SrcBlendAlpha);
        d3d11Desc.RenderTarget[i].DestBlendAlpha        = D3D11_BLEND   (pBlendStateDesc->DestBlendAlpha);
        d3d11Desc.RenderTarget[i].BlendOpAlpha          = D3D11_BLEND_OP(pBlendStateDesc->BlendOpAlpha);
        d3d11Desc.RenderTarget[i].RenderTargetWriteMask = pBlendStateDesc->RenderTargetWriteMask[i];
      }
    }

    ID3D11BlendState* d3d11BlendState = nullptr;
    HRESULT hr = m_device->CreateBlendState(&d3d11Desc,
      ppBlendState ? &d3d11BlendState : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppBlendState = static_cast<D3D11BlendState*>(d3d11BlendState)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState1(
    const D3D10_BLEND_DESC1*                pBlendStateDesc,
          ID3D10BlendState1**               ppBlendState) {
    InitReturnPtr(ppBlendState);

    ID3D11BlendState* d3d11BlendState = nullptr;
    HRESULT hr = m_device->CreateBlendState(
      reinterpret_cast<const D3D11_BLEND_DESC*>(pBlendStateDesc),
      ppBlendState ? &d3d11BlendState : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppBlendState = static_cast<D3D11BlendState*>(d3d11BlendState)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilState(
    const D3D10_DEPTH_STENCIL_DESC*         pDepthStencilDesc,
          ID3D10DepthStencilState**         ppDepthStencilState) {
    InitReturnPtr(ppDepthStencilState);

    ID3D11DepthStencilState* d3d11DepthStencilState = nullptr;
    HRESULT hr = m_device->CreateDepthStencilState(
      reinterpret_cast<const D3D11_DEPTH_STENCIL_DESC*>(pDepthStencilDesc),
      ppDepthStencilState ? &d3d11DepthStencilState : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppDepthStencilState = static_cast<D3D11DepthStencilState*>(d3d11DepthStencilState)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateRasterizerState(
    const D3D10_RASTERIZER_DESC*            pRasterizerDesc,
          ID3D10RasterizerState**           ppRasterizerState) {
    InitReturnPtr(ppRasterizerState);

    ID3D11RasterizerState* d3d11RasterizerState = nullptr;
    HRESULT hr = m_device->CreateRasterizerState(
      reinterpret_cast<const D3D11_RASTERIZER_DESC*>(pRasterizerDesc),
      ppRasterizerState ? &d3d11RasterizerState : nullptr);
    
    if (hr != S_OK)
      return hr;
    
    *ppRasterizerState = static_cast<D3D11RasterizerState*>(d3d11RasterizerState)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateSamplerState(
    const D3D10_SAMPLER_DESC*               pSamplerDesc,
          ID3D10SamplerState**              ppSamplerState) {
    InitReturnPtr(ppSamplerState);

    if (pSamplerDesc == nullptr)
      return E_INVALIDARG;

    D3D11_SAMPLER_DESC d3d11Desc;
    d3d11Desc.Filter            = D3D11_FILTER(pSamplerDesc->Filter);
    d3d11Desc.AddressU          = D3D11_TEXTURE_ADDRESS_MODE(pSamplerDesc->AddressU);
    d3d11Desc.AddressV          = D3D11_TEXTURE_ADDRESS_MODE(pSamplerDesc->AddressV);
    d3d11Desc.AddressW          = D3D11_TEXTURE_ADDRESS_MODE(pSamplerDesc->AddressW);
    d3d11Desc.MipLODBias        = pSamplerDesc->MipLODBias;
    d3d11Desc.MaxAnisotropy     = pSamplerDesc->MaxAnisotropy;
    d3d11Desc.ComparisonFunc    = D3D11_COMPARISON_FUNC(pSamplerDesc->ComparisonFunc);
    d3d11Desc.MinLOD            = pSamplerDesc->MinLOD;
    d3d11Desc.MaxLOD            = pSamplerDesc->MaxLOD;

    for (uint32_t i = 0; i < 4; i++)
      d3d11Desc.BorderColor[i] = pSamplerDesc->BorderColor[i];

    ID3D11SamplerState* d3d11SamplerState = nullptr;
    HRESULT hr = m_device->CreateSamplerState(&d3d11Desc,
      ppSamplerState ? &d3d11SamplerState : nullptr);
    
    if (hr != S_OK)
      return hr;

    *ppSamplerState = static_cast<D3D11SamplerState*>(d3d11SamplerState)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateQuery(
    const D3D10_QUERY_DESC*                 pQueryDesc,
          ID3D10Query**                     ppQuery) {
    InitReturnPtr(ppQuery);

    if (pQueryDesc == nullptr)
      return E_INVALIDARG;

    D3D11_QUERY_DESC d3d11Desc;
    d3d11Desc.Query      = D3D11_QUERY(pQueryDesc->Query);
    d3d11Desc.MiscFlags  = pQueryDesc->MiscFlags;

    ID3D11Query* d3d11Query = nullptr;
    HRESULT hr = m_device->CreateQuery(&d3d11Desc,
      ppQuery ? &d3d11Query : nullptr);

    if (hr != S_OK)
      return hr;

    *ppQuery = static_cast<D3D11Query*>(d3d11Query)->GetD3D10Iface();
     return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreatePredicate(
    const D3D10_QUERY_DESC*                 pPredicateDesc,
          ID3D10Predicate**                 ppPredicate) {
    InitReturnPtr(ppPredicate);

    D3D11_QUERY_DESC d3d11Desc;
    d3d11Desc.Query      = D3D11_QUERY(pPredicateDesc->Query);
    d3d11Desc.MiscFlags  = pPredicateDesc->MiscFlags;

    ID3D11Predicate* d3d11Predicate = nullptr;
    HRESULT hr = m_device->CreatePredicate(&d3d11Desc,
      ppPredicate ? &d3d11Predicate : nullptr);

    if (hr != S_OK)
      return hr;
    
    *ppPredicate = D3D11Query::FromPredicate(d3d11Predicate)->GetD3D10Iface();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateCounter(
    const D3D10_COUNTER_DESC*               pCounterDesc,
          ID3D10Counter**                   ppCounter) {
    Logger::err("D3D10Device::CreateCounter: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CheckFormatSupport(
          DXGI_FORMAT                       Format,
          UINT*                             pFormatSupport) {
    return m_device->CheckFormatSupport(Format, pFormatSupport);
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CheckMultisampleQualityLevels(
          DXGI_FORMAT                       Format,
          UINT                              SampleCount,
          UINT*                             pNumQualityLevels) {
    return m_device->CheckMultisampleQualityLevels(
      Format, SampleCount, pNumQualityLevels);
  }


  void STDMETHODCALLTYPE D3D10Device::CheckCounterInfo(
          D3D10_COUNTER_INFO*               pCounterInfo) {
    Logger::err("D3D10Device::CheckCounterInfo: Not implemented");
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CheckCounter(
    const D3D10_COUNTER_DESC*               pDesc,
          D3D10_COUNTER_TYPE*               pType,
          UINT*                             pActiveCounters,
          char*                             name,
          UINT*                             pNameLength,
          char*                             units,
          UINT*                             pUnitsLength,
          char*                             description,
          UINT*                             pDescriptionLength) {
    Logger::err("D3D10Device::CheckCounter: Not implemented");
    return E_NOTIMPL;
  }


  UINT STDMETHODCALLTYPE D3D10Device::GetCreationFlags() {
    return m_device->GetCreationFlags();
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::OpenSharedResource(
          HANDLE                            hResource,
          REFIID                            ReturnedInterface,
          void**                            ppResource) {
    InitReturnPtr(ppResource);
    
    Logger::err("D3D10Device::OpenSharedResource: Not implemented");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D10Device::ClearRenderTargetView(
          ID3D10RenderTargetView*           pRenderTargetView,
    const FLOAT                             ColorRGBA[4]) {
    D3D10RenderTargetView* d3d10View = static_cast<D3D10RenderTargetView*>(pRenderTargetView);
    D3D11RenderTargetView* d3d11View = d3d10View ? d3d10View->GetD3D11Iface() : nullptr;

    m_context->ClearRenderTargetView(d3d11View, ColorRGBA);
  }


  void STDMETHODCALLTYPE D3D10Device::ClearDepthStencilView(
          ID3D10DepthStencilView*           pDepthStencilView,
          UINT                              ClearFlags,
          FLOAT                             Depth,
          UINT8                             Stencil) {
    D3D10DepthStencilView* d3d10View = static_cast<D3D10DepthStencilView*>(pDepthStencilView);
    D3D11DepthStencilView* d3d11View = d3d10View ? d3d10View->GetD3D11Iface() : nullptr;

    m_context->ClearDepthStencilView(d3d11View, ClearFlags, Depth, Stencil);
  }


  void STDMETHODCALLTYPE D3D10Device::SetPredication(
          ID3D10Predicate*                  pPredicate,
          BOOL                              PredicateValue) {
    D3D10Query* d3d10Predicate = static_cast<D3D10Query*>(pPredicate);
    D3D11Query* d3d11Predicate = d3d10Predicate ? d3d10Predicate->GetD3D11Iface() : nullptr;

    m_context->SetPredication(D3D11Query::AsPredicate(d3d11Predicate), PredicateValue);
  }


  void STDMETHODCALLTYPE D3D10Device::GetPredication(
          ID3D10Predicate**                 ppPredicate,
          BOOL*                             pPredicateValue) {
    ID3D11Predicate* d3d11Predicate = nullptr;

    m_context->GetPredication(
      ppPredicate ? &d3d11Predicate : nullptr,
      pPredicateValue);

    if (ppPredicate)
      *ppPredicate = d3d11Predicate ? D3D11Query::FromPredicate(d3d11Predicate)->GetD3D10Iface() : nullptr;
  }


  void STDMETHODCALLTYPE D3D10Device::CopySubresourceRegion(
          ID3D10Resource*                   pDstResource,
          UINT                              DstSubresource,
          UINT                              DstX,
          UINT                              DstY,
          UINT                              DstZ,
          ID3D10Resource*                   pSrcResource,
          UINT                              SrcSubresource,
    const D3D10_BOX*                        pSrcBox) {
    if (!pDstResource || !pSrcResource)
      return;
    
    Com<ID3D11Resource> d3d11DstResource;
    Com<ID3D11Resource> d3d11SrcResource;

    GetD3D11Resource(pDstResource, &d3d11DstResource);
    GetD3D11Resource(pSrcResource, &d3d11SrcResource);

    m_context->CopySubresourceRegion(
      d3d11DstResource.ptr(), DstSubresource, DstX, DstY, DstZ,
      d3d11SrcResource.ptr(), SrcSubresource,
      reinterpret_cast<const D3D11_BOX*>(pSrcBox));
  }


  void STDMETHODCALLTYPE D3D10Device::CopyResource(
          ID3D10Resource*                   pDstResource,
          ID3D10Resource*                   pSrcResource) {
    if (!pDstResource || !pSrcResource)
      return;

    Com<ID3D11Resource> d3d11DstResource;
    Com<ID3D11Resource> d3d11SrcResource;
    
    GetD3D11Resource(pDstResource, &d3d11DstResource);
    GetD3D11Resource(pSrcResource, &d3d11SrcResource);

    m_context->CopyResource(
      d3d11DstResource.ptr(),
      d3d11SrcResource.ptr());
  }


  void STDMETHODCALLTYPE D3D10Device::UpdateSubresource(
          ID3D10Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D10_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch) {
    if (!pDstResource)
      return;

    Com<ID3D11Resource> d3d11DstResource;
    GetD3D11Resource(pDstResource, &d3d11DstResource);

    m_context->UpdateSubresource(
      d3d11DstResource.ptr(), DstSubresource,
      reinterpret_cast<const D3D11_BOX*>(pDstBox),
      pSrcData, SrcRowPitch, SrcDepthPitch);
  }


  void STDMETHODCALLTYPE D3D10Device::GenerateMips(
          ID3D10ShaderResourceView*         pShaderResourceView) {
    D3D10ShaderResourceView* d3d10View = static_cast<D3D10ShaderResourceView*>(pShaderResourceView);
    D3D11ShaderResourceView* d3d11View = d3d10View ? d3d10View->GetD3D11Iface() : nullptr;

    m_context->GenerateMips(d3d11View);
  }


  void STDMETHODCALLTYPE D3D10Device::ResolveSubresource(
          ID3D10Resource*                   pDstResource,
          UINT                              DstSubresource,
          ID3D10Resource*                   pSrcResource,
          UINT                              SrcSubresource,
          DXGI_FORMAT                       Format) {
    if (!pDstResource || !pSrcResource)
      return;

    Com<ID3D11Resource> d3d11DstResource;
    Com<ID3D11Resource> d3d11SrcResource;
    
    GetD3D11Resource(pDstResource, &d3d11DstResource);
    GetD3D11Resource(pSrcResource, &d3d11SrcResource);

    m_context->ResolveSubresource(
      d3d11DstResource.ptr(), DstSubresource,
      d3d11SrcResource.ptr(), SrcSubresource,
      Format);
  }


  void STDMETHODCALLTYPE D3D10Device::Draw(
          UINT                              VertexCount,
          UINT                              StartVertexLocation) {
    m_context->Draw(VertexCount,
      StartVertexLocation);
  }


  void STDMETHODCALLTYPE D3D10Device::DrawIndexed(
          UINT                              IndexCount,
          UINT                              StartIndexLocation,
          INT                               BaseVertexLocation) {
    m_context->DrawIndexed(IndexCount,
      StartIndexLocation,
      BaseVertexLocation);
  }


  void STDMETHODCALLTYPE D3D10Device::DrawInstanced(
          UINT                              VertexCountPerInstance,
          UINT                              InstanceCount,
          UINT                              StartVertexLocation,
          UINT                              StartInstanceLocation) {
    m_context->DrawInstanced(
      VertexCountPerInstance,
      InstanceCount,
      StartVertexLocation,
      StartInstanceLocation);
  }


  void STDMETHODCALLTYPE D3D10Device::DrawIndexedInstanced(
          UINT                              IndexCountPerInstance,
          UINT                              InstanceCount,
          UINT                              StartIndexLocation,
          INT                               BaseVertexLocation,
          UINT                              StartInstanceLocation) {
    m_context->DrawIndexedInstanced(
      IndexCountPerInstance,
      InstanceCount,
      StartIndexLocation,
      BaseVertexLocation,
      StartInstanceLocation);
  }


  void STDMETHODCALLTYPE D3D10Device::DrawAuto() {
    m_context->DrawAuto();
  }


  void STDMETHODCALLTYPE D3D10Device::IASetInputLayout(
          ID3D10InputLayout*                pInputLayout) {
    D3D10InputLayout* d3d10InputLayout = static_cast<D3D10InputLayout*>(pInputLayout);
    D3D11InputLayout* d3d11InputLayout = d3d10InputLayout ? d3d10InputLayout->GetD3D11Iface() : nullptr;

    m_context->IASetInputLayout(d3d11InputLayout);
  }


  void STDMETHODCALLTYPE D3D10Device::IASetPrimitiveTopology(
          D3D10_PRIMITIVE_TOPOLOGY          Topology) {
    m_context->IASetPrimitiveTopology(
      D3D11_PRIMITIVE_TOPOLOGY(Topology));
  }


  void STDMETHODCALLTYPE D3D10Device::IASetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer* const*              ppVertexBuffers,
    const UINT*                             pStrides,
    const UINT*                             pOffsets) {
    ID3D11Buffer* d3d11Buffers[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

    if (NumBuffers > D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumBuffers; i++) {
      d3d11Buffers[i] = ppVertexBuffers[i]
        ? static_cast<D3D10Buffer*>(ppVertexBuffers[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->IASetVertexBuffers(
      StartSlot, NumBuffers, d3d11Buffers,
      pStrides, pOffsets);
  }


  void STDMETHODCALLTYPE D3D10Device::IASetIndexBuffer(
          ID3D10Buffer*                     pIndexBuffer,
          DXGI_FORMAT                       Format,
          UINT                              Offset) {
    D3D10Buffer* d3d10Buffer = static_cast<D3D10Buffer*>(pIndexBuffer);
    D3D11Buffer* d3d11Buffer = d3d10Buffer ? d3d10Buffer->GetD3D11Iface() : nullptr;

    m_context->IASetIndexBuffer(d3d11Buffer, Format, Offset);
  }


  void STDMETHODCALLTYPE D3D10Device::IAGetInputLayout(
          ID3D10InputLayout**               ppInputLayout) {
    ID3D11InputLayout* d3d11InputLayout = nullptr;
    m_context->IAGetInputLayout(&d3d11InputLayout);

    *ppInputLayout = d3d11InputLayout
      ? static_cast<D3D11InputLayout*>(d3d11InputLayout)->GetD3D10Iface()
      : nullptr;
  }


  void STDMETHODCALLTYPE D3D10Device::IAGetPrimitiveTopology(
          D3D10_PRIMITIVE_TOPOLOGY*         pTopology) {
    D3D11_PRIMITIVE_TOPOLOGY d3d11Topology;
    m_context->IAGetPrimitiveTopology(&d3d11Topology);

    *pTopology = d3d11Topology <= 32 /* begin patch list */
      ? D3D10_PRIMITIVE_TOPOLOGY(d3d11Topology)
      : D3D10_PRIMITIVE_TOPOLOGY_UNDEFINED;
  }


  void STDMETHODCALLTYPE D3D10Device::IAGetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer**                    ppVertexBuffers,
          UINT*                             pStrides,
          UINT*                             pOffsets) {
    ID3D11Buffer* d3d11Buffers[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

    m_context->IAGetVertexBuffers(
      StartSlot, NumBuffers,
      ppVertexBuffers ? d3d11Buffers : nullptr,
      pStrides, pOffsets);

    if (ppVertexBuffers) {
      for (uint32_t i = 0; i < NumBuffers; i++) {
        ppVertexBuffers[i] = d3d11Buffers[i]
          ? static_cast<D3D11Buffer*>(d3d11Buffers[i])->GetD3D10Iface()
          : nullptr;
      }
    }
  }


  void STDMETHODCALLTYPE D3D10Device::IAGetIndexBuffer(
          ID3D10Buffer**                    pIndexBuffer,
          DXGI_FORMAT*                      Format,
          UINT*                             Offset) {
    ID3D11Buffer* d3d11Buffer = nullptr;

    m_context->IAGetIndexBuffer(
      pIndexBuffer ? &d3d11Buffer : nullptr,
      Format, Offset);

    if (pIndexBuffer)
      *pIndexBuffer = d3d11Buffer ? static_cast<D3D11Buffer*>(d3d11Buffer)->GetD3D10Iface() : nullptr;
  }


  void STDMETHODCALLTYPE D3D10Device::VSSetShader(
          ID3D10VertexShader*               pVertexShader) {
    D3D10VertexShader* d3d10Shader = static_cast<D3D10VertexShader*>(pVertexShader);
    D3D11VertexShader* d3d11Shader = d3d10Shader ? d3d10Shader->GetD3D11Iface() : nullptr;

    m_context->VSSetShader(d3d11Shader, nullptr, 0);
  }


  void STDMETHODCALLTYPE D3D10Device::VSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer* const*              ppConstantBuffers) {
    ID3D11Buffer* d3d11Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

    if (NumBuffers > D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumBuffers; i++) {
      d3d11Buffers[i] = ppConstantBuffers && ppConstantBuffers[i]
        ? static_cast<D3D10Buffer*>(ppConstantBuffers[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->VSSetConstantBuffers(StartSlot, NumBuffers, d3d11Buffers);
  }


  void STDMETHODCALLTYPE D3D10Device::VSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D10ShaderResourceView* const*  ppShaderResourceViews) {
    ID3D11ShaderResourceView* d3d11Views[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

    if (NumViews > D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumViews; i++) {
      d3d11Views[i] = ppShaderResourceViews && ppShaderResourceViews[i]
        ? static_cast<D3D10ShaderResourceView*>(ppShaderResourceViews[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->VSSetShaderResources(StartSlot, NumViews, d3d11Views);
  }


  void STDMETHODCALLTYPE D3D10Device::VSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D10SamplerState* const*        ppSamplers) {
    ID3D11SamplerState* d3d11Samplers[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];

    if (NumSamplers > D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumSamplers; i++) {
      d3d11Samplers[i] = ppSamplers && ppSamplers[i]
        ? static_cast<D3D10SamplerState*>(ppSamplers[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->VSSetSamplers(StartSlot, NumSamplers, d3d11Samplers);
  }


  void STDMETHODCALLTYPE D3D10Device::VSGetShader(
          ID3D10VertexShader**              ppVertexShader) {
    ID3D11VertexShader* d3d11Shader = nullptr;
    m_context->VSGetShader(&d3d11Shader, nullptr, nullptr);

    *ppVertexShader = d3d11Shader ? static_cast<D3D11VertexShader*>(d3d11Shader)->GetD3D10Iface() : nullptr;
  }


  void STDMETHODCALLTYPE D3D10Device::VSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer**                    ppConstantBuffers) {
    ID3D11Buffer* d3d11Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    m_context->VSGetConstantBuffers(StartSlot, NumBuffers, d3d11Buffers);

    for (uint32_t i = 0; i < NumBuffers; i++) {
      ppConstantBuffers[i] = d3d11Buffers[i]
        ? static_cast<D3D11Buffer*>(d3d11Buffers[i])->GetD3D10Iface()
        : nullptr;
    }
  }


  void STDMETHODCALLTYPE D3D10Device::VSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D10ShaderResourceView**        ppShaderResourceViews) {
    ID3D11ShaderResourceView* d3d11Views[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    m_context->VSGetShaderResources(StartSlot, NumViews, d3d11Views);

    for (uint32_t i = 0; i < NumViews; i++) {
      ppShaderResourceViews[i] = d3d11Views[i]
        ? static_cast<D3D11ShaderResourceView*>(d3d11Views[i])->GetD3D10Iface()
        : nullptr;
    }
  }


  void STDMETHODCALLTYPE D3D10Device::VSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D10SamplerState**              ppSamplers) {
    ID3D11SamplerState* d3d11Samplers[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
    m_context->VSGetSamplers(StartSlot, NumSamplers, d3d11Samplers);

    for (uint32_t i = 0; i < NumSamplers; i++) {
      ppSamplers[i] = d3d11Samplers[i]
        ? static_cast<D3D11SamplerState*>(d3d11Samplers[i])->GetD3D10Iface()
        : nullptr;
    }
  }


  void STDMETHODCALLTYPE D3D10Device::GSSetShader(
          ID3D10GeometryShader*             pShader) {
    D3D10GeometryShader* d3d10Shader = static_cast<D3D10GeometryShader*>(pShader);
    D3D11GeometryShader* d3d11Shader = d3d10Shader ? d3d10Shader->GetD3D11Iface() : nullptr;

    m_context->GSSetShader(d3d11Shader, nullptr, 0);
  }


  void STDMETHODCALLTYPE D3D10Device::GSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer* const*              ppConstantBuffers) {
    ID3D11Buffer* d3d11Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

    if (NumBuffers > D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumBuffers; i++) {
      d3d11Buffers[i] = ppConstantBuffers && ppConstantBuffers[i]
        ? static_cast<D3D10Buffer*>(ppConstantBuffers[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->GSSetConstantBuffers(StartSlot, NumBuffers, d3d11Buffers);
  }


  void STDMETHODCALLTYPE D3D10Device::GSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D10ShaderResourceView* const*  ppShaderResourceViews) {
    ID3D11ShaderResourceView* d3d11Views[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

    if (NumViews > D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumViews; i++) {
      d3d11Views[i] = ppShaderResourceViews && ppShaderResourceViews[i]
        ? static_cast<D3D10ShaderResourceView*>(ppShaderResourceViews[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->GSSetShaderResources(StartSlot, NumViews, d3d11Views);
  }


  void STDMETHODCALLTYPE D3D10Device::GSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D10SamplerState* const*        ppSamplers) {
    ID3D11SamplerState* d3d11Samplers[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];

    if (NumSamplers > D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumSamplers; i++) {
      d3d11Samplers[i] = ppSamplers && ppSamplers[i]
        ? static_cast<D3D10SamplerState*>(ppSamplers[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->GSSetSamplers(StartSlot, NumSamplers, d3d11Samplers);
  }


  void STDMETHODCALLTYPE D3D10Device::GSGetShader(
          ID3D10GeometryShader**            ppGeometryShader) {
    ID3D11GeometryShader* d3d11Shader = nullptr;
    m_context->GSGetShader(&d3d11Shader, nullptr, nullptr);

    *ppGeometryShader = d3d11Shader ? static_cast<D3D11GeometryShader*>(d3d11Shader)->GetD3D10Iface() : nullptr;
  }


  void STDMETHODCALLTYPE D3D10Device::GSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer**                    ppConstantBuffers) {
    ID3D11Buffer* d3d11Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    m_context->GSGetConstantBuffers(StartSlot, NumBuffers, d3d11Buffers);

    for (uint32_t i = 0; i < NumBuffers; i++) {
      ppConstantBuffers[i] = d3d11Buffers[i]
        ? static_cast<D3D11Buffer*>(d3d11Buffers[i])->GetD3D10Iface()
        : nullptr;
    }
  }


  void STDMETHODCALLTYPE D3D10Device::GSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D10ShaderResourceView**        ppShaderResourceViews) {
    ID3D11ShaderResourceView* d3d11Views[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    m_context->GSGetShaderResources(StartSlot, NumViews, d3d11Views);

    for (uint32_t i = 0; i < NumViews; i++) {
      ppShaderResourceViews[i] = d3d11Views[i]
        ? static_cast<D3D11ShaderResourceView*>(d3d11Views[i])->GetD3D10Iface()
        : nullptr;
    }
  }


  void STDMETHODCALLTYPE D3D10Device::GSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D10SamplerState**              ppSamplers) {
    ID3D11SamplerState* d3d11Samplers[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
    m_context->GSGetSamplers(StartSlot, NumSamplers, d3d11Samplers);

    for (uint32_t i = 0; i < NumSamplers; i++) {
      ppSamplers[i] = d3d11Samplers[i]
        ? static_cast<D3D11SamplerState*>(d3d11Samplers[i])->GetD3D10Iface()
        : nullptr;
    }
  }


  void STDMETHODCALLTYPE D3D10Device::PSSetShader(
          ID3D10PixelShader*                pPixelShader) {
    D3D10PixelShader* d3d10Shader = static_cast<D3D10PixelShader*>(pPixelShader);
    D3D11PixelShader* d3d11Shader = d3d10Shader ? d3d10Shader->GetD3D11Iface() : nullptr;

    m_context->PSSetShader(d3d11Shader, nullptr, 0);
  }


  void STDMETHODCALLTYPE D3D10Device::PSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer* const*              ppConstantBuffers) {
    ID3D11Buffer* d3d11Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

    if (NumBuffers > D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumBuffers; i++) {
      d3d11Buffers[i] = ppConstantBuffers && ppConstantBuffers[i]
        ? static_cast<D3D10Buffer*>(ppConstantBuffers[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->PSSetConstantBuffers(StartSlot, NumBuffers, d3d11Buffers);
  }


  void STDMETHODCALLTYPE D3D10Device::PSSetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D10ShaderResourceView* const*  ppShaderResourceViews) {
    ID3D11ShaderResourceView* d3d11Views[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];

    if (NumViews > D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumViews; i++) {
      d3d11Views[i] = ppShaderResourceViews && ppShaderResourceViews[i]
        ? static_cast<D3D10ShaderResourceView*>(ppShaderResourceViews[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->PSSetShaderResources(StartSlot, NumViews, d3d11Views);
  }


  void STDMETHODCALLTYPE D3D10Device::PSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D10SamplerState* const*        ppSamplers) {
    ID3D11SamplerState* d3d11Samplers[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];

    if (NumSamplers > D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumSamplers; i++) {
      d3d11Samplers[i] = ppSamplers && ppSamplers[i]
        ? static_cast<D3D10SamplerState*>(ppSamplers[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->PSSetSamplers(StartSlot, NumSamplers, d3d11Samplers);
  }


  void STDMETHODCALLTYPE D3D10Device::PSGetShader(
          ID3D10PixelShader**               ppPixelShader) {
    ID3D11PixelShader* d3d11Shader = nullptr;
    m_context->PSGetShader(&d3d11Shader, nullptr, nullptr);

    *ppPixelShader = d3d11Shader ? static_cast<D3D11PixelShader*>(d3d11Shader)->GetD3D10Iface() : nullptr;
  }


  void STDMETHODCALLTYPE D3D10Device::PSGetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer**                    ppConstantBuffers) {
    ID3D11Buffer* d3d11Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    m_context->PSGetConstantBuffers(StartSlot, NumBuffers, d3d11Buffers);

    for (uint32_t i = 0; i < NumBuffers; i++) {
      ppConstantBuffers[i] = d3d11Buffers[i]
        ? static_cast<D3D11Buffer*>(d3d11Buffers[i])->GetD3D10Iface()
        : nullptr;
    }
  }


  void STDMETHODCALLTYPE D3D10Device::PSGetShaderResources(
          UINT                              StartSlot,
          UINT                              NumViews,
          ID3D10ShaderResourceView**        ppShaderResourceViews) {
    ID3D11ShaderResourceView* d3d11Views[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    m_context->PSGetShaderResources(StartSlot, NumViews, d3d11Views);

    for (uint32_t i = 0; i < NumViews; i++) {
      ppShaderResourceViews[i] = d3d11Views[i]
        ? static_cast<D3D11ShaderResourceView*>(d3d11Views[i])->GetD3D10Iface()
        : nullptr;
    }
  }


  void STDMETHODCALLTYPE D3D10Device::PSGetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D10SamplerState**              ppSamplers) {
    ID3D11SamplerState* d3d11Samplers[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
    m_context->PSGetSamplers(StartSlot, NumSamplers, d3d11Samplers);

    for (uint32_t i = 0; i < NumSamplers; i++) {
      ppSamplers[i] = d3d11Samplers[i]
        ? static_cast<D3D11SamplerState*>(d3d11Samplers[i])->GetD3D10Iface()
        : nullptr;
    }
  }


  void STDMETHODCALLTYPE D3D10Device::OMSetRenderTargets(
          UINT                              NumViews,
          ID3D10RenderTargetView* const*    ppRenderTargetViews,
          ID3D10DepthStencilView*           pDepthStencilView) {
    ID3D11RenderTargetView* d3d11Rtv[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];

    if (NumViews > D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT)
      return;

    for (uint32_t i = 0; i < NumViews; i++) {
      d3d11Rtv[i] = ppRenderTargetViews && ppRenderTargetViews[i]
        ? static_cast<D3D10RenderTargetView*>(ppRenderTargetViews[i])->GetD3D11Iface()
        : nullptr;
    }

    D3D10DepthStencilView* d3d10Dsv = static_cast<D3D10DepthStencilView*>(pDepthStencilView);
    D3D11DepthStencilView* d3d11Dsv = d3d10Dsv ? d3d10Dsv->GetD3D11Iface() : nullptr;

    m_context->OMSetRenderTargets(NumViews, d3d11Rtv, d3d11Dsv);
  }


  void STDMETHODCALLTYPE D3D10Device::OMSetBlendState(
          ID3D10BlendState*                 pBlendState,
    const FLOAT                             BlendFactor[4],
          UINT                              SampleMask) {
    D3D10BlendState* d3d10BlendState = static_cast<D3D10BlendState*>(pBlendState);
    D3D11BlendState* d3d11BlendState = d3d10BlendState ? d3d10BlendState->GetD3D11Iface() : nullptr;

    m_context->OMSetBlendState(d3d11BlendState, BlendFactor, SampleMask);
  }


  void STDMETHODCALLTYPE D3D10Device::OMSetDepthStencilState(
          ID3D10DepthStencilState*          pDepthStencilState,
          UINT                              StencilRef) {
    D3D10DepthStencilState* d3d10DepthStencilState = static_cast<D3D10DepthStencilState*>(pDepthStencilState);
    D3D11DepthStencilState* d3d11DepthStencilState = d3d10DepthStencilState ? d3d10DepthStencilState->GetD3D11Iface() : nullptr;

    m_context->OMSetDepthStencilState(d3d11DepthStencilState, StencilRef);
  }


  void STDMETHODCALLTYPE D3D10Device::OMGetRenderTargets(
          UINT                              NumViews,
          ID3D10RenderTargetView**          ppRenderTargetViews,
          ID3D10DepthStencilView**          ppDepthStencilView) {
    ID3D11RenderTargetView* d3d11Rtv[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
    ID3D11DepthStencilView* d3d11Dsv = nullptr;

    m_context->OMGetRenderTargets(NumViews,
      ppRenderTargetViews ? d3d11Rtv : nullptr,
      ppDepthStencilView ? &d3d11Dsv : nullptr);

    if (ppRenderTargetViews != nullptr) {
      for (uint32_t i = 0; i < NumViews; i++) {
        ppRenderTargetViews[i] = d3d11Rtv[i]
          ? static_cast<D3D11RenderTargetView*>(d3d11Rtv[i])->GetD3D10Iface()
          : nullptr;
      }
    }

    if (ppDepthStencilView)
      *ppDepthStencilView = d3d11Dsv ? static_cast<D3D11DepthStencilView*>(d3d11Dsv)->GetD3D10Iface() : nullptr;
  }


  void STDMETHODCALLTYPE D3D10Device::OMGetBlendState(
          ID3D10BlendState**                ppBlendState,
          FLOAT                             BlendFactor[4],
          UINT*                             pSampleMask) {
    ID3D11BlendState* d3d11BlendState = nullptr;

    m_context->OMGetBlendState(
      ppBlendState ? &d3d11BlendState : nullptr,
      BlendFactor, pSampleMask);

    if (ppBlendState != nullptr)
      *ppBlendState = d3d11BlendState ? static_cast<D3D11BlendState*>(d3d11BlendState)->GetD3D10Iface() : nullptr;
  }


  void STDMETHODCALLTYPE D3D10Device::OMGetDepthStencilState(
          ID3D10DepthStencilState**         ppDepthStencilState,
          UINT*                             pStencilRef) {
    ID3D11DepthStencilState* d3d11DepthStencilState = nullptr;

    m_context->OMGetDepthStencilState(
      ppDepthStencilState ? &d3d11DepthStencilState : nullptr,
      pStencilRef);

    if (ppDepthStencilState != nullptr)
      *ppDepthStencilState = d3d11DepthStencilState ? static_cast<D3D11DepthStencilState*>(d3d11DepthStencilState)->GetD3D10Iface() : nullptr;
  }


  void STDMETHODCALLTYPE D3D10Device::RSSetState(
          ID3D10RasterizerState*            pRasterizerState) {
    D3D10RasterizerState* d3d10RasterizerState = static_cast<D3D10RasterizerState*>(pRasterizerState);
    D3D11RasterizerState* d3d11RasterizerState = d3d10RasterizerState ? d3d10RasterizerState->GetD3D11Iface() : nullptr;

    m_context->RSSetState(d3d11RasterizerState);
  }


  void STDMETHODCALLTYPE D3D10Device::RSSetViewports(
          UINT                              NumViewports,
    const D3D10_VIEWPORT*                   pViewports) {
    D3D11_VIEWPORT vp[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

    if (NumViewports > D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
      return;

    for (uint32_t i = 0; i < NumViewports; i++) {
      vp[i].TopLeftX = float(pViewports[i].TopLeftX);
      vp[i].TopLeftY = float(pViewports[i].TopLeftY);
      vp[i].Width    = float(pViewports[i].Width);
      vp[i].Height   = float(pViewports[i].Height);
      vp[i].MinDepth = pViewports[i].MinDepth;
      vp[i].MaxDepth = pViewports[i].MaxDepth;
    }

    m_context->RSSetViewports(NumViewports, vp);
  }


  void STDMETHODCALLTYPE D3D10Device::RSSetScissorRects(
          UINT                              NumRects,
    const D3D10_RECT*                       pRects) {
    m_context->RSSetScissorRects(NumRects, pRects);
  }


  void STDMETHODCALLTYPE D3D10Device::RSGetState(
          ID3D10RasterizerState**           ppRasterizerState) {
    ID3D11RasterizerState* d3d11RasterizerState = nullptr;
    m_context->RSGetState(&d3d11RasterizerState);

    *ppRasterizerState = d3d11RasterizerState ? static_cast<D3D11RasterizerState*>(d3d11RasterizerState)->GetD3D10Iface() : nullptr;
  }


  void STDMETHODCALLTYPE D3D10Device::RSGetViewports(
          UINT*                             NumViewports,
          D3D10_VIEWPORT*                   pViewports) {
    D3D11_VIEWPORT vp[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    m_context->RSGetViewports(NumViewports, pViewports != nullptr ? vp : nullptr);

    if (pViewports != nullptr) {
      for (uint32_t i = 0; i < *NumViewports; i++) {
        pViewports[i].TopLeftX = int32_t(vp[i].TopLeftX);
        pViewports[i].TopLeftY = int32_t(vp[i].TopLeftY);
        pViewports[i].Width    = uint32_t(vp[i].Width);
        pViewports[i].Height   = uint32_t(vp[i].Height);
        pViewports[i].MinDepth = vp[i].MinDepth;
        pViewports[i].MaxDepth = vp[i].MaxDepth;
      }
    }
  }


  void STDMETHODCALLTYPE D3D10Device::RSGetScissorRects(
          UINT*                             NumRects,
          D3D10_RECT*                       pRects) {
    m_context->RSGetScissorRects(NumRects, pRects);
  }


  void STDMETHODCALLTYPE D3D10Device::SOSetTargets(
          UINT                              NumBuffers,
          ID3D10Buffer* const*              ppSOTargets,
    const UINT*                             pOffsets) {
    ID3D11Buffer* d3d11Buffers[D3D10_SO_BUFFER_SLOT_COUNT];

    if (NumBuffers > D3D10_SO_BUFFER_SLOT_COUNT)
      return;

    for (uint32_t i = 0; i < NumBuffers; i++) {
      d3d11Buffers[i] = ppSOTargets && ppSOTargets[i]
        ? static_cast<D3D10Buffer*>(ppSOTargets[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->SOSetTargets(NumBuffers, d3d11Buffers, pOffsets);
  }


  void STDMETHODCALLTYPE D3D10Device::SOGetTargets(
          UINT                              NumBuffers,
          ID3D10Buffer**                    ppSOTargets,
          UINT*                             pOffsets) {
    ID3D11Buffer* d3d11Buffers[D3D10_SO_BUFFER_SLOT_COUNT];

    m_context->SOGetTargetsWithOffsets(NumBuffers,
      ppSOTargets ? d3d11Buffers : nullptr,
      pOffsets);

    if (ppSOTargets != nullptr) {
      for (uint32_t i = 0; i < NumBuffers; i++) {
        ppSOTargets[i] = d3d11Buffers[i]
          ? static_cast<D3D11Buffer*>(d3d11Buffers[i])->GetD3D10Iface()
          : nullptr;
      }
    }
  }


  void STDMETHODCALLTYPE D3D10Device::SetTextFilterSize(
          UINT                              Width,
          UINT                              Height) {
    // D3D10 doesn't seem to actually store or do anything with these values,
    // as when calling GetTextFilterSize, it just makes the values 0.
  }


  void STDMETHODCALLTYPE D3D10Device::GetTextFilterSize(
          UINT*                             pWidth,
          UINT*                             pHeight) {
    if (pWidth)
        *pWidth = 0;

    if (pHeight)
        *pHeight = 0;
  }

}
