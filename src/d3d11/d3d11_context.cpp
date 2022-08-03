#include <cstring>

#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_query.h"
#include "d3d11_texture.h"
#include "d3d11_video.h"

#include "../dxbc/dxbc_util.h"

namespace dxvk {

  D3D11DeviceContext::D3D11DeviceContext(
          D3D11Device*            pParent,
    const Rc<DxvkDevice>&         Device,
          DxvkCsChunkFlags        CsFlags)
  : D3D11DeviceChild<ID3D11DeviceContext4>(pParent),
    m_multithread(this, false),
    m_device    (Device),
    m_staging   (Device, StagingBufferSize),
    m_csFlags   (CsFlags),
    m_csChunk   (AllocCsChunk()),
    m_cmdData   (nullptr) {

  }
  
  
  D3D11DeviceContext::~D3D11DeviceContext() {
    
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::CopyTiles(
          ID3D11Resource*                   pTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pTileRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE*           pTileRegionSize,
          ID3D11Buffer*                     pBuffer,
          UINT64                            BufferStartOffsetInBytes,
          UINT                              Flags) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::CopyTiles: Not implemented");
  }


  HRESULT STDMETHODCALLTYPE D3D11DeviceContext::CopyTileMappings(
          ID3D11Resource*                   pDestTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pDestRegionStartCoordinate,
          ID3D11Resource*                   pSourceTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pSourceRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE*           pTileRegionSize,
          UINT                              Flags) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::CopyTileMappings: Not implemented");

