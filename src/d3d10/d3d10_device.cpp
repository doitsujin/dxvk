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
    
    if (FAILED(hr))
      return hr;
    
    if (ppBuffer != nullptr) {
      *ppBuffer = static_cast<D3D11Buffer*>(d3d11Buffer)->GetD3D10Iface();
      return S_OK;
    } return S_FALSE;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture1D(
    const D3D10_TEXTURE1D_DESC*             pDesc,
    const D3D10_SUBRESOURCE_DATA*           pInitialData,
          ID3D10Texture1D**                 ppTexture1D) {
    InitReturnPtr(ppTexture1D);

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
    
    if (FAILED(hr))
      return hr;

    if (ppTexture1D != nullptr) {
      *ppTexture1D = static_cast<D3D11Texture1D*>(d3d11Texture1D)->GetD3D10Iface();
      return S_OK;
    } return S_FALSE;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture2D(
    const D3D10_TEXTURE2D_DESC*             pDesc,
    const D3D10_SUBRESOURCE_DATA*           pInitialData,
          ID3D10Texture2D**                 ppTexture2D) {
    InitReturnPtr(ppTexture2D);

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

    if (FAILED(hr))
      return hr;

    if (ppTexture2D != nullptr) {
      *ppTexture2D = static_cast<D3D11Texture2D*>(d3d11Texture2D)->GetD3D10Iface();
      return S_OK;
    } return S_FALSE;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture3D(
    const D3D10_TEXTURE3D_DESC*             pDesc,
    const D3D10_SUBRESOURCE_DATA*           pInitialData,
          ID3D10Texture3D**                 ppTexture3D) {
    InitReturnPtr(ppTexture3D);

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

    if (FAILED(hr))
      return hr;

    if (ppTexture3D != nullptr) {
      *ppTexture3D = static_cast<D3D11Texture3D*>(d3d11Texture3D)->GetD3D10Iface();
      return S_OK;
    } return S_FALSE;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView(
          ID3D10Resource*                   pResource,
    const D3D10_SHADER_RESOURCE_VIEW_DESC*  pDesc,
          ID3D10ShaderResourceView**        ppSRView) {
    Logger::err("D3D10Device::CreateShaderResourceView: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView1(
          ID3D10Resource*                   pResource,
    const D3D10_SHADER_RESOURCE_VIEW_DESC1* pDesc,
          ID3D10ShaderResourceView1**       ppSRView) {
    Logger::err("D3D10Device::CreateShaderResourceView1: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateRenderTargetView(
          ID3D10Resource*                   pResource,
    const D3D10_RENDER_TARGET_VIEW_DESC*    pDesc,
          ID3D10RenderTargetView**          ppRTView) {
    Logger::err("D3D10Device::CreateRenderTargetView: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilView(
          ID3D10Resource*                   pResource,
    const D3D10_DEPTH_STENCIL_VIEW_DESC*    pDesc,
          ID3D10DepthStencilView**          ppDepthStencilView) {
    Logger::err("D3D10Device::CreateDepthStencilView: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateInputLayout(
    const D3D10_INPUT_ELEMENT_DESC*         pInputElementDescs,
          UINT                              NumElements,
    const void*                             pShaderBytecodeWithInputSignature,
          SIZE_T                            BytecodeLength,
          ID3D10InputLayout**               ppInputLayout) {
    Logger::err("D3D10Device::CreateInputLayout: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateVertexShader(
    const void*                             pShaderBytecode,
          SIZE_T                            BytecodeLength,
          ID3D10VertexShader**              ppVertexShader) {
    Logger::err("D3D10Device::CreateVertexShader: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShader(
    const void*                             pShaderBytecode,
          SIZE_T                            BytecodeLength,
          ID3D10GeometryShader**            ppGeometryShader) {
    Logger::err("D3D10Device::CreateGeometryShader: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShaderWithStreamOutput(
    const void*                             pShaderBytecode,
          SIZE_T                            BytecodeLength,
    const D3D10_SO_DECLARATION_ENTRY*       pSODeclaration,
          UINT                              NumEntries,
          UINT                              OutputStreamStride,
          ID3D10GeometryShader**            ppGeometryShader) {
    Logger::err("D3D10Device::CreateGeometryShaderWithStreamOutput: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreatePixelShader(
    const void*                             pShaderBytecode,
          SIZE_T                            BytecodeLength,
          ID3D10PixelShader**               ppPixelShader) {
    Logger::err("D3D10Device::CreatePixelShader: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState(
    const D3D10_BLEND_DESC*                 pBlendStateDesc,
          ID3D10BlendState**                ppBlendState) {
    Logger::err("D3D10Device::CreateBlendState: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState1(
    const D3D10_BLEND_DESC1*                pBlendStateDesc,
          ID3D10BlendState1**               ppBlendState) {
    Logger::err("D3D10Device::CreateBlendState1: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilState(
    const D3D10_DEPTH_STENCIL_DESC*         pDepthStencilDesc,
          ID3D10DepthStencilState**         ppDepthStencilState) {
    Logger::err("D3D10Device::CreateDepthStencilState: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateRasterizerState(
    const D3D10_RASTERIZER_DESC*            pRasterizerDesc,
          ID3D10RasterizerState**           ppRasterizerState) {
    Logger::err("D3D10Device::CreateRasterizerState: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateSamplerState(
    const D3D10_SAMPLER_DESC*               pSamplerDesc,
          ID3D10SamplerState**              ppSamplerState) {
    InitReturnPtr(ppSamplerState);

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
    
    if (FAILED(hr))
      return hr;

    if (ppSamplerState) {
      *ppSamplerState = static_cast<D3D11SamplerState*>(d3d11SamplerState)->GetD3D10Iface();
      return S_OK;
    } return S_FALSE;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreateQuery(
    const D3D10_QUERY_DESC*                 pQueryDesc,
          ID3D10Query**                     ppQuery) {
    Logger::err("D3D10Device::CreateQuery: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D10Device::CreatePredicate(
    const D3D10_QUERY_DESC*                 pPredicateDesc,
          ID3D10Predicate**                 ppPredicate) {
    Logger::err("D3D10Device::CreatePredicate: Not implemented");
    return E_NOTIMPL;
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
    Logger::err("D3D10Device::CheckFormatSupport: Not implemented");
    return E_NOTIMPL;
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
    Logger::err("D3D10Device::ClearRenderTargetView: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::ClearDepthStencilView(
          ID3D10DepthStencilView*           pDepthStencilView,
          UINT                              ClearFlags,
          FLOAT                             Depth,
          UINT8                             Stencil) {
    Logger::err("D3D10Device::ClearDepthStencilView: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::SetPredication(
          ID3D10Predicate*                  pPredicate,
          BOOL                              PredicateValue) {
    Logger::err("D3D10Device::SetPredication: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::GetPredication(
          ID3D10Predicate**                 ppPredicate,
          BOOL*                             pPredicateValue) {
    Logger::err("D3D10Device::GetPredication: Not implemented");
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
    Logger::err("D3D10Device::CopySubresourceRegion: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::CopyResource(
          ID3D10Resource*                   pDstResource,
          ID3D10Resource*                   pSrcResource) {
    Logger::err("D3D10Device::CopyResource: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::UpdateSubresource(
          ID3D10Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D10_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch) {
    Logger::err("D3D10Device::UpdateSubresource: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::GenerateMips(
          ID3D10ShaderResourceView*         pShaderResourceView) {
    Logger::err("D3D10Device::GenerateMips: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::ResolveSubresource(
          ID3D10Resource*                   pDstResource,
          UINT                              DstSubresource,
          ID3D10Resource*                   pSrcResource,
          UINT                              SrcSubresource,
          DXGI_FORMAT                       Format) {
    Logger::err("D3D10Device::ResolveSubresource: Not implemented");
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
    Logger::err("D3D10Device::IASetInputLayout: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::IASetPrimitiveTopology(
          D3D10_PRIMITIVE_TOPOLOGY          Topology) {
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY(Topology));
  }


  void STDMETHODCALLTYPE D3D10Device::IASetVertexBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer* const*              ppVertexBuffers,
    const UINT*                             pStrides,
    const UINT*                             pOffsets) {
    ID3D11Buffer* d3d11Buffers[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

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
    D3D11Buffer* d3d11Buffer = d3d10Buffer
      ? d3d10Buffer->GetD3D11Iface()
      : nullptr;

    m_context->IASetIndexBuffer(d3d11Buffer, Format, Offset);
  }


  void STDMETHODCALLTYPE D3D10Device::IAGetInputLayout(
          ID3D10InputLayout**               ppInputLayout) {
    Logger::err("D3D10Device::IAGetInputLayout: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::IAGetPrimitiveTopology(
          D3D10_PRIMITIVE_TOPOLOGY*         pTopology) {
    D3D11_PRIMITIVE_TOPOLOGY d3d11Topology;
    m_context->IAGetPrimitiveTopology(&d3d11Topology);

    *pTopology = d3d11Topology <= 32 /* begin patch list */
      ? D3D10_PRIMITIVE_TOPOLOGY(d3d11Topology)
      : D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
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
    
    if (ppVertexBuffers != nullptr) {
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
      *pIndexBuffer = static_cast<D3D11Buffer*>(d3d11Buffer)->GetD3D10Iface();
  }


  void STDMETHODCALLTYPE D3D10Device::VSSetShader(
          ID3D10VertexShader*               pVertexShader) {
    Logger::err("D3D10Device::VSSetShader: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::VSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer* const*              ppConstantBuffers) {
    ID3D11Buffer* d3d11Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

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
    Logger::err("D3D10Device::VSSetShaderResources: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::VSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D10SamplerState* const*        ppSamplers) {
    ID3D11SamplerState* d3d11Samplers[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];

    for (uint32_t i = 0; i < NumSamplers; i++) {
      d3d11Samplers[i] = ppSamplers && ppSamplers[i]
        ? static_cast<D3D10SamplerState*>(ppSamplers[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->VSSetSamplers(StartSlot, NumSamplers, d3d11Samplers);
  }


  void STDMETHODCALLTYPE D3D10Device::VSGetShader(
          ID3D10VertexShader**              ppVertexShader) {
    Logger::err("D3D10Device::VSGetShader: Not implemented");
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
    Logger::err("D3D10Device::VSGetShaderResources: Not implemented");
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
    Logger::err("D3D10Device::GSSetShader: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::GSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer* const*              ppConstantBuffers) {
    ID3D11Buffer* d3d11Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

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
    Logger::err("D3D10Device::GSSetShaderResources: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::GSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D10SamplerState* const*        ppSamplers) {
    ID3D11SamplerState* d3d11Samplers[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];

    for (uint32_t i = 0; i < NumSamplers; i++) {
      d3d11Samplers[i] = ppSamplers && ppSamplers[i]
        ? static_cast<D3D10SamplerState*>(ppSamplers[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->GSSetSamplers(StartSlot, NumSamplers, d3d11Samplers);
  }


  void STDMETHODCALLTYPE D3D10Device::GSGetShader(
          ID3D10GeometryShader**            ppGeometryShader) {
    Logger::err("D3D10Device::GSGetShader: Not implemented");
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
    Logger::err("D3D10Device::GSGetShaderResources: Not implemented");
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
    Logger::err("D3D10Device::PSSetShader: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::PSSetConstantBuffers(
          UINT                              StartSlot,
          UINT                              NumBuffers,
          ID3D10Buffer* const*              ppConstantBuffers) {
    ID3D11Buffer* d3d11Buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

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
    Logger::err("D3D10Device::PSSetConstantBuffers: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::PSSetSamplers(
          UINT                              StartSlot,
          UINT                              NumSamplers,
          ID3D10SamplerState* const*        ppSamplers) {
    ID3D11SamplerState* d3d11Samplers[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];

    for (uint32_t i = 0; i < NumSamplers; i++) {
      d3d11Samplers[i] = ppSamplers && ppSamplers[i]
        ? static_cast<D3D10SamplerState*>(ppSamplers[i])->GetD3D11Iface()
        : nullptr;
    }

    m_context->PSSetSamplers(StartSlot, NumSamplers, d3d11Samplers);
  }


  void STDMETHODCALLTYPE D3D10Device::PSGetShader(
          ID3D10PixelShader**               ppPixelShader) {
    Logger::err("D3D10Device::PSGetShader: Not implemented");
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
    Logger::err("D3D10Device::PSGetShaderResources: Not implemented");
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
    Logger::err("D3D10Device::OMSetRenderTargets: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::OMSetBlendState(
          ID3D10BlendState*                 pBlendState,
    const FLOAT                             BlendFactor[4],
          UINT                              SampleMask) {
    Logger::err("D3D10Device::OMSetBlendState: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::OMSetDepthStencilState(
          ID3D10DepthStencilState*          pDepthStencilState,
          UINT                              StencilRef) {
    Logger::err("D3D10Device::OMSetDepthStencilState: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::OMGetRenderTargets(
          UINT                              NumViews,
          ID3D10RenderTargetView**          ppRenderTargetViews,
          ID3D10DepthStencilView**          ppDepthStencilView) {
    Logger::err("D3D10Device::OMGetRenderTargets: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::OMGetBlendState(
          ID3D10BlendState**                ppBlendState,
          FLOAT                             BlendFactor[4],
          UINT*                             pSampleMask) {
    Logger::err("D3D10Device::OMGetBlendState: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::OMGetDepthStencilState(
          ID3D10DepthStencilState**         ppDepthStencilState,
          UINT*                             pStencilRef) {
    Logger::err("D3D10Device::OMGetDepthStencilState: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::RSSetState(
          ID3D10RasterizerState*            pRasterizerState) {
    Logger::err("D3D10Device::RSSetState: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::RSSetViewports(
          UINT                              NumViewports,
    const D3D10_VIEWPORT*                   pViewports) {
    D3D11_VIEWPORT vp[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

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
    Logger::err("D3D10Device::RSGetState: Not implemented");
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
    m_context->SOGetTargets(NumBuffers, ppSOTargets ? d3d11Buffers : nullptr);

    if (ppSOTargets != nullptr) {
      for (uint32_t i = 0; i < NumBuffers; i++) {
        ppSOTargets[i] = d3d11Buffers[i]
          ? static_cast<D3D11Buffer*>(d3d11Buffers[i])->GetD3D10Iface()
          : nullptr;
      }
    }

    if (pOffsets != nullptr)
      Logger::warn("D3D10: SOGetTargets: Reporting buffer offsets not supported");
  }


  void STDMETHODCALLTYPE D3D10Device::SetTextFilterSize(
          UINT                              Width,
          UINT                              Height) {
    Logger::err("D3D10Device::SetTextFilterSize: Not implemented");
  }


  void STDMETHODCALLTYPE D3D10Device::GetTextFilterSize(
          UINT*                             pWidth,
          UINT*                             pHeight) {
    Logger::err("D3D10Device::GetTextFilterSize: Not implemented");
  }

}