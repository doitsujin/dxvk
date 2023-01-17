#include "d3d11_context_def.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11DeferredContext::D3D11DeferredContext(
          D3D11Device*    pParent,
    const Rc<DxvkDevice>& Device,
          UINT            ContextFlags)
  : D3D11CommonContext<D3D11DeferredContext>(pParent, Device, ContextFlags, GetCsChunkFlags(pParent)),
    m_commandList (CreateCommandList()) {
    ResetContextState();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeferredContext::GetData(
          ID3D11Asynchronous*               pAsync,
          void*                             pData,
          UINT                              DataSize,
          UINT                              GetDataFlags) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11: GetData called on a deferred context");

    return DXGI_ERROR_INVALID_CALL;
  }


  void STDMETHODCALLTYPE D3D11DeferredContext::Begin(
          ID3D11Asynchronous*         pAsync) {
    D3D10DeviceLock lock = LockContext();

    if (unlikely(!pAsync))
      return;

    Com<D3D11Query, false> query(static_cast<D3D11Query*>(pAsync));

    if (unlikely(!query->IsScoped()))
      return;

    auto entry = std::find(
      m_queriesBegun.begin(),
      m_queriesBegun.end(), query);

    if (unlikely(entry != m_queriesBegun.end()))
      return;

    EmitCs([cQuery = query]
    (DxvkContext* ctx) {
      cQuery->Begin(ctx);
    });

    m_queriesBegun.push_back(std::move(query));
  }


  void STDMETHODCALLTYPE D3D11DeferredContext::End(
          ID3D11Asynchronous*         pAsync) {
    D3D10DeviceLock lock = LockContext();

    if (unlikely(!pAsync))
      return;

    Com<D3D11Query, false> query(static_cast<D3D11Query*>(pAsync));

    if (query->IsScoped()) {
      auto entry = std::find(
        m_queriesBegun.begin(),
        m_queriesBegun.end(), query);

      if (likely(entry != m_queriesBegun.end())) {
        m_queriesBegun.erase(entry);
      } else {
        EmitCs([cQuery = query]
        (DxvkContext* ctx) {
          cQuery->Begin(ctx);
        });
      }
    }

    m_commandList->AddQuery(query.ptr());

    EmitCs([cQuery = std::move(query)]
    (DxvkContext* ctx) {
      cQuery->End(ctx);
    });
  }


  void STDMETHODCALLTYPE D3D11DeferredContext::Flush() {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11: Flush called on a deferred context");
  }


  void STDMETHODCALLTYPE D3D11DeferredContext::Flush1(
          D3D11_CONTEXT_TYPE          ContextType,
          HANDLE                      hEvent) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11: Flush1 called on a deferred context");
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeferredContext::Signal(
          ID3D11Fence*                pFence,
          UINT64                      Value) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11: Signal called on a deferred context");

    return DXGI_ERROR_INVALID_CALL;
  }


  HRESULT STDMETHODCALLTYPE D3D11DeferredContext::Wait(
          ID3D11Fence*                pFence,
          UINT64                      Value) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11: Wait called on a deferred context");

    return DXGI_ERROR_INVALID_CALL;
  }


  void STDMETHODCALLTYPE D3D11DeferredContext::ExecuteCommandList(
          ID3D11CommandList*  pCommandList,
          BOOL                RestoreContextState) {
    D3D10DeviceLock lock = LockContext();

    // Clear state so that the command list can't observe any
    // current context state. The command list itself will clean
    // up after execution to ensure that no state changes done
    // by the command list are visible to the immediate context.
    ResetCommandListState();

    // Flush any outstanding commands so that
    // we don't mess up the execution order
    FlushCsChunk();
    
    // Record any chunks from the given command list into the
    // current command list and deal with context state
    auto commandList = static_cast<D3D11CommandList*>(pCommandList);
    m_chunkId = m_commandList->AddCommandList(commandList);
    
    // Restore deferred context state
    if (RestoreContextState)
      RestoreCommandListState();
    else
      ResetContextState();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeferredContext::FinishCommandList(
          BOOL                RestoreDeferredContextState,
          ID3D11CommandList   **ppCommandList) {
    D3D10DeviceLock lock = LockContext();

    // End all queries that were left active by the app
    FinalizeQueries();

    // Clean up command list state so that the any state changed
    // by this command list does not affect the calling context.
    // This also ensures that the command list is never empty.
    ResetCommandListState();

    // Make sure all commands are visible to the command list
    FlushCsChunk();
    
    if (ppCommandList)
      *ppCommandList = m_commandList.ref();

    // Create a clean command list, and if requested, restore all
    // previously set context state. Otherwise, reset the context.
    // Any use of ExecuteCommandList will reset command list state
    // before the command list is actually executed.
    m_commandList = CreateCommandList();
    m_chunkId = 0;
    
    if (RestoreDeferredContextState)
      RestoreCommandListState();
    else
      ResetContextState();
    
    m_mappedResources.clear();
    ResetStagingBuffer();
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DeferredContext::Map(
          ID3D11Resource*             pResource,
          UINT                        Subresource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    D3D10DeviceLock lock = LockContext();

    if (unlikely(!pResource || !pMappedResource))
      return E_INVALIDARG;
    
    if (MapType == D3D11_MAP_WRITE_DISCARD) {
      D3D11_RESOURCE_DIMENSION resourceDim;
      pResource->GetType(&resourceDim);

      D3D11_MAPPED_SUBRESOURCE mapInfo;
      HRESULT status = resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER
        ? MapBuffer(pResource,              &mapInfo)
        : MapImage (pResource, Subresource, &mapInfo);
      
      if (unlikely(FAILED(status))) {
        *pMappedResource = D3D11_MAPPED_SUBRESOURCE();
        return status;
      }
      
      AddMapEntry(pResource, Subresource, resourceDim, mapInfo);
      *pMappedResource = mapInfo;
      return S_OK;
    } else if (MapType == D3D11_MAP_WRITE_NO_OVERWRITE) {
      // The resource must be mapped with D3D11_MAP_WRITE_DISCARD
      // before it can be mapped with D3D11_MAP_WRITE_NO_OVERWRITE.
      auto entry = FindMapEntry(pResource, Subresource);
      
      if (unlikely(!entry)) {
        *pMappedResource = D3D11_MAPPED_SUBRESOURCE();
        return E_INVALIDARG;
      }
      
      // Return same memory region as earlier
      *pMappedResource = entry->MapInfo;
      return S_OK;
    } else {
      // Not allowed on deferred contexts
      *pMappedResource = D3D11_MAPPED_SUBRESOURCE();
      return E_INVALIDARG;
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11DeferredContext::Unmap(
          ID3D11Resource*             pResource,
          UINT                        Subresource) {
    // No-op, updates are committed in Map
  }
  
  
  void STDMETHODCALLTYPE D3D11DeferredContext::SwapDeviceContextState(
          ID3DDeviceContextState*           pState,
          ID3DDeviceContextState**          ppPreviousState) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11: SwapDeviceContextState called on a deferred context");
  }


  HRESULT D3D11DeferredContext::MapBuffer(
          ID3D11Resource*               pResource,
          D3D11_MAPPED_SUBRESOURCE*     pMappedResource) {
    D3D11Buffer* pBuffer = static_cast<D3D11Buffer*>(pResource);
    
    if (unlikely(pBuffer->GetMapMode() == D3D11_COMMON_BUFFER_MAP_MODE_NONE)) {
      Logger::err("D3D11: Cannot map a device-local buffer");
      return E_INVALIDARG;
    }
    
    pMappedResource->RowPitch     = pBuffer->Desc()->ByteWidth;
    pMappedResource->DepthPitch   = pBuffer->Desc()->ByteWidth;
    
    if (likely(m_csFlags.test(DxvkCsChunkFlag::SingleUse))) {
      // For resources that cannot be written by the GPU,
      // we may write to the buffer resource directly and
      // just swap in the buffer slice as needed.
      auto bufferSlice = pBuffer->AllocSlice();
      pMappedResource->pData = bufferSlice.mapPtr;

      EmitCs([
        cDstBuffer = pBuffer->GetBuffer(),
        cPhysSlice = bufferSlice
      ] (DxvkContext* ctx) {
        ctx->invalidateBuffer(cDstBuffer, cPhysSlice);
      });
    } else {
      // For GPU-writable resources, we need a data slice
      // to perform the update operation at execution time.
      auto dataSlice = AllocUpdateBufferSlice(pBuffer->Desc()->ByteWidth);
      pMappedResource->pData = dataSlice.ptr();

      EmitCs([
        cDstBuffer = pBuffer->GetBuffer(),
        cDataSlice = dataSlice
      ] (DxvkContext* ctx) {
        DxvkBufferSliceHandle slice = cDstBuffer->allocSlice();
        std::memcpy(slice.mapPtr, cDataSlice.ptr(), cDataSlice.length());
        ctx->invalidateBuffer(cDstBuffer, slice);
      });
    }
    
    return S_OK;
  }
  
  
  HRESULT D3D11DeferredContext::MapImage(
          ID3D11Resource*               pResource,
          UINT                          Subresource,
          D3D11_MAPPED_SUBRESOURCE*     pMappedResource) {
    D3D11CommonTexture* pTexture = GetCommonTexture(pResource);
    
    if (unlikely(pTexture->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_NONE)) {
      Logger::err("D3D11: Cannot map a device-local image");
      return E_INVALIDARG;
    }

    if (unlikely(Subresource >= pTexture->CountSubresources()))
      return E_INVALIDARG;
    
    VkFormat packedFormat = pTexture->GetPackedFormat();
    
    auto formatInfo = lookupFormatInfo(packedFormat);
    auto subresource = pTexture->GetSubresourceFromIndex(
        formatInfo->aspectMask, Subresource);
    
    VkExtent3D levelExtent = pTexture->MipLevelExtent(subresource.mipLevel);
    
    auto layout = pTexture->GetSubresourceLayout(formatInfo->aspectMask, Subresource);
    auto dataSlice = AllocStagingBuffer(util::computeImageDataSize(packedFormat, levelExtent));
    
    pMappedResource->RowPitch   = layout.RowPitch;
    pMappedResource->DepthPitch = layout.DepthPitch;
    pMappedResource->pData      = dataSlice.mapPtr(0);

    UpdateImage(pTexture, &subresource,
      VkOffset3D { 0, 0, 0 }, levelExtent,
      std::move(dataSlice));
    return S_OK;
  }
  
  
  void D3D11DeferredContext::UpdateMappedBuffer(
          D3D11Buffer*                  pDstBuffer,
          UINT                          Offset,
          UINT                          Length,
    const void*                         pSrcData,
          UINT                          CopyFlags) {
    void* mapPtr = nullptr;

    if (unlikely(CopyFlags == D3D11_COPY_NO_OVERWRITE)) {
      auto entry = FindMapEntry(pDstBuffer, 0);

      if (entry)
        mapPtr = entry->MapInfo.pData;
    }

    if (likely(!mapPtr)) {
      // The caller validates the map mode, so we can
      // safely ignore the MapBuffer return value here
      D3D11_MAPPED_SUBRESOURCE mapInfo;
      MapBuffer(pDstBuffer, &mapInfo);
      AddMapEntry(pDstBuffer, 0, D3D11_RESOURCE_DIMENSION_BUFFER, mapInfo);
      mapPtr = mapInfo.pData;
    }

    std::memcpy(reinterpret_cast<char*>(mapPtr) + Offset, pSrcData, Length);
  }


  void D3D11DeferredContext::FinalizeQueries() {
    for (auto& query : m_queriesBegun) {
      m_commandList->AddQuery(query.ptr());

      EmitCs([cQuery = std::move(query)]
      (DxvkContext* ctx) {
        cQuery->End(ctx);
      });
    }

    m_queriesBegun.clear();
  }


  Com<D3D11CommandList> D3D11DeferredContext::CreateCommandList() {
    return new D3D11CommandList(m_parent, m_flags);
  }
  
  
  void D3D11DeferredContext::EmitCsChunk(DxvkCsChunkRef&& chunk) {
    m_chunkId = m_commandList->AddChunk(std::move(chunk));
  }


  uint64_t D3D11DeferredContext::GetCurrentChunkId() const {
    return m_csChunk->empty() ? m_chunkId : m_chunkId + 1;
  }


  void D3D11DeferredContext::TrackTextureSequenceNumber(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource) {
    m_commandList->TrackResourceUsage(
      pResource->GetInterface(),
      pResource->GetDimension(),
      Subresource, GetCurrentChunkId());
  }


  void D3D11DeferredContext::TrackBufferSequenceNumber(
          D3D11Buffer*                pResource) {
    m_commandList->TrackResourceUsage(pResource,
      D3D11_RESOURCE_DIMENSION_BUFFER, 0,
      GetCurrentChunkId());
  }


  D3D11DeferredContextMapEntry* D3D11DeferredContext::FindMapEntry(
          ID3D11Resource*               pResource,
          UINT                          Subresource) {
    // Recently mapped resources as well as entries with
    // up-to-date map infos will be located at the end
    // of the resource array, so scan in reverse order.
    size_t size = m_mappedResources.size();

    for (size_t i = 1; i <= size; i++) {
      auto entry = &m_mappedResources[size - i];

      if (entry->Resource.Get()            == pResource
       && entry->Resource.GetSubresource() == Subresource)
        return entry;
    }

    return nullptr;
  }

  void D3D11DeferredContext::AddMapEntry(
          ID3D11Resource*               pResource,
          UINT                          Subresource,
          D3D11_RESOURCE_DIMENSION      ResourceType,
    const D3D11_MAPPED_SUBRESOURCE&     MapInfo) {
    m_mappedResources.emplace_back(pResource,
      Subresource, ResourceType, MapInfo);
  }


  DxvkCsChunkFlags D3D11DeferredContext::GetCsChunkFlags(
          D3D11Device*                  pDevice) {
    return pDevice->GetOptions()->dcSingleUseMode
      ? DxvkCsChunkFlags(DxvkCsChunkFlag::SingleUse)
      : DxvkCsChunkFlags();
  }

}