#pragma once

#include "d3d11_context_state.h"
#include "d3d11_device_child.h"

#include <dxvk_adapter.h>
#include <dxvk_device.h>

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11DeviceContext : public D3D11DeviceChild<ID3D11DeviceContext> {
    
  public:
    
    D3D11DeviceContext(
      ID3D11Device*   parent,
      Rc<DxvkDevice>  device);
    ~D3D11DeviceContext();
    
    HRESULT QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    void GetDevice(ID3D11Device **ppDevice) final;
    
    D3D11_DEVICE_CONTEXT_TYPE GetType() final;
    
    UINT GetContextFlags() final;
    
    void ClearState() final;
    
    void Flush() final;
    
    void ExecuteCommandList(
            ID3D11CommandList*  pCommandList,
            WINBOOL             RestoreContextState) final;
    
    HRESULT FinishCommandList(
            WINBOOL             RestoreDeferredContextState,
            ID3D11CommandList   **ppCommandList) final;
    
    HRESULT Map(
            ID3D11Resource*             pResource,
            UINT                        Subresource,
            D3D11_MAP                   MapType,
            UINT                        MapFlags,
            D3D11_MAPPED_SUBRESOURCE*   pMappedResource) final;
    
    void Unmap(
            ID3D11Resource*             pResource,
            UINT                        Subresource) final;
    
    void Begin(ID3D11Asynchronous *pAsync) final;
    
    void End(ID3D11Asynchronous *pAsync) final;
    
    HRESULT GetData(
            ID3D11Asynchronous*               pAsync,
            void*                             pData,
            UINT                              DataSize,
            UINT                              GetDataFlags) final;
    
    void SetPredication(
            ID3D11Predicate*                  pPredicate,
            WINBOOL                           PredicateValue) final;
    
    void GetPredication(
            ID3D11Predicate**                 ppPredicate,
            WINBOOL*                          pPredicateValue) final;
    
    void CopySubresourceRegion(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
            UINT                              DstX,
            UINT                              DstY,
            UINT                              DstZ,
            ID3D11Resource*                   pSrcResource,
            UINT                              SrcSubresource,
      const D3D11_BOX*                        pSrcBox) final;
    
    void CopyResource(
            ID3D11Resource*                   pDstResource,
            ID3D11Resource*                   pSrcResource) final;
    
    void CopyStructureCount(
            ID3D11Buffer*                     pDstBuffer,
            UINT                              DstAlignedByteOffset,
            ID3D11UnorderedAccessView*        pSrcView) final;
    
    void ClearRenderTargetView(
            ID3D11RenderTargetView*           pRenderTargetView,
      const FLOAT                             ColorRGBA[4]) final;
    
    void ClearUnorderedAccessViewUint(
            ID3D11UnorderedAccessView*        pUnorderedAccessView,
      const UINT                              Values[4]) final;
    
    void ClearUnorderedAccessViewFloat(
            ID3D11UnorderedAccessView*        pUnorderedAccessView,
      const FLOAT                             Values[4]) final;
    
    void ClearDepthStencilView(
            ID3D11DepthStencilView*           pDepthStencilView,
            UINT                              ClearFlags,
            FLOAT                             Depth,
            UINT8                             Stencil) final;
    
    void GenerateMips(
            ID3D11ShaderResourceView*         pShaderResourceView) final;
    
    void UpdateSubresource(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
      const D3D11_BOX*                        pDstBox,
      const void*                             pSrcData,
            UINT                              SrcRowPitch,
            UINT                              SrcDepthPitch) final;
    
    void SetResourceMinLOD(
            ID3D11Resource*                   pResource,
            FLOAT                             MinLOD) final;
    
    FLOAT GetResourceMinLOD(
            ID3D11Resource*                   pResource) final;
    
    void ResolveSubresource(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
            ID3D11Resource*                   pSrcResource,
            UINT                              SrcSubresource,
            DXGI_FORMAT                       Format) final;
    
    void DrawAuto() final;
    
    void Draw(
            UINT            VertexCount,
            UINT            StartVertexLocation) final;
    
    void DrawIndexed(
            UINT            IndexCount,
            UINT            StartIndexLocation,
            INT             BaseVertexLocation) final;
    
    void DrawInstanced(
            UINT            VertexCountPerInstance,
            UINT            InstanceCount,
            UINT            StartVertexLocation,
            UINT            StartInstanceLocation) final;
    
    void DrawIndexedInstanced(
            UINT            IndexCountPerInstance,
            UINT            InstanceCount,
            UINT            StartIndexLocation,
            INT             BaseVertexLocation,
            UINT            StartInstanceLocation) final;
    
    void DrawIndexedInstancedIndirect(
            ID3D11Buffer*   pBufferForArgs,
            UINT            AlignedByteOffsetForArgs) final;
    
    void DrawInstancedIndirect(
            ID3D11Buffer*   pBufferForArgs,
            UINT            AlignedByteOffsetForArgs) final;
    
    void Dispatch(
            UINT            ThreadGroupCountX,
            UINT            ThreadGroupCountY,
            UINT            ThreadGroupCountZ) final;
    
    void DispatchIndirect(
            ID3D11Buffer*   pBufferForArgs,
            UINT            AlignedByteOffsetForArgs) final;
    
    void IASetInputLayout(
            ID3D11InputLayout*                pInputLayout) final;
    
    void IASetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY          Topology) final;
    
    void IASetVertexBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppVertexBuffers,
      const UINT*                             pStrides,
      const UINT*                             pOffsets) final;
    
    void IASetIndexBuffer(
            ID3D11Buffer*                     pIndexBuffer,
            DXGI_FORMAT                       Format,
            UINT                              Offset) final;
    
    void IAGetInputLayout(
            ID3D11InputLayout**               ppInputLayout) final;
    
    void IAGetPrimitiveTopology(
            D3D11_PRIMITIVE_TOPOLOGY*         pTopology) final;
    
    void IAGetVertexBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppVertexBuffers,
            UINT*                             pStrides,
            UINT*                             pOffsets) final;
    
    void IAGetIndexBuffer(
            ID3D11Buffer**                    pIndexBuffer,
            DXGI_FORMAT*                      Format,
            UINT*                             Offset) final;
    
    void VSSetShader(
            ID3D11VertexShader*               pVertexShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void VSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;
    
    void VSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void VSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void VSGetShader(
            ID3D11VertexShader**              ppVertexShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void VSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;
    
    void VSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void VSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void HSSetShader(
            ID3D11HullShader*                 pHullShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void HSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void HSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;
    
    void HSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void HSGetShader(
            ID3D11HullShader**                ppHullShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void HSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;
    
    void HSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void HSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void DSSetShader(
            ID3D11DomainShader*               pDomainShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void DSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void DSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;
    
    void DSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void DSGetShader(
            ID3D11DomainShader**              ppDomainShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void DSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;
    
    void DSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void DSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void GSSetShader(
            ID3D11GeometryShader*             pShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void GSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;
    
    void GSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void GSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void GSGetShader(
            ID3D11GeometryShader**            ppGeometryShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void GSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;
    
    void GSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void GSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void PSSetShader(
            ID3D11PixelShader*                pPixelShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void PSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;
    
    void PSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void PSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void PSGetShader(
            ID3D11PixelShader**               ppPixelShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void PSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;
    
    void PSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void PSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void CSSetShader(
            ID3D11ComputeShader*              pComputeShader,
            ID3D11ClassInstance* const*       ppClassInstances,
            UINT                              NumClassInstances) final;
    
    void CSSetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppConstantBuffers) final;
    
    void CSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews) final;
    
    void CSSetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers) final;
    
    void CSSetUnorderedAccessViews(
            UINT                              StartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
      const UINT*                             pUAVInitialCounts) final;
    
    void CSGetShader(
            ID3D11ComputeShader**             ppComputeShader,
            ID3D11ClassInstance**             ppClassInstances,
            UINT*                             pNumClassInstances) final;
    
    void CSGetConstantBuffers(
            UINT                              StartSlot,
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppConstantBuffers) final;
    
    void CSGetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView**        ppShaderResourceViews) final;
    
    void CSGetSamplers(
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState**              ppSamplers) final;
    
    void CSGetUnorderedAccessViews(
            UINT                              StartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView**       ppUnorderedAccessViews) final;
    
    void OMSetRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView) final;
    
    void OMSetRenderTargetsAndUnorderedAccessViews(
            UINT                              NumRTVs,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView,
            UINT                              UAVStartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
      const UINT*                             pUAVInitialCounts) final;
    
    void OMSetBlendState(
            ID3D11BlendState*                 pBlendState,
      const FLOAT                             BlendFactor[4],
            UINT                              SampleMask) final;
    
    void OMSetDepthStencilState(
            ID3D11DepthStencilState*          pDepthStencilState,
            UINT                              StencilRef) final;
    
    void OMGetRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView**          ppRenderTargetViews,
            ID3D11DepthStencilView**          ppDepthStencilView) final;
    
    void OMGetRenderTargetsAndUnorderedAccessViews(
            UINT                              NumRTVs,
            ID3D11RenderTargetView**          ppRenderTargetViews,
            ID3D11DepthStencilView**          ppDepthStencilView,
            UINT                              UAVStartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView**       ppUnorderedAccessViews) final;
    
    void OMGetBlendState(
            ID3D11BlendState**                ppBlendState,
            FLOAT                             BlendFactor[4],
            UINT*                             pSampleMask) final;
    
    void OMGetDepthStencilState(
            ID3D11DepthStencilState**         ppDepthStencilState,
            UINT*                             pStencilRef) final;
    
    void RSSetState(
            ID3D11RasterizerState*            pRasterizerState) final;
    
    void RSSetViewports(
            UINT                              NumViewports,
      const D3D11_VIEWPORT*                   pViewports) final;
    
    void RSSetScissorRects(
            UINT                              NumRects,
      const D3D11_RECT*                       pRects) final;
    
    void RSGetState(
            ID3D11RasterizerState**           ppRasterizerState) final;
    
    void RSGetViewports(
            UINT*                             pNumViewports,
            D3D11_VIEWPORT*                   pViewports) final;
    
    void RSGetScissorRects(
            UINT*                             pNumRects,
            D3D11_RECT*                       pRects) final;
    
    void SOSetTargets(
            UINT                              NumBuffers,
            ID3D11Buffer* const*              ppSOTargets,
      const UINT*                             pOffsets) final;
    
    void SOGetTargets(
            UINT                              NumBuffers,
            ID3D11Buffer**                    ppSOTargets) final;
    
  private:
    
    ID3D11Device* const m_parent;
    
    const D3D11_DEVICE_CONTEXT_TYPE m_type  = D3D11_DEVICE_CONTEXT_IMMEDIATE;
    const UINT                      m_flags = 0;
    
    Rc<DxvkDevice>      m_device;
    Rc<DxvkContext>     m_context;
    Rc<DxvkCommandList> m_cmdList;
    
    D3D11ContextState   m_state;
    
  };
  
}
