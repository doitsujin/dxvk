#pragma once

#include "../dxvk/dxvk_adapter.h"
#include "../dxvk/dxvk_cs.h"
#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_staging.h"

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
    template<typename T>
    friend class D3D11DeviceContextExt;
    // Needed in order to call EmitCs for pushing markers
    template<typename T>
    friend class D3D11UserDefinedAnnotation;

    constexpr static VkDeviceSize StagingBufferSize = 4ull << 20;
  public:
    
    D3D11DeviceContext(
            D3D11Device*            pParent,
      const Rc<DxvkDevice>&         Device,
            DxvkCsChunkFlags        CsFlags);
    ~D3D11DeviceContext();
    
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
    
    void STDMETHODCALLTYPE CSSetUnorderedAccessViews(
            UINT                              StartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
      const UINT*                             pUAVInitialCounts);
    
    void STDMETHODCALLTYPE CSGetUnorderedAccessViews(
            UINT                              StartSlot,
            UINT                              NumUAVs,
            ID3D11UnorderedAccessView**       ppUnorderedAccessViews);
    
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
    
    D3D10Multithread            m_multithread;
    
    Rc<DxvkDevice>              m_device;
    Rc<DxvkDataBuffer>          m_updateBuffer;

    DxvkStagingBuffer           m_staging;

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
    
    void ApplyRasterizerSampleCount();

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

    void UpdateBuffer(
            D3D11Buffer*                      pDstBuffer,
            UINT                              Offset,
            UINT                              Length,
      const void*                             pSrcData);

    void UpdateTexture(
            D3D11CommonTexture*               pDstTexture,
            UINT                              DstSubresource,
      const D3D11_BOX*                        pDstBox,
      const void*                             pSrcData,
            UINT                              SrcRowPitch,
            UINT                              SrcDepthPitch);

    void UpdateImage(
            D3D11CommonTexture*               pDstTexture,
      const VkImageSubresource*               pDstSubresource,
            VkOffset3D                        DstOffset,
            VkExtent3D                        DstExtent,
            DxvkBufferSlice                   StagingBuffer);

    void SetDrawBuffers(
            ID3D11Buffer*                     pBufferForArgs,
            ID3D11Buffer*                     pBufferForCount);
    
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

    VkClearValue ConvertColorValue(
      const FLOAT                             Color[4],
      const DxvkFormatInfo*                   pFormatInfo);
    
    DxvkDataSlice AllocUpdateBufferSlice(size_t Size);
    
    DxvkBufferSlice AllocStagingBuffer(
            VkDeviceSize                      Size);

    void ResetStagingBuffer();

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
    
    void TrackResourceSequenceNumber(
            ID3D11Resource*             pResource);

    virtual void EmitCsChunk(DxvkCsChunkRef&& chunk) = 0;
    
    virtual void TrackTextureSequenceNumber(
            D3D11CommonTexture*         pResource,
            UINT                        Subresource) = 0;

    virtual void TrackBufferSequenceNumber(
            D3D11Buffer*                pResource) = 0;

  };
  
}
