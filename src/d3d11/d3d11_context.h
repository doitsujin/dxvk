#pragma once

#include "../dxvk/dxvk_adapter.h"
#include "../dxvk/dxvk_cs.h"
#include "../dxvk/dxvk_device.h"

#include "../d3d10/d3d10_multithread.h"

#include "d3d11_annotation.h"
#include "d3d11_cmd.h"
#include "d3d11_context_ext.h"
#include "d3d11_context_state.h"
#include "d3d11_device_child.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  class D3D11Device;
  
  class D3D11DeviceContext : public D3D11DeviceChild<ID3D11DeviceContext4> {
    friend class D3D11DeviceContextExt;
    // Needed in order to call EmitCs for pushing markers
    friend class D3D11UserDefinedAnnotation;
  public:
    
    D3D11DeviceContext(
            D3D11Device*            pParent,
      const Rc<DxvkDevice>&         Device,
            DxvkCsChunkFlags        CsFlags);
    ~D3D11DeviceContext();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject);
    
    void STDMETHODCALLTYPE DiscardResource(ID3D11Resource *pResource);

    void STDMETHODCALLTYPE DiscardView(ID3D11View* pResourceView);

    void STDMETHODCALLTYPE DiscardView1(
            ID3D11View*                      pResourceView,
      const D3D11_RECT*                      pRects,
            UINT                             NumRects);
    
    void STDMETHODCALLTYPE ClearState();
    
    void STDMETHODCALLTYPE SetPredication(
            ID3D11Predicate*                  pPredicate,
            BOOL                              PredicateValue);
    
    void STDMETHODCALLTYPE GetPredication(
            ID3D11Predicate**                 ppPredicate,
            BOOL*                             pPredicateValue);
    
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
            ID3D11UnorderedAccessView*        pSrcView);

    void STDMETHODCALLTYPE CopyTiles(
            ID3D11Resource*                   pTiledResource,
      const D3D11_TILED_RESOURCE_COORDINATE*  pTileRegionStartCoordinate,
      const D3D11_TILE_REGION_SIZE*           pTileRegionSize,
            ID3D11Buffer*                     pBuffer,
            UINT64                            BufferStartOffsetInBytes,
            UINT                              Flags);
    
    HRESULT STDMETHODCALLTYPE CopyTileMappings(
            ID3D11Resource*                   pDestTiledResource,
      const D3D11_TILED_RESOURCE_COORDINATE*  pDestRegionStartCoordinate,
            ID3D11Resource*                   pSourceTiledResource,
      const D3D11_TILED_RESOURCE_COORDINATE*  pSourceRegionStartCoordinate,
      const D3D11_TILE_REGION_SIZE*           pTileRegionSize,
            UINT                              Flags);

    HRESULT STDMETHODCALLTYPE ResizeTilePool(
            ID3D11Buffer*                     pTilePool,
            UINT64                            NewSizeInBytes);

    void STDMETHODCALLTYPE TiledResourceBarrier(
            ID3D11DeviceChild*                pTiledResourceOrViewAccessBeforeBarrier,
            ID3D11DeviceChild*                pTiledResourceOrViewAccessAfterBarrier);

    void STDMETHODCALLTYPE ClearRenderTargetView(
            ID3D11RenderTargetView*           pRenderTargetView,
      const FLOAT                             ColorRGBA[4]);
    
    void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
            ID3D11UnorderedAccessView*        pUnorderedAccessView,
      const UINT                              Values[4]);
    
    void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
            ID3D11UnorderedAccessView*        pUnorderedAccessView,
      const FLOAT                             Values[4]);
    
    void STDMETHODCALLTYPE ClearDepthStencilView(
            ID3D11DepthStencilView*           pDepthStencilView,
            UINT                              ClearFlags,
            FLOAT                             Depth,
            UINT8                             Stencil);
    
    void STDMETHODCALLTYPE ClearView(
            ID3D11View                        *pView,
      const FLOAT                             Color[4],
      const D3D11_RECT                        *pRect,
            UINT                              NumRects);

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

    HRESULT STDMETHODCALLTYPE UpdateTileMappings(
            ID3D11Resource*                   pTiledResource,
            UINT                              NumTiledResourceRegions,
      const D3D11_TILED_RESOURCE_COORDINATE*  pTiledResourceRegionStartCoordinates,
      const D3D11_TILE_REGION_SIZE*           pTiledResourceRegionSizes,
            ID3D11Buffer*                     pTilePool,
            UINT                              NumRanges,
      const UINT*                             pRangeFlags,
      const UINT*                             pTilePoolStartOffsets,
      const UINT*                             pRangeTileCounts,
            UINT                              Flags);

    void STDMETHODCALLTYPE UpdateTiles(
            ID3D11Resource*                   pDestTiledResource,
      const D3D11_TILED_RESOURCE_COORDINATE*  pDestTileRegionStartCoordinate,
      const D3D11_TILE_REGION_SIZE*           pDestTileRegionSize,
      const void*                             pSourceTileData,
            UINT                              Flags);

    void STDMETHODCALLTYPE SetResourceMinLOD(
            ID3D11Resource*                   pResource,
            FLOAT                             MinLOD);
    
    FLOAT STDMETHODCALLTYPE GetResourceMinLOD(
            ID3D11Resource*                   pResource);
    
    void STDMETHODCALLTYPE ResolveSubresource(
            ID3D11Resource*                   pDstResource,
            UINT                              DstSubresource,
            ID3D11Resource*                   pSrcResource,
            UINT                              SrcSubresource,
            DXGI_FORMAT                       Format);
    
    void STDMETHODCALLTYPE DrawAuto();
    
    void STDMETHODCALLTYPE Draw(
            UINT            VertexCount,
            UINT            StartVertexLocation);
    
    void STDMETHODCALLTYPE DrawIndexed(
            UINT            IndexCount,
            UINT            StartIndexLocation,
            INT             BaseVertexLocation);
    
    void STDMETHODCALLTYPE DrawInstanced(
            UINT            VertexCountPerInstance,
            UINT            InstanceCount,
            UINT            StartVertexLocation,
            UINT            StartInstanceLocation);
    
    void STDMETHODCALLTYPE DrawIndexedInstanced(
            UINT            IndexCountPerInstance,
            UINT            InstanceCount,
            UINT            StartIndexLocation,
            INT             BaseVertexLocation,
            UINT            StartInstanceLocation);
    
    void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(
            ID3D11Buffer*   pBufferForArgs,
            UINT            AlignedByteOffsetForArgs);
    
    void STDMETHODCALLTYPE DrawInstancedIndirect(
            ID3D11Buffer*   pBufferForArgs,
            UINT            AlignedByteOffsetForArgs);
    
    void STDMETHODCALLTYPE Dispatch(
            UINT            ThreadGroupCountX,
            UINT            ThreadGroupCountY,
            UINT            ThreadGroupCountZ);
    
    void STDMETHODCALLTYPE DispatchIndirect(
            ID3D11Buffer*   pBufferForArgs,
            UINT            AlignedByteOffsetForArgs);
    
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
    
    void STDMETHODCALLTYPE HSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews);
    
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
    
    void STDMETHODCALLTYPE DSSetShaderResources(
            UINT                              StartSlot,
            UINT                              NumViews,
            ID3D11ShaderResourceView* const*  ppShaderResourceViews);
    
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
    
    BOOL STDMETHODCALLTYPE IsAnnotationEnabled();

    void STDMETHODCALLTYPE SetMarkerInt(
            LPCWSTR                           pLabel,
            INT                               Data);

    void STDMETHODCALLTYPE BeginEventInt(
            LPCWSTR                           pLabel,
            INT                               Data);

    void STDMETHODCALLTYPE EndEvent();

    void STDMETHODCALLTYPE GetHardwareProtectionState(
            BOOL*                             pHwProtectionEnable);
    
    void STDMETHODCALLTYPE SetHardwareProtectionState(
            BOOL                              HwProtectionEnable);

    void STDMETHODCALLTYPE TransitionSurfaceLayout(
            IDXGIVkInteropSurface*    pSurface,
      const VkImageSubresourceRange*  pSubresources,
            VkImageLayout             OldLayout,
            VkImageLayout             NewLayout);

    D3D10DeviceLock LockContext() {
      return m_multithread.AcquireLock();
    }

  protected:
    
    D3D11DeviceContextExt       m_contextExt;
    D3D11UserDefinedAnnotation  m_annotation;
    D3D10Multithread            m_multithread;
    
    Rc<DxvkDevice>              m_device;
    Rc<DxvkDataBuffer>          m_updateBuffer;
    
    DxvkCsChunkFlags            m_csFlags;
    DxvkCsChunkRef              m_csChunk;
    
    D3D11ContextState           m_state;
    D3D11CmdData*               m_cmdData;
    
    void ApplyInputLayout();
    
    void ApplyPrimitiveTopology();
    
    void ApplyBlendState();
    
    void ApplyBlendFactor();
    
    void ApplyDepthStencilState();
    
    void ApplyStencilRef();
    
    void ApplyRasterizerState();
    
    void ApplyViewportState();

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
    
    void BindConstantBuffer(
            UINT                              Slot,
            D3D11Buffer*                      pBuffer,
            UINT                              Offset,
            UINT                              Length);
    
    void BindSampler(
            UINT                              Slot,
            D3D11SamplerState*                pSampler);
    
    void BindShaderResource(
            UINT                              Slot,
            D3D11ShaderResourceView*          pResource);
    
    void BindUnorderedAccessView(
            UINT                              UavSlot,
            D3D11UnorderedAccessView*         pUav,
            UINT                              CtrSlot,
            UINT                              Counter);

    void CopyBuffer(
            D3D11Buffer*                      pDstBuffer,
            VkDeviceSize                      DstOffset,
            D3D11Buffer*                      pSrcBuffer,
            VkDeviceSize                      SrcOffset,
            VkDeviceSize                      ByteCount);

    void CopyImage(
            D3D11CommonTexture*               pDstTexture,
      const VkImageSubresourceLayers*         pDstLayers,
            VkOffset3D                        DstOffset,
            D3D11CommonTexture*               pSrcTexture,
      const VkImageSubresourceLayers*         pSrcLayers,
            VkOffset3D                        SrcOffset,
            VkExtent3D                        SrcExtent);

    void DiscardBuffer(
            ID3D11Resource*                   pResource);
    
    void DiscardTexture(
            ID3D11Resource*                   pResource,
            UINT                              Subresource);

    void UpdateImage(
            D3D11CommonTexture*               pDstTexture,
      const VkImageSubresource*               pDstSubresource,
            VkOffset3D                        DstOffset,
            VkExtent3D                        DstExtent,
            DxvkBufferSlice                   StagingBuffer);

    void SetDrawBuffers(
            ID3D11Buffer*                     pBufferForArgs,
            ID3D11Buffer*                     pBufferForCount);
    
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
    void SetSamplers(
            D3D11SamplerBindings&             Bindings,
            UINT                              StartSlot,
            UINT                              NumSamplers,
            ID3D11SamplerState* const*        ppSamplers);
    
    template<DxbcProgramType ShaderStage>
    void SetShaderResources(
            D3D11ShaderResourceBindings&      Bindings,
            UINT                              StartSlot,
            UINT                              NumResources,
            ID3D11ShaderResourceView* const*  ppResources);
    
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
    
    bool TestRtvUavHazards(
            UINT                              NumRTVs,
            ID3D11RenderTargetView* const*    ppRTVs,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUAVs);
    
    template<DxbcProgramType ShaderStage>
    bool TestSrvHazards(
            D3D11ShaderResourceView*          pView);

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
    
    bool ValidateRenderTargets(
            UINT                              NumViews,
            ID3D11RenderTargetView* const*    ppRenderTargetViews,
            ID3D11DepthStencilView*           pDepthStencilView);
    
    VkClearValue ConvertColorValue(
      const FLOAT                             Color[4],
      const DxvkFormatInfo*                   pFormatInfo);
    
    DxvkDataSlice AllocUpdateBufferSlice(size_t Size);
    
    DxvkBufferSlice AllocStagingBuffer(
            VkDeviceSize                      Size);
    
    DxvkCsChunkRef AllocCsChunk();
    
    static void InitDefaultPrimitiveTopology(
            DxvkInputAssemblyState*           pIaState);

    static void InitDefaultRasterizerState(
            DxvkRasterizerState*              pRsState);

    static void InitDefaultDepthStencilState(
            DxvkDepthStencilState*            pDsState);

    static void InitDefaultBlendState(
            DxvkBlendMode*                    pCbState,
            DxvkLogicOpState*                 pLoState,
            DxvkMultisampleState*             pMsState,
            UINT                              SampleMask);

    template<typename T>
    const D3D11CommonShader* GetCommonShader(T* pShader) const {
      return pShader != nullptr ? pShader->GetCommonShader() : nullptr;
    }

    static uint32_t GetIndirectCommandStride(const D3D11CmdDrawIndirectData* cmdData, uint32_t offset, uint32_t minStride) {
      if (likely(cmdData->stride))
        return cmdData->offset + cmdData->count * cmdData->stride == offset ? cmdData->stride : 0;

      uint32_t stride = offset - cmdData->offset;
      return stride >= minStride && stride <= 32 ? stride : 0;
    }

    static bool ValidateDrawBufferSize(ID3D11Buffer* pBuffer, UINT Offset, UINT Size) {
      UINT bufferSize = 0;

      if (likely(pBuffer != nullptr))
        bufferSize = static_cast<D3D11Buffer*>(pBuffer)->Desc()->ByteWidth;

      return bufferSize >= Offset + Size;
    }
    
    template<typename Cmd>
    void EmitCs(Cmd&& command) {
      m_cmdData = nullptr;

      if (unlikely(!m_csChunk->push(command))) {
        EmitCsChunk(std::move(m_csChunk));
        
        m_csChunk = AllocCsChunk();
        m_csChunk->push(command);
      }
    }

    template<typename M, typename Cmd, typename... Args>
    M* EmitCsCmd(Cmd&& command, Args&&... args) {
      M* data = m_csChunk->pushCmd<M, Cmd, Args...>(
        command, std::forward<Args>(args)...);

      if (unlikely(!data)) {
        EmitCsChunk(std::move(m_csChunk));
        
        m_csChunk = AllocCsChunk();
        data = m_csChunk->pushCmd<M, Cmd, Args...>(
          command, std::forward<Args>(args)...);
      }

      m_cmdData = data;
      return data;
    }
    
    void FlushCsChunk() {
      if (likely(!m_csChunk->empty())) {
        EmitCsChunk(std::move(m_csChunk));
        m_csChunk = AllocCsChunk();
        m_cmdData = nullptr;
      }
    }
    
    virtual void EmitCsChunk(DxvkCsChunkRef&& chunk) = 0;
    
  };
  
}
