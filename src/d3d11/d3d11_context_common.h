#pragma once

#include <type_traits>
#include <vector>

#include "d3d11_buffer.h"
#include "d3d11_context.h"
#include "d3d11_texture.h"

namespace dxvk {

  class D3D11DeferredContext;
  class D3D11ImmediateContext;

  template<bool IsDeferred>
  struct D3D11ContextObjectForwarder;

  /**
   * \brief Object forwarder for immediate contexts
   *
   * Binding methods can use this to efficiently bind objects
   * to the DXVK context without redundant reference counting.
   */
  template<>
  struct D3D11ContextObjectForwarder<false> {
    template<typename T>
    static T&& move(T& object) {
      return std::move(object);
    }
  };

  /**
   * \brief Object forwarder for deferred contexts
   *
   * This forwarder will create a copy of the object passed
   * into it, so that CS chunks can be reused if necessary.
   */
  template<>
  struct D3D11ContextObjectForwarder<true> {
    template<typename T>
    static T move(const T& object) {
      return object;
    }
  };

  /**
   * \brief Common D3D11 device context implementation
   *
   * Implements all common device context methods, but since this is
   * templates with the actual context type (deferred or immediate),
   * all methods can call back into context-specific methods without
   * having to use virtual methods.
   */
  template<typename ContextType>
  class D3D11CommonContext : public D3D11DeviceContext {
    constexpr static bool IsDeferred = std::is_same_v<ContextType, D3D11DeferredContext>;
    using Forwarder = D3D11ContextObjectForwarder<IsDeferred>;
  public:
    
    D3D11CommonContext(
            D3D11Device*            pParent,
      const Rc<DxvkDevice>&         Device,
            DxvkCsChunkFlags        CsFlags);

    ~D3D11CommonContext();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject);

    void STDMETHODCALLTYPE ClearState();

    void STDMETHODCALLTYPE DiscardResource(ID3D11Resource *pResource);

    void STDMETHODCALLTYPE DiscardView(ID3D11View* pResourceView);

    void STDMETHODCALLTYPE DiscardView1(
            ID3D11View*                      pResourceView,
      const D3D11_RECT*                      pRects,
            UINT                             NumRects);

    void STDMETHODCALLTYPE UpdateSubresource(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
      const D3D11_BOX*                        pDstBox,
      const void*                             pSrcData,
            UINT                              SrcRowPitch,
            UINT                              SrcDepthPitch);

    void STDMETHODCALLTYPE UpdateSubresource1(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
      const D3D11_BOX*                        pDstBox,
      const void*                             pSrcData,
            UINT                              SrcRowPitch,
            UINT                              SrcDepthPitch,
            UINT                              CopyFlags);

    void STDMETHODCALLTYPE IASetInputLayout(
            ID3D11InputLayout*                pInputLayout);

