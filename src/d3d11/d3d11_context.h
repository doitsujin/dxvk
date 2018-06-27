#pragma once

#include "../dxvk/dxvk_adapter.h"
#include "../dxvk/dxvk_cs.h"
#include "../dxvk/dxvk_device.h"

#include "d3d11_annotation.h"
#include "d3d11_context_state.h"
#include "d3d11_device_child.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11DeviceContext : public D3D11DeviceChild<ID3D11DeviceContext1> {
    
  public:
    
    D3D11DeviceContext(
            D3D11Device*    pParent,
      const Rc<DxvkDevice>& Device);
    ~D3D11DeviceContext();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void STDMETHODCALLTYPE DiscardResource(ID3D11Resource *pResource) final;

    void STDMETHODCALLTYPE DiscardView(ID3D11View* pResourceView) final;

    void STDMETHODCALLTYPE DiscardView1(
            ID3D11View*                      pResourceView,
      const D3D11_RECT*                      pRects,
            UINT                             NumRects) final;

    void STDMETHODCALLTYPE SwapDeviceContextState(
           ID3DDeviceContextState*           pState,
           ID3DDeviceContextState**          ppPreviousState) final;

    void STDMETHODCALLTYPE GetDevice(ID3D11Device **ppDevice) final;
    
    void STDMETHODCALLTYPE ClearState() final;
    
    void STDMETHODCALLTYPE Begin(ID3D11Asynchronous *pAsync) final;
    
    void STDMETHODCALLTYPE End(ID3D11Asynchronous *pAsync) final;
    
    void STDMETHODCALLTYPE SetPredication(
            ID3D11Predicate*                  pPredicate,
            BOOL                              PredicateValue) final;
    
    void STDMETHODCALLTYPE GetPredication(
            ID3D11Predicate**                 ppPredicate,
            BOOL*                             pPredicateValue) final;
    
    void STDMETHODCALLTYPE CopySubresourceRegion(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
            UINT                              DstX,
            UINT                              DstY,
            UINT                              DstZ,
            ID3D11Resource*                   pSrcResource,
            UINT                              SrcSubresource,
      const D3D11_BOX*                        pSrcBox);
    
    void STDMETHODCALLTYPE CopySubresourceRegion1(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
            UINT                              DstX,
            UINT                              DstY,
            UINT                              DstZ,
            ID3D11Resource*                   pSrcResource,
            UINT                              SrcSubresource,
      const D3D11_BOX*                        pSrcBox,
            UINT                              CopyFlags);
    
    void STDMETHODCALLTYPE CopyResource(
            ID3D11Resource*                   pDstResource,
            ID3D11Resource*                   pSrcResource);
    
    void STDMETHODCALLTYPE CopyStructureCount(
            ID3D11Buffer*                     pDstBuffer,
            UINT                              DstAlignedByteOffset,
            ID3D11UnorderedAccessView*        pSrcView) final;
    
    void STDMETHODCALLTYPE ClearRenderTargetView(
            ID3D11RenderTargetView*           pRenderTargetView,
      const FLOAT                             ColorRGBA[4]) final;
    
    void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
            ID3D11UnorderedAccessView*        pUnorderedAccessView,
      const UINT                              Values[4]) final;
    
    void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
            ID3D11UnorderedAccessView*        pUnorderedAccessView,
      const FLOAT                             Values[4]) final;
    
    void STDMETHODCALLTYPE ClearDepthStencilView(
            ID3D11DepthStencilView*           pDepthStencilView,
            UINT                              ClearFlags,
            FLOAT                             Depth,
            UINT8                             Stencil) final;
    
    void STDMETHODCALLTYPE ClearView(
            ID3D11View                        *pView,
      const FLOAT                             Color[4],
      const D3D11_RECT                        *pRect,
            UINT                              NumRects) final;

    void STDMETHODCALLTYPE GenerateMips(
            ID3D11ShaderResourceView*         pShaderResourceView);
    
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
    
    void STDMETHODCALLTYPE SetResourceMinLOD(
            ID3D11Resource*                   pResource,
            FLOAT                             MinLOD) final;
    
    FLOAT STDMETHODCALLTYPE GetResourceMinLOD(
            ID3D11Resource*                   pResource) final;
    
    void STDMETHODCALLTYPE ResolveSubresource(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
            ID3D11Resource*                   pSrcResource,
            UINT                              SrcSubresource,
            DXGI_FORMAT                       Format);
    
    void STDMETHODCALLTYPE DrawAuto() final;
    
    void STDMETHODCALLTYPE Draw(
            UINT            VertexCount,
            UINT            StartVertexLocation) final;
    
    void STDMETHODCALLTYPE DrawIndexed(
            UINT            IndexCount,
            UINT            StartIndexLocation,
            INT             BaseVertexLocation) final;
    
    void STDMETHODCALLTYPE DrawInstanced(
            UINT            VertexCountPerInstance,
            UINT            InstanceCount,
            UINT            StartVertexLocation,
            UINT            StartInstanceLocation) final;
    
    void STDMETHODCALLTYPE DrawIndexedInstanced(
            UINT            IndexCountPerInstance,
            UINT            InstanceCount,
            UINT            StartIndexLocation,
            INT             BaseVertexLocation,
            UINT            StartInstanceLocation) final;
    
    void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(
            ID3D11Buffer*   pBufferForArgs,
            UINT            AlignedByteOffsetForArgs) final;
    
    void STDMETHODCALLTYPE DrawInstancedIndirect(
            ID3D11Buffer*   pBufferForArgs,
            UINT            AlignedByteOffsetForArgs) final;
    
    void STDMETHODCALLTYPE Dispatch(
            UINT            ThreadGroupCountX,
            UINT            ThreadGroupCountY,
            UINT            ThreadGroupCountZ) final;
    
    void STDMETHODCALLTYPE DispatchIndirect(
            ID3D11Buffer*   pBufferForArgs,
            UINT            AlignedByteOffsetForArgs) final;
    
    void STDMETHODCALLTYPE IASetInputLayout(
            ID3D11InputLayout*                pInputLayout) final;
    
    void STDMETHODCALLTYPE IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY          Topology) final;
    
    void STDMETHODCALLTYPE IASetVertexBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppVertexBuffers,
      const UINT*                             pStrides,
      const UINT*                             pOffsets) final;
    
    void STDMETHODCALLTYPE IASetIndexBuffer(
            ID3D11Buffer*                     pIndexBuffer,
            DXGI_FORMAT                       Format,
            UINT                              Offset) final;
    
    void STDMETHODCALLTYPE IAGetInputLayout(
            ID3D11InputLayout**               ppInputLayout) final;
    
    void STDMETHODCALLTYPE IAGetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY*         pTopology) final;
    
    void STDMETHODCALLTYPE IAGetVertexBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppVertexBuffers,
            UINT*                             pStrides,
            UINT*                             pOffsets) final;
    
    void STDMETHODCALLTYPE IAGetIndexBuffer(
            ID3D11Buffer**                    ppIndexBuffer,
            DXGI_FORMAT*                      pFormat,
            UINT*                             pOffset) final;
    
    void STDMETHODCALLTYPE VSSetShader(
            ID3D11VertexShader*               pVertexShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void STDMETHODCALLTYPE VSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;

     void STDMETHODCALLTYPE VSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants) final;
    
    void STDMETHODCALLTYPE VSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE VSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void STDMETHODCALLTYPE VSGetShader(
            ID3D11VertexShader**              ppVertexShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void STDMETHODCALLTYPE VSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;

    void STDMETHODCALLTYPE VSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants) final;
    
    void STDMETHODCALLTYPE VSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE VSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void STDMETHODCALLTYPE HSSetShader(
            ID3D11HullShader*                 pHullShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void STDMETHODCALLTYPE HSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE HSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;
    
    void STDMETHODCALLTYPE HSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants) final;
    
    void STDMETHODCALLTYPE HSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void STDMETHODCALLTYPE HSGetShader(
            ID3D11HullShader**                ppHullShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void STDMETHODCALLTYPE HSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;

     void STDMETHODCALLTYPE HSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants) final;
    
    void STDMETHODCALLTYPE HSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE HSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void STDMETHODCALLTYPE DSSetShader(
            ID3D11DomainShader*               pDomainShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void STDMETHODCALLTYPE DSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE DSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;

    void STDMETHODCALLTYPE DSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants) final;
    
    void STDMETHODCALLTYPE DSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void STDMETHODCALLTYPE DSGetShader(
            ID3D11DomainShader**              ppDomainShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void STDMETHODCALLTYPE DSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;
    
     void STDMETHODCALLTYPE DSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants) final;
    
    void STDMETHODCALLTYPE DSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE DSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void STDMETHODCALLTYPE GSSetShader(
            ID3D11GeometryShader*             pShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void STDMETHODCALLTYPE GSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;
    
    virtual void STDMETHODCALLTYPE GSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants) final;

    void STDMETHODCALLTYPE GSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE GSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void STDMETHODCALLTYPE GSGetShader(
            ID3D11GeometryShader**            ppGeometryShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void STDMETHODCALLTYPE GSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;
    
     void STDMETHODCALLTYPE GSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants) final;
    
    void STDMETHODCALLTYPE GSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE GSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void STDMETHODCALLTYPE PSSetShader(
            ID3D11PixelShader*                pPixelShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void STDMETHODCALLTYPE PSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;

    void STDMETHODCALLTYPE PSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants) final;
    
    void STDMETHODCALLTYPE PSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE PSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void STDMETHODCALLTYPE PSGetShader(
            ID3D11PixelShader**               ppPixelShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void STDMETHODCALLTYPE PSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;
    
    void STDMETHODCALLTYPE PSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants) final;

    void STDMETHODCALLTYPE PSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE PSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void STDMETHODCALLTYPE CSSetShader(
            ID3D11ComputeShader*              pComputeShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void STDMETHODCALLTYPE CSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;
    
    void STDMETHODCALLTYPE CSSetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants) final;
    
    void STDMETHODCALLTYPE CSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE CSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void STDMETHODCALLTYPE CSSetUnorderedAccessViews(
            UINT                              StartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
      const UINT*                             pUAVInitialCounts) final;
    
    void STDMETHODCALLTYPE CSGetShader(
            ID3D11ComputeShader**             ppComputeShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void STDMETHODCALLTYPE CSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;
    
    void STDMETHODCALLTYPE CSGetConstantBuffers1(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers,
            UINT*                             pFirstConstant,
            UINT*                             pNumConstants) final;
    
    void STDMETHODCALLTYPE CSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void STDMETHODCALLTYPE CSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void STDMETHODCALLTYPE CSGetUnorderedAccessViews(
            UINT                              StartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView**       ppUnorderedAccessViews) final;
    
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
            UINT                              SampleMask) final;
    
    void STDMETHODCALLTYPE OMSetDepthStencilState(
            ID3D11DepthStencilState*          pDepthStencilState,
            UINT                              StencilRef) final;
    
    void STDMETHODCALLTYPE OMGetRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView**          ppRenderTargetViews,
            ID3D11DepthStencilView**          ppDepthStencilView) final;
    
    void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(
            UINT                              NumRTVs,
            ID3D11RenderTargetView**          ppRenderTargetViews,
            ID3D11DepthStencilView**          ppDepthStencilView,
            UINT                              UAVStartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView**       ppUnorderedAccessViews) final;
    
    void STDMETHODCALLTYPE OMGetBlendState(
            ID3D11BlendState**                ppBlendState,
            FLOAT                             BlendFactor[4],
            UINT*                             pSampleMask) final;
    
    void STDMETHODCALLTYPE OMGetDepthStencilState(
            ID3D11DepthStencilState**         ppDepthStencilState,
            UINT*                             pStencilRef) final;
    
    void STDMETHODCALLTYPE RSSetState(
            ID3D11RasterizerState*            pRasterizerState) final;
    
    void STDMETHODCALLTYPE RSSetViewports(
            UINT                              NumViewports,
      const D3D11_VIEWPORT*                   pViewports) final;
    
    void STDMETHODCALLTYPE RSSetScissorRects(
            UINT                              NumRects,
      const D3D11_RECT*                       pRects) final;
    
    void STDMETHODCALLTYPE RSGetState(
            ID3D11RasterizerState**           ppRasterizerState) final;
    
    void STDMETHODCALLTYPE RSGetViewports(
            UINT*                             pNumViewports,
            D3D11_VIEWPORT*                   pViewports) final;
    
    void STDMETHODCALLTYPE RSGetScissorRects(
            UINT*                             pNumRects,
            D3D11_RECT*                       pRects) final;
    
    void STDMETHODCALLTYPE SOSetTargets(
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppSOTargets,
      const UINT*                             pOffsets) final;
    
    void STDMETHODCALLTYPE SOGetTargets(
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppSOTargets) final;
    
    void STDMETHODCALLTYPE TransitionSurfaceLayout(
            IDXGIVkInteropSurface*    pSurface,
      const VkImageSubresourceRange*  pSubresources,
            VkImageLayout             OldLayout,
            VkImageLayout             NewLayout);
    
  protected:
    
    D3D11Device* const          m_parent;
    D3D11UserDefinedAnnotation  m_annotation;
    
    Rc<DxvkDevice>              m_device;
    Rc<DxvkCsChunk>             m_csChunk;
    Rc<DxvkDataBuffer>          m_updateBuffer;
    
    Com<D3D11BlendState>        m_defaultBlendState;
    Com<D3D11DepthStencilState> m_defaultDepthStencilState;
    Com<D3D11RasterizerState>   m_defaultRasterizerState;
    
    D3D11ContextState           m_state;
    
    void ApplyInputLayout();
    
    void ApplyPrimitiveTopology();
    
    void ApplyBlendState();
    
    void ApplyBlendFactor();
    
    void ApplyDepthStencilState();
    
    void ApplyStencilRef();
    
    void ApplyRasterizerState();
    
    void ApplyViewportState();
    
    void BindFramebuffer(
            BOOL                              Spill);
    
    template<typename T>
    void BindShader(
            T*                                pShader,
            VkShaderStageFlagBits             Stage) {
      EmitCs([
        cShader = pShader != nullptr ? pShader->GetShader() : nullptr,
        cStage  = Stage
      ] (DxvkContext* ctx) {
        ctx->bindShader(cStage, cShader);
      });
    }
    
    void BindVertexBuffer(
            UINT                              Slot,
            D3D11Buffer*                      pBuffer,
            UINT                              Offset,
            UINT                              Stride);
    
    void BindIndexBuffer(
            D3D11Buffer*                      pBuffer,
            UINT                              Offset,
            DXGI_FORMAT                       Format);
    
    void BindConstantBuffer(
            UINT                              Slot,
      const D3D11ConstantBufferBinding*       pBufferBinding);
    
    void BindSampler(
            UINT                              Slot,
            D3D11SamplerState*                pSampler);
    
    void BindShaderResource(
            UINT                              Slot,
            D3D11ShaderResourceView*          pResource);
    
    void BindUnorderedAccessView(
            UINT                              UavSlot,
            UINT                              CtrSlot,
            D3D11UnorderedAccessView*         pUav);
    
    void SetConstantBuffers(
            DxbcProgramType                   ShaderStage,
            D3D11ConstantBufferBindings&      Bindings,
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers,
      const UINT*                             pFirstConstant,
      const UINT*                             pNumConstants);
    
    void SetSamplers(
            DxbcProgramType                   ShaderStage,
            D3D11SamplerBindings&             Bindings,
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers);
    
    void SetShaderResources(
            DxbcProgramType                   ShaderStage,
            D3D11ShaderResourceBindings&      Bindings,
            UINT                              StartSlot,
            UINT                              NumResources,
            ID3D11ShaderResourceView* const*  ppResources);
    
    void SetUnorderedAccessViews(
            DxbcProgramType                   ShaderStage,
            D3D11UnorderedAccessBindings&     Bindings,
            UINT                              StartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews);
    
    void SetRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView);
    
    void InitUnorderedAccessViewCounters(
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
      const UINT*                             pUAVInitialCounts);
    
    void GetConstantBuffers(
      const D3D11ConstantBufferBindings&      Bindings,
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers, 
            UINT*                             pFirstConstant, 
            UINT*                             pNumConstants);
    
    void RestoreState();
    
    void RestoreConstantBuffers(
            DxbcProgramType                   Stage,
            D3D11ConstantBufferBindings&      Bindings);
    
    void RestoreSamplers(
            DxbcProgramType                   Stage,
            D3D11SamplerBindings&             Bindings);
    
    void RestoreShaderResources(
            DxbcProgramType                   Stage,
            D3D11ShaderResourceBindings&      Bindings);
    
    void RestoreUnorderedAccessViews(
            DxbcProgramType                   Stage,
            D3D11UnorderedAccessBindings&     Bindings);
    
    bool ValidateRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView);
    
    DxvkDataSlice AllocUpdateBufferSlice(size_t Size);
    
    template<typename Cmd>
    void EmitCs(Cmd&& command) {
      if (!m_csChunk->push(command)) {
        EmitCsChunk(std::move(m_csChunk));
        
        m_csChunk = new DxvkCsChunk();
        m_csChunk->push(command);
      }
    }
    
    void FlushCsChunk() {
      if (m_csChunk->commandCount() != 0) {
        EmitCsChunk(std::move(m_csChunk));
        m_csChunk = new DxvkCsChunk();
      }
    }
    
    virtual void EmitCsChunk(Rc<DxvkCsChunk>&& chunk) = 0;
    
  };
  
}
