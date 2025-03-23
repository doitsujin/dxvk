#include "d3d11_context_def.h"
#include "d3d11_device.h"

namespace dxvk {
  
  D3D11DeferredContext::D3D11DeferredContext(
          D3D11Device*    pParent,
    const Rc<DxvkDevice>& Device,
          UINT            ContextFlags)
  : D3D11CommonContext<D3D11DeferredContext>(pParent, Device, ContextFlags, 0u),
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
    
    if (likely(MapType == D3D11_MAP_WRITE_DISCARD)) {
      D3D11_RESOURCE_DIMENSION resourceDim;
      pResource->GetType(&resourceDim);

      return likely(resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER)
        ? MapBuffer(pResource, pMappedResource)
        : MapImage(pResource, Subresource, pMappedResource);
    } else if (likely(MapType == D3D11_MAP_WRITE_NO_OVERWRITE)) {
      // The resource must be mapped with D3D11_MAP_WRITE_DISCARD
      // before it can be mapped with D3D11_MAP_WRITE_NO_OVERWRITE.
      D3D11_RESOURCE_DIMENSION resourceDim;
      pResource->GetType(&resourceDim);

      if (likely(resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER)) {
        D3D11_MAPPED_SUBRESOURCE sr = FindMapEntry(static_cast<D3D11Buffer*>(pResource)->GetCookie());
        pMappedResource->pData = sr.pData;

        if (unlikely(!sr.pData))
          return D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD;

        pMappedResource->RowPitch = sr.RowPitch;
        pMappedResource->DepthPitch = sr.DepthPitch;
        return S_OK;
      } else {
        // Images cannot be mapped with NO_OVERWRITE
        pMappedResource->pData = nullptr;
        return E_INVALIDARG;
      }
    } else {
      // Not allowed on deferred contexts
      pMappedResource->pData = nullptr;
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
      pMappedResource->pData = nullptr;
      return E_INVALIDARG;
    }

    auto bufferSlice = pBuffer->AllocSlice(&m_allocationCache);
    pMappedResource->pData        = bufferSlice->mapPtr();
    pMappedResource->RowPitch     = pBuffer->Desc()->ByteWidth;
    pMappedResource->DepthPitch   = pBuffer->Desc()->ByteWidth;

    EmitCs([
      cDstBuffer = pBuffer->GetBuffer(),
      cDstSlice  = std::move(bufferSlice)
    ] (DxvkContext* ctx) {
      ctx->invalidateBuffer(cDstBuffer, Rc<DxvkResourceAllocation>(cDstSlice));
    });

    AddMapEntry(pBuffer->GetCookie(), *pMappedResource);
    return S_OK;
  }
  
  
  HRESULT D3D11DeferredContext::MapImage(
          ID3D11Resource*               pResource,
          UINT                          Subresource,
          D3D11_MAPPED_SUBRESOURCE*     pMappedResource) {
    D3D11CommonTexture* pTexture = GetCommonTexture(pResource);
    
    if (unlikely(Subresource >= pTexture->CountSubresources())) {
      pMappedResource->pData = nullptr;
      return E_INVALIDARG;
    }

    if (unlikely(pTexture->Desc()->Usage != D3D11_USAGE_DYNAMIC)) {
      pMappedResource->pData = nullptr;
      return E_INVALIDARG;
    }

    VkFormat packedFormat = pTexture->GetPackedFormat();
    auto formatInfo = lookupFormatInfo(packedFormat);
    auto layout = pTexture->GetSubresourceLayout(formatInfo->aspectMask, Subresource);

    if (pTexture->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      auto storage = pTexture->AllocStorage();
      auto mapPtr = storage->mapPtr();

      EmitCs([
        cImage = pTexture->GetImage(),
        cStorage = std::move(storage)
      ] (DxvkContext* ctx) {
        ctx->invalidateImage(cImage, Rc<DxvkResourceAllocation>(cStorage));
        ctx->initImage(cImage, VK_IMAGE_LAYOUT_PREINITIALIZED);
      });

      pMappedResource->RowPitch   = layout.RowPitch;
      pMappedResource->DepthPitch = layout.DepthPitch;
      pMappedResource->pData      = mapPtr;
      return S_OK;
    } else {
      auto dataSlice = AllocStagingBuffer(layout.Size);

      pMappedResource->RowPitch   = layout.RowPitch;
      pMappedResource->DepthPitch = layout.DepthPitch;
      pMappedResource->pData      = dataSlice.mapPtr(0);

      auto subresource = pTexture->GetSubresourceFromIndex(formatInfo->aspectMask, Subresource);
      auto mipExtent = pTexture->MipLevelExtent(subresource.mipLevel);

      UpdateImage(pTexture, &subresource,
        VkOffset3D { 0, 0, 0 }, mipExtent,
        std::move(dataSlice));

      return S_OK;
    }
  }
  
  
  void D3D11DeferredContext::UpdateMappedBuffer(
          D3D11Buffer*                  pDstBuffer,
          UINT                          Offset,
          UINT                          Length,
    const void*                         pSrcData,
          UINT                          CopyFlags) {
    void* mapPtr = nullptr;

    if (unlikely(CopyFlags == D3D11_COPY_NO_OVERWRITE))
      mapPtr = FindMapEntry(pDstBuffer->GetCookie()).pData;

    if (likely(!mapPtr)) {
      // The caller validates the map mode, so we can
      // safely ignore the MapBuffer return value here
      D3D11_MAPPED_SUBRESOURCE mapInfo;
      MapBuffer(pDstBuffer, &mapInfo);
      AddMapEntry(pDstBuffer->GetCookie(), mapInfo);
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


  D3D11_MAPPED_SUBRESOURCE D3D11DeferredContext::FindMapEntry(
          uint64_t                      Cookie) {
    // Recently mapped resources as well as entries with
    // up-to-date map infos will be located at the end
    // of the resource array, so scan in reverse order.
    size_t size = m_mappedResources.size();

    for (size_t i = 1; i <= size; i++) {
      const auto& entry = m_mappedResources[size - i];

      if (entry.ResourceCookie == Cookie)
        return entry.MapInfo;
    }

    return D3D11_MAPPED_SUBRESOURCE();
  }

  void D3D11DeferredContext::AddMapEntry(
          uint64_t                      Cookie,
    const D3D11_MAPPED_SUBRESOURCE&     MapInfo) {
    m_mappedResources.push_back({ Cookie, MapInfo });
  }

}
