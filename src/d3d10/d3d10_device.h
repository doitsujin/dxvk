#pragma once

#include "d3d10_multithread.h"

namespace dxvk {

  class D3D11Device;
  class D3D11ImmediateContext;

  class D3D10Device final : public ID3D10Device1 {

  public:

    D3D10Device(
            D3D11Device*                      pDevice,
            D3D11ImmediateContext*            pContext);
    
    ~D3D10Device();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                            riid,
            void**                            ppvObject);

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID                           guid,
            UINT*                             pDataSize,
            void*                             pData);

    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID                           guid,
            UINT                              DataSize,
      const void*                             pData);

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID                           guid,
      const IUnknown*                         pData);

    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason();

    HRESULT STDMETHODCALLTYPE SetExceptionMode(
            UINT                              RaiseFlags);

    UINT STDMETHODCALLTYPE GetExceptionMode();

    D3D10_FEATURE_LEVEL1 STDMETHODCALLTYPE GetFeatureLevel();

    void STDMETHODCALLTYPE ClearState();

    void STDMETHODCALLTYPE Flush();

    HRESULT STDMETHODCALLTYPE CreateBuffer(
      const D3D10_BUFFER_DESC*                pDesc,
      const D3D10_SUBRESOURCE_DATA*           pInitialData,
            ID3D10Buffer**                    ppBuffer);

    HRESULT STDMETHODCALLTYPE CreateTexture1D(
      const D3D10_TEXTURE1D_DESC*             pDesc,
      const D3D10_SUBRESOURCE_DATA*           pInitialData,
            ID3D10Texture1D**                 ppTexture1D);

    HRESULT STDMETHODCALLTYPE CreateTexture2D(
      const D3D10_TEXTURE2D_DESC*             pDesc,
      const D3D10_SUBRESOURCE_DATA*           pInitialData,
            ID3D10Texture2D**                 ppTexture2D);

    HRESULT STDMETHODCALLTYPE CreateTexture3D(
      const D3D10_TEXTURE3D_DESC*             pDesc,
      const D3D10_SUBRESOURCE_DATA*           pInitialData,
            ID3D10Texture3D**                 ppTexture3D);

    HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
            ID3D10Resource*                   pResource,
      const D3D10_SHADER_RESOURCE_VIEW_DESC*  pDesc,
            ID3D10ShaderResourceView**        ppSRView);

    HRESULT STDMETHODCALLTYPE CreateShaderResourceView1(
            ID3D10Resource*                   pResource,
      const D3D10_SHADER_RESOURCE_VIEW_DESC1* pDesc,
            ID3D10ShaderResourceView1**       ppSRView);

    HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
            ID3D10Resource*                   pResource,
      const D3D10_RENDER_TARGET_VIEW_DESC*    pDesc,
            ID3D10RenderTargetView**          ppRTView);

    HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
            ID3D10Resource*                   pResource,
      const D3D10_DEPTH_STENCIL_VIEW_DESC*    pDesc,
            ID3D10DepthStencilView**          ppDepthStencilView);

    HRESULT STDMETHODCALLTYPE CreateInputLayout(
      const D3D10_INPUT_ELEMENT_DESC*         pInputElementDescs,
            UINT                              NumElements,
      const void*                             pShaderBytecodeWithInputSignature,
            SIZE_T                            BytecodeLength,
            ID3D10InputLayout**               ppInputLayout);

    HRESULT STDMETHODCALLTYPE CreateVertexShader(
      const void*                             pShaderBytecode,
            SIZE_T                            BytecodeLength,
            ID3D10VertexShader**              ppVertexShader);

    HRESULT STDMETHODCALLTYPE CreateGeometryShader(
      const void*                             pShaderBytecode,
            SIZE_T                            BytecodeLength,
            ID3D10GeometryShader**            ppGeometryShader);

    HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
      const void*                             pShaderBytecode,
            SIZE_T                            BytecodeLength,
      const D3D10_SO_DECLARATION_ENTRY*       pSODeclaration,
            UINT                              NumEntries,
            UINT                              OutputStreamStride,
            ID3D10GeometryShader**            ppGeometryShader);

    HRESULT STDMETHODCALLTYPE CreatePixelShader(
      const void*                             pShaderBytecode,
            SIZE_T                            BytecodeLength,
            ID3D10PixelShader**               ppPixelShader);

    HRESULT STDMETHODCALLTYPE CreateBlendState(
      const D3D10_BLEND_DESC*                 pBlendStateDesc,
            ID3D10BlendState**                ppBlendState);

    HRESULT STDMETHODCALLTYPE CreateBlendState1(
      const D3D10_BLEND_DESC1*                pBlendStateDesc,
            ID3D10BlendState1**               ppBlendState);

    HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
      const D3D10_DEPTH_STENCIL_DESC*         pDepthStencilDesc,
            ID3D10DepthStencilState**         ppDepthStencilState);

    HRESULT STDMETHODCALLTYPE CreateRasterizerState(
      const D3D10_RASTERIZER_DESC*            pRasterizerDesc,
            ID3D10RasterizerState**           ppRasterizerState);

    HRESULT STDMETHODCALLTYPE CreateSamplerState(
      const D3D10_SAMPLER_DESC*               pSamplerDesc,
            ID3D10SamplerState**              ppSamplerState);

    HRESULT STDMETHODCALLTYPE CreateQuery(
      const D3D10_QUERY_DESC*                 pQueryDesc,
            ID3D10Query**                     ppQuery);

    HRESULT STDMETHODCALLTYPE CreatePredicate(
      const D3D10_QUERY_DESC*                 pPredicateDesc,
            ID3D10Predicate**                 ppPredicate);

    HRESULT STDMETHODCALLTYPE CreateCounter(
      const D3D10_COUNTER_DESC*               pCounterDesc,
            ID3D10Counter**                   ppCounter);

    HRESULT STDMETHODCALLTYPE CheckFormatSupport(
            DXGI_FORMAT                       Format,
            UINT*                             pFormatSupport);

    HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
            DXGI_FORMAT                       Format,
            UINT                              SampleCount,
            UINT*                             pNumQualityLevels);

    void STDMETHODCALLTYPE CheckCounterInfo(
            D3D10_COUNTER_INFO*               pCounterInfo);

    HRESULT STDMETHODCALLTYPE CheckCounter(
      const D3D10_COUNTER_DESC*               pDesc,
            D3D10_COUNTER_TYPE*               pType,
            UINT*                             pActiveCounters,
            char*                             name,
            UINT*                             pNameLength,
            char*                             units,
            UINT*                             pUnitsLength,
            char*                             description,
            UINT*                             pDescriptionLength);

    UINT STDMETHODCALLTYPE GetCreationFlags();

    HRESULT STDMETHODCALLTYPE OpenSharedResource(
            HANDLE                            hResource,
            REFIID                            ReturnedInterface,
            void**                            ppResource);

    void STDMETHODCALLTYPE ClearRenderTargetView(
            ID3D10RenderTargetView*           pRenderTargetView,
      const FLOAT                             ColorRGBA[4]);

    void STDMETHODCALLTYPE ClearDepthStencilView(
            ID3D10DepthStencilView*           pDepthStencilView,
            UINT                              ClearFlags,
            FLOAT                             Depth,
            UINT8                             Stencil);

    void STDMETHODCALLTYPE SetPredication(
            ID3D10Predicate*                  pPredicate,
            BOOL                              PredicateValue);

    void STDMETHODCALLTYPE GetPredication(
            ID3D10Predicate**                 ppPredicate,
            BOOL*                             pPredicateValue);

    void STDMETHODCALLTYPE CopySubresourceRegion(
            ID3D10Resource*                   pDstResource,
            UINT                              DstSubresource,
            UINT                              DstX,
            UINT                              DstY,
            UINT                              DstZ,
            ID3D10Resource*                   pSrcResource,
            UINT                              SrcSubresource,
      const D3D10_BOX*                        pSrcBox);

    void STDMETHODCALLTYPE CopyResource(
            ID3D10Resource*                   pDstResource,
            ID3D10Resource*                   pSrcResource);

    void STDMETHODCALLTYPE UpdateSubresource(
            ID3D10Resource*                   pDstResource,
            UINT                              DstSubresource,
      const D3D10_BOX*                        pDstBox,
      const void*                             pSrcData,
            UINT                              SrcRowPitch,
            UINT                              SrcDepthPitch);

    void STDMETHODCALLTYPE GenerateMips(
            ID3D10ShaderResourceView*         pShaderResourceView);

    void STDMETHODCALLTYPE ResolveSubresource(
            ID3D10Resource*                   pDstResource,
            UINT                              DstSubresource,
            ID3D10Resource*                   pSrcResource,
            UINT                              SrcSubresource,
            DXGI_FORMAT                       Format);

    void STDMETHODCALLTYPE Draw(
            UINT                              VertexCount,
            UINT                              StartVertexLocation);

    void STDMETHODCALLTYPE DrawIndexed(
            UINT                              IndexCount,
            UINT                              StartIndexLocation,
            INT                               BaseVertexLocation);

    void STDMETHODCALLTYPE DrawInstanced(
            UINT                              VertexCountPerInstance,
            UINT                              InstanceCount,
            UINT                              StartVertexLocation,
            UINT                              StartInstanceLocation);

    void STDMETHODCALLTYPE DrawIndexedInstanced(
            UINT                              IndexCountPerInstance,
            UINT                              InstanceCount,
            UINT                              StartIndexLocation,
            INT                               BaseVertexLocation,
            UINT                              StartInstanceLocation);

    void STDMETHODCALLTYPE DrawAuto();

    void STDMETHODCALLTYPE IASetInputLayout(
            ID3D10InputLayout*                pInputLayout);

    void STDMETHODCALLTYPE IASetPrimitiveTopology(
            D3D10_PRIMITIVE_TOPOLOGY          Topology);

    void STDMETHODCALLTYPE IASetVertexBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D10Buffer* const*              ppVertexBuffers,
      const UINT*                             pStrides,
      const UINT*                             pOffsets);

    void STDMETHODCALLTYPE IASetIndexBuffer(
            ID3D10Buffer*                     pIndexBuffer,
            DXGI_FORMAT                       Format,
            UINT                              Offset);

    void STDMETHODCALLTYPE IAGetInputLayout(
            ID3D10InputLayout**               ppInputLayout);

    void STDMETHODCALLTYPE IAGetPrimitiveTopology(
            D3D10_PRIMITIVE_TOPOLOGY*         pTopology);

    void STDMETHODCALLTYPE IAGetVertexBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D10Buffer**                    ppVertexBuffers,
            UINT*                             pStrides,
            UINT*                             pOffsets);

    void STDMETHODCALLTYPE IAGetIndexBuffer(
            ID3D10Buffer**                    pIndexBuffer,
            DXGI_FORMAT*                      Format,
            UINT*                             Offset);

    void STDMETHODCALLTYPE VSSetShader(
            ID3D10VertexShader*               pVertexShader);

    void STDMETHODCALLTYPE VSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D10Buffer* const*              ppConstantBuffers);

    void STDMETHODCALLTYPE VSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D10ShaderResourceView* const*  ppShaderResourceViews);

    void STDMETHODCALLTYPE VSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D10SamplerState* const*        ppSamplers);

    void STDMETHODCALLTYPE VSGetShader(
            ID3D10VertexShader**              ppVertexShader);

    void STDMETHODCALLTYPE VSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D10Buffer**                    ppConstantBuffers);

    void STDMETHODCALLTYPE VSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D10ShaderResourceView**        ppShaderResourceViews);

    void STDMETHODCALLTYPE VSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D10SamplerState**              ppSamplers);

    void STDMETHODCALLTYPE GSSetShader(
            ID3D10GeometryShader*             pShader);

    void STDMETHODCALLTYPE GSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D10Buffer* const*              ppConstantBuffers);

    void STDMETHODCALLTYPE GSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D10ShaderResourceView* const*  ppShaderResourceViews);

    void STDMETHODCALLTYPE GSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D10SamplerState* const*        ppSamplers);

    void STDMETHODCALLTYPE GSGetShader(
            ID3D10GeometryShader**            ppGeometryShader);

    void STDMETHODCALLTYPE GSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D10Buffer**                    ppConstantBuffers);

    void STDMETHODCALLTYPE GSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D10ShaderResourceView**        ppShaderResourceViews);

    void STDMETHODCALLTYPE GSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D10SamplerState**              ppSamplers);

    void STDMETHODCALLTYPE PSSetShader(
            ID3D10PixelShader*                pPixelShader);

    void STDMETHODCALLTYPE PSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D10Buffer* const*              ppConstantBuffers);

    void STDMETHODCALLTYPE PSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D10ShaderResourceView* const*  ppShaderResourceViews);

    void STDMETHODCALLTYPE PSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D10SamplerState* const*        ppSamplers);

    void STDMETHODCALLTYPE PSGetShader(
            ID3D10PixelShader**               ppPixelShader);

    void STDMETHODCALLTYPE PSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D10Buffer**                    ppConstantBuffers);

    void STDMETHODCALLTYPE PSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D10ShaderResourceView**        ppShaderResourceViews);

    void STDMETHODCALLTYPE PSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D10SamplerState**              ppSamplers);

    void STDMETHODCALLTYPE OMSetRenderTargets(
            UINT                              NumViews,
            ID3D10RenderTargetView* const*    ppRenderTargetViews,
            ID3D10DepthStencilView*           pDepthStencilView);

    void STDMETHODCALLTYPE OMSetBlendState(
            ID3D10BlendState*                 pBlendState,
      const FLOAT                             BlendFactor[4],
            UINT                              SampleMask);

    void STDMETHODCALLTYPE OMSetDepthStencilState(
            ID3D10DepthStencilState*          pDepthStencilState,
            UINT                              StencilRef);

    void STDMETHODCALLTYPE OMGetRenderTargets(
            UINT                              NumViews,
            ID3D10RenderTargetView**          ppRenderTargetViews,
            ID3D10DepthStencilView**          ppDepthStencilView);

    void STDMETHODCALLTYPE OMGetBlendState(
            ID3D10BlendState**                ppBlendState,
            FLOAT                             BlendFactor[4],
            UINT*                             pSampleMask);

    void STDMETHODCALLTYPE OMGetDepthStencilState(
            ID3D10DepthStencilState**         ppDepthStencilState,
            UINT*                             pStencilRef);

    void STDMETHODCALLTYPE RSSetState(
            ID3D10RasterizerState*            pRasterizerState);

    void STDMETHODCALLTYPE RSSetViewports(
            UINT                              NumViewports,
      const D3D10_VIEWPORT*                   pViewports);

    void STDMETHODCALLTYPE RSSetScissorRects(
            UINT                              NumRects,
      const D3D10_RECT*                       pRects);

    void STDMETHODCALLTYPE RSGetState(
            ID3D10RasterizerState**           ppRasterizerState);

    void STDMETHODCALLTYPE RSGetViewports(
            UINT*                             NumViewports,
            D3D10_VIEWPORT*                   pViewports);

    void STDMETHODCALLTYPE RSGetScissorRects(
            UINT*                             NumRects,
            D3D10_RECT*                       pRects);

    void STDMETHODCALLTYPE SOSetTargets(
            UINT                              NumBuffers,
            ID3D10Buffer* const*              ppSOTargets,
      const UINT*                             pOffsets);

    void STDMETHODCALLTYPE SOGetTargets(
            UINT                              NumBuffers,
            ID3D10Buffer**                    ppSOTargets,
            UINT*                             pOffsets);

    void STDMETHODCALLTYPE SetTextFilterSize(
            UINT                              Width,
            UINT                              Height);

    void STDMETHODCALLTYPE GetTextFilterSize(
            UINT*                             pWidth,
            UINT*                             pHeight);

  private:

    D3D11Device*            m_device;
    D3D11ImmediateContext*  m_context;

  };

}