    return DXGI_ERROR_INVALID_CALL;
  }


  HRESULT STDMETHODCALLTYPE D3D11DeviceContext::ResizeTilePool(
          ID3D11Buffer*                     pTilePool,
          UINT64                            NewSizeInBytes) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::ResizeTilePool: Not implemented");

    return DXGI_ERROR_INVALID_CALL;
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::TiledResourceBarrier(
          ID3D11DeviceChild*                pTiledResourceOrViewAccessBeforeBarrier,
          ID3D11DeviceChild*                pTiledResourceOrViewAccessAfterBarrier) {
    
  }


  HRESULT STDMETHODCALLTYPE D3D11DeviceContext::UpdateTileMappings(
          ID3D11Resource*                   pTiledResource,
          UINT                              NumTiledResourceRegions,
    const D3D11_TILED_RESOURCE_COORDINATE*  pTiledResourceRegionStartCoordinates,
    const D3D11_TILE_REGION_SIZE*           pTiledResourceRegionSizes,
          ID3D11Buffer*                     pTilePool,
          UINT                              NumRanges,
    const UINT*                             pRangeFlags,
    const UINT*                             pTilePoolStartOffsets,
    const UINT*                             pRangeTileCounts,
          UINT                              Flags) {
    bool s_errorShown = false;

    if (std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::UpdateTileMappings: Not implemented");

    return DXGI_ERROR_INVALID_CALL;
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::UpdateTiles(
          ID3D11Resource*                   pDestTiledResource,
    const D3D11_TILED_RESOURCE_COORDINATE*  pDestTileRegionStartCoordinate,
    const D3D11_TILE_REGION_SIZE*           pDestTileRegionSize,
    const void*                             pSourceTileData,
          UINT                              Flags) {
    bool s_errorShown = false;

    if (std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::UpdateTiles: Not implemented");
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SetResourceMinLOD(
          ID3D11Resource*                   pResource,
          FLOAT                             MinLOD) {
    bool s_errorShown = false;

    if (std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::SetResourceMinLOD: Not implemented");
  }
  
  
  FLOAT STDMETHODCALLTYPE D3D11DeviceContext::GetResourceMinLOD(ID3D11Resource* pResource) {
    bool s_errorShown = false;

    if (std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::GetResourceMinLOD: Not implemented");

    return 0.0f;
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawAuto() {
    D3D10DeviceLock lock = LockContext();

    D3D11Buffer* buffer = m_state.ia.vertexBuffers[0].buffer.ptr();

    if (buffer == nullptr)
      return;
    
    DxvkBufferSlice vtxBuf = buffer->GetBufferSlice();
    DxvkBufferSlice ctrBuf = buffer->GetSOCounter();

    if (!ctrBuf.defined())
      return;

    EmitCs([=] (DxvkContext* ctx) {
      ctx->drawIndirectXfb(ctrBuf,
        vtxBuf.buffer()->getXfbVertexStride(),
        vtxBuf.offset());
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::Draw(
          UINT            VertexCount,
          UINT            StartVertexLocation) {
    D3D10DeviceLock lock = LockContext();

    EmitCs([=] (DxvkContext* ctx) {
      ctx->draw(
        VertexCount, 1,
        StartVertexLocation, 0);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexed(
          UINT            IndexCount,
          UINT            StartIndexLocation,
          INT             BaseVertexLocation) {
    D3D10DeviceLock lock = LockContext();
    
    EmitCs([=] (DxvkContext* ctx) {
      ctx->drawIndexed(
        IndexCount, 1,
        StartIndexLocation,
        BaseVertexLocation, 0);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawInstanced(
          UINT            VertexCountPerInstance,
          UINT            InstanceCount,
          UINT            StartVertexLocation,
          UINT            StartInstanceLocation) {
    D3D10DeviceLock lock = LockContext();
    
    EmitCs([=] (DxvkContext* ctx) {
      ctx->draw(
        VertexCountPerInstance,
        InstanceCount,
        StartVertexLocation,
        StartInstanceLocation);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexedInstanced(
          UINT            IndexCountPerInstance,
          UINT            InstanceCount,
          UINT            StartIndexLocation,
          INT             BaseVertexLocation,
          UINT            StartInstanceLocation) {
    D3D10DeviceLock lock = LockContext();
    
    EmitCs([=] (DxvkContext* ctx) {
      ctx->drawIndexed(
        IndexCountPerInstance,
        InstanceCount,
        StartIndexLocation,
        BaseVertexLocation,
        StartInstanceLocation);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawIndexedInstancedIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D10DeviceLock lock = LockContext();
    SetDrawBuffers(pBufferForArgs, nullptr);

    if (!ValidateDrawBufferSize(pBufferForArgs, AlignedByteOffsetForArgs, sizeof(VkDrawIndexedIndirectCommand)))
      return;
    
    // If possible, batch up multiple indirect draw calls of
    // the same type into one single multiDrawIndirect call
    auto cmdData = static_cast<D3D11CmdDrawIndirectData*>(m_cmdData);
    auto stride = 0u;
    
    if (cmdData && cmdData->type == D3D11CmdType::DrawIndirectIndexed)
      stride = GetIndirectCommandStride(cmdData, AlignedByteOffsetForArgs, sizeof(VkDrawIndexedIndirectCommand));
    
    if (stride) {
      cmdData->count += 1;
      cmdData->stride = stride;
    } else {
      cmdData = EmitCsCmd<D3D11CmdDrawIndirectData>(
        [] (DxvkContext* ctx, const D3D11CmdDrawIndirectData* data) {
          ctx->drawIndexedIndirect(data->offset, data->count, data->stride);
        });
      
      cmdData->type   = D3D11CmdType::DrawIndirectIndexed;
      cmdData->offset = AlignedByteOffsetForArgs;
      cmdData->count  = 1;
      cmdData->stride = 0;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DrawInstancedIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D10DeviceLock lock = LockContext();
    SetDrawBuffers(pBufferForArgs, nullptr);

    if (!ValidateDrawBufferSize(pBufferForArgs, AlignedByteOffsetForArgs, sizeof(VkDrawIndirectCommand)))
      return;

    // If possible, batch up multiple indirect draw calls of
    // the same type into one single multiDrawIndirect call
    auto cmdData = static_cast<D3D11CmdDrawIndirectData*>(m_cmdData);
    auto stride = 0u;
    
    if (cmdData && cmdData->type == D3D11CmdType::DrawIndirect)
      stride = GetIndirectCommandStride(cmdData, AlignedByteOffsetForArgs, sizeof(VkDrawIndirectCommand));
    
    if (stride) {
      cmdData->count += 1;
      cmdData->stride = stride;
    } else {
      cmdData = EmitCsCmd<D3D11CmdDrawIndirectData>(
        [] (DxvkContext* ctx, const D3D11CmdDrawIndirectData* data) {
          ctx->drawIndirect(data->offset, data->count, data->stride);
        });
      
      cmdData->type   = D3D11CmdType::DrawIndirect;
      cmdData->offset = AlignedByteOffsetForArgs;
      cmdData->count  = 1;
      cmdData->stride = 0;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::Dispatch(
          UINT            ThreadGroupCountX,
          UINT            ThreadGroupCountY,
          UINT            ThreadGroupCountZ) {
    D3D10DeviceLock lock = LockContext();
    
    EmitCs([=] (DxvkContext* ctx) {
      ctx->dispatch(
        ThreadGroupCountX,
        ThreadGroupCountY,
        ThreadGroupCountZ);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::DispatchIndirect(
          ID3D11Buffer*   pBufferForArgs,
          UINT            AlignedByteOffsetForArgs) {
    D3D10DeviceLock lock = LockContext();
    SetDrawBuffers(pBufferForArgs, nullptr);
    
    if (!ValidateDrawBufferSize(pBufferForArgs, AlignedByteOffsetForArgs, sizeof(VkDispatchIndirectCommand)))
      return;

    EmitCs([cOffset = AlignedByteOffsetForArgs]
    (DxvkContext* ctx) {
      ctx->dispatchIndirect(cOffset);
    });
  }
  
  
  void STDMETHODCALLTYPE D3D11DeviceContext::SetMarkerInt(
          LPCWSTR                           pLabel,
          INT                               Data) {
    // Not implemented in the backend, ignore
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::BeginEventInt(
          LPCWSTR                           pLabel,
          INT                               Data) {
    // Not implemented in the backend, ignore
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::EndEvent() {
    // Not implemented in the backend, ignore
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::GetHardwareProtectionState(
          BOOL*                             pHwProtectionEnable) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::GetHardwareProtectionState: Not implemented");
    
    if (pHwProtectionEnable)
      *pHwProtectionEnable = FALSE;
  }

  
  void STDMETHODCALLTYPE D3D11DeviceContext::SetHardwareProtectionState(
          BOOL                              HwProtectionEnable) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::err("D3D11DeviceContext::SetHardwareProtectionState: Not implemented");
  }


  void STDMETHODCALLTYPE D3D11DeviceContext::TransitionSurfaceLayout(
          IDXGIVkInteropSurface*    pSurface,
    const VkImageSubresourceRange*  pSubresources,
          VkImageLayout             OldLayout,
          VkImageLayout             NewLayout) {
    D3D10DeviceLock lock = LockContext();
    
    // Get the underlying D3D11 resource
    Com<ID3D11Resource> resource;
    
    pSurface->QueryInterface(__uuidof(ID3D11Resource),
      reinterpret_cast<void**>(&resource));
    
    // Get the texture from that resource
    D3D11CommonTexture* texture = GetCommonTexture(resource.ptr());
    
    EmitCs([
      cImage        = texture->GetImage(),
      cSubresources = *pSubresources,
      cOldLayout    = OldLayout,
      cNewLayout    = NewLayout
    ] (DxvkContext* ctx) {
      ctx->transformImage(
        cImage, cSubresources,
        cOldLayout, cNewLayout);
    });
  }
  
  
  void D3D11DeviceContext::BindDrawBuffers(
          D3D11Buffer*                     pBufferForArgs,
          D3D11Buffer*                     pBufferForCount) {
    EmitCs([
      cArgBuffer = pBufferForArgs  ? pBufferForArgs->GetBufferSlice()  : DxvkBufferSlice(),
      cCntBuffer = pBufferForCount ? pBufferForCount->GetBufferSlice() : DxvkBufferSlice()
    ] (DxvkContext* ctx) {
      ctx->bindDrawBuffers(cArgBuffer, cCntBuffer);
    });
  }


  void D3D11DeviceContext::SetDrawBuffers(
          ID3D11Buffer*                     pBufferForArgs,
          ID3D11Buffer*                     pBufferForCount) {
    auto argBuffer = static_cast<D3D11Buffer*>(pBufferForArgs);
    auto cntBuffer = static_cast<D3D11Buffer*>(pBufferForCount);

    if (m_state.id.argBuffer != argBuffer
     || m_state.id.cntBuffer != cntBuffer) {
      m_state.id.argBuffer = argBuffer;
      m_state.id.cntBuffer = cntBuffer;

      BindDrawBuffers(argBuffer, cntBuffer);
    }
  }


  VkClearValue D3D11DeviceContext::ConvertColorValue(
    const FLOAT                             Color[4],
    const DxvkFormatInfo*                   pFormatInfo) {
    VkClearValue result;

    if (pFormatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      for (uint32_t i = 0; i < 4; i++) {
        if (pFormatInfo->flags.test(DxvkFormatFlag::SampledUInt))
          result.color.uint32[i] = uint32_t(std::max(0.0f, Color[i]));
        else if (pFormatInfo->flags.test(DxvkFormatFlag::SampledSInt))
          result.color.int32[i] = int32_t(Color[i]);
        else
          result.color.float32[i] = Color[i];
      }
    } else {
      result.depthStencil.depth = Color[0];
      result.depthStencil.stencil = 0;
    }

    return result;
  }


  DxvkDataSlice D3D11DeviceContext::AllocUpdateBufferSlice(size_t Size) {
    constexpr size_t UpdateBufferSize = 1 * 1024 * 1024;
    
    if (Size >= UpdateBufferSize) {
      Rc<DxvkDataBuffer> buffer = new DxvkDataBuffer(Size);
      return buffer->alloc(Size);
    } else {
      if (m_updateBuffer == nullptr)
        m_updateBuffer = new DxvkDataBuffer(UpdateBufferSize);
      
      DxvkDataSlice slice = m_updateBuffer->alloc(Size);
      
      if (slice.ptr() == nullptr) {
        m_updateBuffer = new DxvkDataBuffer(UpdateBufferSize);
        slice = m_updateBuffer->alloc(Size);
      }
      
      return slice;
    }
  }
  
  
  DxvkBufferSlice D3D11DeviceContext::AllocStagingBuffer(
          VkDeviceSize                      Size) {
    return m_staging.alloc(256, Size);
  }


  void D3D11DeviceContext::ResetStagingBuffer() {
    m_staging.reset();
  }
  

  DxvkCsChunkRef D3D11DeviceContext::AllocCsChunk() {
    return m_parent->AllocCsChunk(m_csFlags);
  }
  

  void D3D11DeviceContext::InitDefaultPrimitiveTopology(
          DxvkInputAssemblyState*           pIaState) {
    pIaState->primitiveTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    pIaState->primitiveRestart  = VK_FALSE;
    pIaState->patchVertexCount  = 0;
  }


  void D3D11DeviceContext::InitDefaultRasterizerState(
          DxvkRasterizerState*              pRsState) {
    pRsState->polygonMode     = VK_POLYGON_MODE_FILL;
    pRsState->cullMode        = VK_CULL_MODE_BACK_BIT;
    pRsState->frontFace       = VK_FRONT_FACE_CLOCKWISE;
    pRsState->depthClipEnable = VK_TRUE;
    pRsState->depthBiasEnable = VK_FALSE;
    pRsState->conservativeMode = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
    pRsState->sampleCount     = 0;
  }


  void D3D11DeviceContext::InitDefaultDepthStencilState(
          DxvkDepthStencilState*            pDsState) {
    VkStencilOpState stencilOp;
    stencilOp.failOp            = VK_STENCIL_OP_KEEP;
    stencilOp.passOp            = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp       = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp         = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask       = D3D11_DEFAULT_STENCIL_READ_MASK;
    stencilOp.writeMask         = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    stencilOp.reference         = 0;

    pDsState->enableDepthTest   = VK_TRUE;
    pDsState->enableDepthWrite  = VK_TRUE;
    pDsState->enableStencilTest = VK_FALSE;
    pDsState->depthCompareOp    = VK_COMPARE_OP_LESS;
    pDsState->stencilOpFront    = stencilOp;
    pDsState->stencilOpBack     = stencilOp;
  }

  
  void D3D11DeviceContext::InitDefaultBlendState(
          DxvkBlendMode*                    pCbState,
          DxvkLogicOpState*                 pLoState,
          DxvkMultisampleState*             pMsState,
          UINT                              SampleMask) {
    pCbState->enableBlending    = VK_FALSE;
    pCbState->colorSrcFactor    = VK_BLEND_FACTOR_ONE;
    pCbState->colorDstFactor    = VK_BLEND_FACTOR_ZERO;
    pCbState->colorBlendOp      = VK_BLEND_OP_ADD;
    pCbState->alphaSrcFactor    = VK_BLEND_FACTOR_ONE;
    pCbState->alphaDstFactor    = VK_BLEND_FACTOR_ZERO;
    pCbState->alphaBlendOp      = VK_BLEND_OP_ADD;
    pCbState->writeMask         = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    pLoState->enableLogicOp     = VK_FALSE;
    pLoState->logicOp           = VK_LOGIC_OP_NO_OP;

    pMsState->sampleMask            = SampleMask;
    pMsState->enableAlphaToCoverage = VK_FALSE;
  }


  void D3D11DeviceContext::TrackResourceSequenceNumber(
          ID3D11Resource*             pResource) {
    if (!pResource)
      return;

    D3D11CommonTexture* texture = GetCommonTexture(pResource);

    if (texture) {
      if (texture->HasSequenceNumber()) {
        for (uint32_t i = 0; i < texture->CountSubresources(); i++)
          TrackTextureSequenceNumber(texture, i);
      }
    } else {
      D3D11Buffer* buffer = static_cast<D3D11Buffer*>(pResource);

      if (buffer->HasSequenceNumber())
        TrackBufferSequenceNumber(buffer);
    }
  }

}