    void STDMETHODCALLTYPE IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY          Topology);

    void STDMETHODCALLTYPE IASetVertexBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppVertexBuffers,
      const UINT*                             pStrides,
      const UINT*                             pOffsets);

    void STDMETHODCALLTYPE IASetIndexBuffer(
            ID3D11Buffer*                     pIndexBuffer,
            DXGI_FORMAT                       Format,
            UINT                              Offset);

    void STDMETHODCALLTYPE IAGetInputLayout(
            ID3D11InputLayout**               ppInputLayout);

    void STDMETHODCALLTYPE IAGetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY*         pTopology);

    void STDMETHODCALLTYPE IAGetVertexBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppVertexBuffers,
            UINT*                             pStrides,
            UINT*                             pOffsets);

    void STDMETHODCALLTYPE IAGetIndexBuffer(
            ID3D11Buffer**                    ppIndexBuffer,
            DXGI_FORMAT*                      pFormat,
            UINT*                             pOffset);

    void STDMETHODCALLTYPE VSSetShader(
            ID3D11VertexShader*               pVertexShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances);

    void STDMETHODCALLTYPE VSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers);

     void STDMETHODCALLTYPE VSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants);

    void STDMETHODCALLTYPE VSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews);

    void STDMETHODCALLTYPE VSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers);

    void STDMETHODCALLTYPE VSGetShader(
            ID3D11VertexShader**              ppVertexShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances);

    void STDMETHODCALLTYPE VSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers);

    void STDMETHODCALLTYPE VSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants);

    void STDMETHODCALLTYPE VSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews);

    void STDMETHODCALLTYPE VSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers);

    void STDMETHODCALLTYPE HSSetShader(
            ID3D11HullShader*                 pHullShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances);

    void STDMETHODCALLTYPE HSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers);

    void STDMETHODCALLTYPE HSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants);

    void STDMETHODCALLTYPE HSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews);

    void STDMETHODCALLTYPE HSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers);

    void STDMETHODCALLTYPE HSGetShader(
            ID3D11HullShader**                ppHullShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances);

    void STDMETHODCALLTYPE HSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers);

     void STDMETHODCALLTYPE HSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants);

    void STDMETHODCALLTYPE HSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews);

    void STDMETHODCALLTYPE HSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers);

    void STDMETHODCALLTYPE DSSetShader(
            ID3D11DomainShader*               pDomainShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances);

    void STDMETHODCALLTYPE DSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers);

    void STDMETHODCALLTYPE DSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants);

    void STDMETHODCALLTYPE DSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews);

    void STDMETHODCALLTYPE DSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers);

    void STDMETHODCALLTYPE DSGetShader(
            ID3D11DomainShader**              ppDomainShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances);

    void STDMETHODCALLTYPE DSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers);

     void STDMETHODCALLTYPE DSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants);

    void STDMETHODCALLTYPE DSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews);

    void STDMETHODCALLTYPE DSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers);

    void STDMETHODCALLTYPE GSSetShader(
            ID3D11GeometryShader*             pShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances);

    void STDMETHODCALLTYPE GSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers);

    void STDMETHODCALLTYPE GSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants);

    void STDMETHODCALLTYPE GSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews);

    void STDMETHODCALLTYPE GSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers);

    void STDMETHODCALLTYPE GSGetShader(
            ID3D11GeometryShader**            ppGeometryShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances);

    void STDMETHODCALLTYPE GSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers);

     void STDMETHODCALLTYPE GSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants);

    void STDMETHODCALLTYPE GSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews);

    void STDMETHODCALLTYPE GSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers);

    void STDMETHODCALLTYPE PSSetShader(
            ID3D11PixelShader*                pPixelShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances);

    void STDMETHODCALLTYPE PSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers);

    void STDMETHODCALLTYPE PSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants);

    void STDMETHODCALLTYPE PSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews);

    void STDMETHODCALLTYPE PSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers);

    void STDMETHODCALLTYPE PSGetShader(
            ID3D11PixelShader**               ppPixelShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances);

    void STDMETHODCALLTYPE PSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers);

    void STDMETHODCALLTYPE PSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants);

    void STDMETHODCALLTYPE PSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews);

    void STDMETHODCALLTYPE PSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers);

    void STDMETHODCALLTYPE CSSetShader(
            ID3D11ComputeShader*              pComputeShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances);

    void STDMETHODCALLTYPE CSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers);

    void STDMETHODCALLTYPE CSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants);

    void STDMETHODCALLTYPE CSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews);

    void STDMETHODCALLTYPE CSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers);

    void STDMETHODCALLTYPE CSSetUnorderedAccessViews(
            UINT                              StartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
      const UINT*                             pUAVInitialCounts);

    void STDMETHODCALLTYPE CSGetShader(
            ID3D11ComputeShader**             ppComputeShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances);

    void STDMETHODCALLTYPE CSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers);

    void STDMETHODCALLTYPE CSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants);

    void STDMETHODCALLTYPE CSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews);

    void STDMETHODCALLTYPE CSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers);

    void STDMETHODCALLTYPE CSGetUnorderedAccessViews(
            UINT                              StartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView**       ppUnorderedAccessViews);

    void STDMETHODCALLTYPE OMSetRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView);

    void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(
            UINT                              NumRTVs,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView,
            UINT                              UAVStartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
      const UINT*                             pUAVInitialCounts);

    void STDMETHODCALLTYPE OMSetBlendState(
            ID3D11BlendState*                 pBlendState,
      const FLOAT                             BlendFactor[4],
            UINT                              SampleMask);

    void STDMETHODCALLTYPE OMSetDepthStencilState(
            ID3D11DepthStencilState*          pDepthStencilState,
            UINT                              StencilRef);

    void STDMETHODCALLTYPE OMGetRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView**          ppRenderTargetViews,
            ID3D11DepthStencilView**          ppDepthStencilView);

    void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(
            UINT                              NumRTVs,
            ID3D11RenderTargetView**          ppRenderTargetViews,
            ID3D11DepthStencilView**          ppDepthStencilView,
            UINT                              UAVStartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView**       ppUnorderedAccessViews);

    void STDMETHODCALLTYPE OMGetBlendState(
            ID3D11BlendState**                ppBlendState,
            FLOAT                             BlendFactor[4],
            UINT*                             pSampleMask);

    void STDMETHODCALLTYPE OMGetDepthStencilState(
            ID3D11DepthStencilState**         ppDepthStencilState,
            UINT*                             pStencilRef);

    void STDMETHODCALLTYPE RSSetState(
            ID3D11RasterizerState*            pRasterizerState);

    void STDMETHODCALLTYPE RSSetViewports(
            UINT                              NumViewports,
      const D3D11_VIEWPORT*                   pViewports);

    void STDMETHODCALLTYPE RSSetScissorRects(
            UINT                              NumRects,
      const D3D11_RECT*                       pRects);

    void STDMETHODCALLTYPE RSGetState(
            ID3D11RasterizerState**           ppRasterizerState);

    void STDMETHODCALLTYPE RSGetViewports(
            UINT*                             pNumViewports,
            D3D11_VIEWPORT*                   pViewports);

    void STDMETHODCALLTYPE RSGetScissorRects(
            UINT*                             pNumRects,
            D3D11_RECT*                       pRects);

    void STDMETHODCALLTYPE SOSetTargets(
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppSOTargets,
      const UINT*                             pOffsets);

    void STDMETHODCALLTYPE SOGetTargets(
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppSOTargets);

    void STDMETHODCALLTYPE SOGetTargetsWithOffsets(
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppSOTargets,
            UINT*                             pOffsets);

    void STDMETHODCALLTYPE SetPredication(
            ID3D11Predicate*                  pPredicate,
            BOOL                              PredicateValue);

    void STDMETHODCALLTYPE GetPredication(
            ID3D11Predicate**                 ppPredicate,
            BOOL*                             pPredicateValue);

    BOOL STDMETHODCALLTYPE IsAnnotationEnabled();

  protected:

    D3D11DeviceContextExt<ContextType>        m_contextExt;
    D3D11UserDefinedAnnotation<ContextType>   m_annotation;

    template<DxbcProgramType ShaderStage>
    void BindShader(
      const D3D11CommonShader*                pShaderModule);

    void BindFramebuffer();

    void BindDrawBuffers(
            D3D11Buffer*                      pBufferForArgs,
            D3D11Buffer*                      pBufferForCount);

    void BindVertexBuffer(
            UINT                              Slot,
            D3D11Buffer*                      pBuffer,
            UINT                              Offset,
            UINT                              Stride);

    void BindIndexBuffer(
            D3D11Buffer*                      pBuffer,
            UINT                              Offset,
            DXGI_FORMAT                       Format);

    void BindXfbBuffer(
            UINT                              Slot,
            D3D11Buffer*                      pBuffer,
            UINT                              Offset);

    template<DxbcProgramType ShaderStage>
    void BindConstantBuffer(
            UINT                              Slot,
            D3D11Buffer*                      pBuffer,
            UINT                              Offset,
            UINT                              Length);

    template<DxbcProgramType ShaderStage>
    void BindConstantBufferRange(
            UINT                              Slot,
            UINT                              Offset,
            UINT                              Length);

    template<DxbcProgramType ShaderStage>
    void BindSampler(
            UINT                              Slot,
            D3D11SamplerState*                pSampler);

    template<DxbcProgramType ShaderStage>
    void BindShaderResource(
            UINT                              Slot,
            D3D11ShaderResourceView*          pResource);

    template<DxbcProgramType ShaderStage>
    void BindUnorderedAccessView(
            UINT                              UavSlot,
            D3D11UnorderedAccessView*         pUav,
            UINT                              CtrSlot,
            UINT                              Counter);

    void DiscardBuffer(
            ID3D11Resource*                   pResource);

    void DiscardTexture(
            ID3D11Resource*                   pResource,
            UINT                              Subresource);

    void GetConstantBuffers(
      const D3D11ConstantBufferBindings&      Bindings,
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants);

    void GetShaderResources(
      const D3D11ShaderResourceBindings&      Bindings,
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews);

    void GetSamplers(
      const D3D11SamplerBindings&             Bindings,
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers);

    void ResetState();

    template<DxbcProgramType ShaderStage, typename T>
    void ResolveSrvHazards(
            T*                                pView,
            D3D11ShaderResourceBindings&      Bindings);

    template<typename T>
    void ResolveCsSrvHazards(
            T*                                pView);

    template<typename T>
    void ResolveOmSrvHazards(
            T*                                pView);

    bool ResolveOmRtvHazards(
            D3D11UnorderedAccessView*         pView);

    void ResolveOmUavHazards(
            D3D11RenderTargetView*            pView);

    void RestoreState();
    
    template<DxbcProgramType Stage>
    void RestoreConstantBuffers(
            D3D11ConstantBufferBindings&      Bindings);
    
    template<DxbcProgramType Stage>
    void RestoreSamplers(
            D3D11SamplerBindings&             Bindings);
    
    template<DxbcProgramType Stage>
    void RestoreShaderResources(
            D3D11ShaderResourceBindings&      Bindings);
    
    template<DxbcProgramType Stage>
    void RestoreUnorderedAccessViews(
            D3D11UnorderedAccessBindings&     Bindings);
    
    template<DxbcProgramType ShaderStage>
    void SetConstantBuffers(
            D3D11ConstantBufferBindings&      Bindings,
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers);

    template<DxbcProgramType ShaderStage>
    void SetConstantBuffers1(
            D3D11ConstantBufferBindings&      Bindings,
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants);

    template<DxbcProgramType ShaderStage>
    void SetShaderResources(
            D3D11ShaderResourceBindings&      Bindings,
            UINT                              StartSlot,
            UINT                              NumResources,
            ID3D11ShaderResourceView* const*  ppResources);

    template<DxbcProgramType ShaderStage>
    void SetSamplers(
            D3D11SamplerBindings&             Bindings,
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers);

    void SetRenderTargetsAndUnorderedAccessViews(
            UINT                              NumRTVs,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView,
            UINT                              UAVStartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
      const UINT*                             pUAVInitialCounts);

    bool TestRtvUavHazards(
            UINT                              NumRTVs,
            ID3D11RenderTargetView* const*    ppRTVs,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUAVs);

    template<DxbcProgramType ShaderStage>
    bool TestSrvHazards(
            D3D11ShaderResourceView*          pView);

    void UpdateResource(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
      const D3D11_BOX*                        pDstBox,
      const void*                             pSrcData,
            UINT                              SrcRowPitch,
            UINT                              SrcDepthPitch,
            UINT                              CopyFlags);

    bool ValidateRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView);

  private:

    ContextType* GetTypedContext() {
      return static_cast<ContextType*>(this);
    }

  };
  
}
