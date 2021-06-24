#include "d3d11_cmdlist.h"
#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_texture.h"

constexpr static uint32_t MinFlushIntervalUs = 750;
constexpr static uint32_t IncFlushIntervalUs = 250;
constexpr static uint32_t MaxPendingSubmits  = 6;

namespace dxvk {
  
  D3D11ImmediateContext::D3D11ImmediateContext(
          D3D11Device*    pParent,
    const Rc<DxvkDevice>& Device)
  : D3D11DeviceContext(pParent, Device, DxvkCsChunkFlag::SingleUse),
    m_csThread(Device->createContext()),
    m_videoContext(this, Device) {
    EmitCs([
      cDevice          = m_device,
      cRelaxedBarriers = pParent->GetOptions()->relaxedBarriers
    ] (DxvkContext* ctx) {
      ctx->beginRecording(cDevice->createCommandList());

      if (cRelaxedBarriers)
        ctx->setBarrierControl(DxvkBarrierControl::IgnoreWriteAfterWrite);
    });
    
    ClearState();
  }
  
  
  D3D11ImmediateContext::~D3D11ImmediateContext() {
    Flush();
    SynchronizeCsThread();
    SynchronizeDevice();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::QueryInterface(REFIID riid, void** ppvObject) {
    if (riid == __uuidof(ID3D11VideoContext)) {
      *ppvObject = ref(&m_videoContext);
      return S_OK;
    }

    return D3D11DeviceContext::QueryInterface(riid, ppvObject);
  }


  D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE D3D11ImmediateContext::GetType() {
    return D3D11_DEVICE_CONTEXT_IMMEDIATE;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11ImmediateContext::GetContextFlags() {
    return 0;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::GetData(
          ID3D11Asynchronous*               pAsync,
          void*                             pData,
          UINT                              DataSize,
          UINT                              GetDataFlags) {
    if (!pAsync || (DataSize && !pData))
      return E_INVALIDARG;
    
    // Check whether the data size is actually correct
    if (DataSize && DataSize != pAsync->GetDataSize())
      return E_INVALIDARG;
    
    // Passing a non-null pData is actually allowed if
    // DataSize is 0, but we should ignore that pointer
    pData = DataSize ? pData : nullptr;

    // Get query status directly from the query object
    auto query = static_cast<D3D11Query*>(pAsync);
    HRESULT hr = query->GetData(pData, GetDataFlags);
    
    // If we're likely going to spin on the asynchronous object,
    // flush the context so that we're keeping the GPU busy.
    if (hr == S_FALSE) {
      // Don't mark the event query as stalling if the app does
      // not intend to spin on it. This reduces flushes on End.
      if (!(GetDataFlags & D3D11_ASYNC_GETDATA_DONOTFLUSH))
        query->NotifyStall();

      // Ignore the DONOTFLUSH flag here as some games will spin
      // on queries without ever flushing the context otherwise.
      FlushImplicit(FALSE);
    }
    
    return hr;
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::Begin(ID3D11Asynchronous* pAsync) {
    D3D10DeviceLock lock = LockContext();

    if (unlikely(!pAsync))
      return;
    
    auto query = static_cast<D3D11Query*>(pAsync);

    if (unlikely(!query->DoBegin()))
      return;

    EmitCs([cQuery = Com<D3D11Query, false>(query)]
    (DxvkContext* ctx) {
      cQuery->Begin(ctx);
    });
  }


  void STDMETHODCALLTYPE D3D11ImmediateContext::End(ID3D11Asynchronous* pAsync) {
    D3D10DeviceLock lock = LockContext();

    if (unlikely(!pAsync))
      return;
    
    auto query = static_cast<D3D11Query*>(pAsync);

    if (unlikely(!query->DoEnd())) {
      EmitCs([cQuery = Com<D3D11Query, false>(query)]
      (DxvkContext* ctx) {
        cQuery->Begin(ctx);
      });
    }

    EmitCs([cQuery = Com<D3D11Query, false>(query)]
    (DxvkContext* ctx) {
      cQuery->End(ctx);
    });

    if (unlikely(query->IsEvent())) {
      query->NotifyEnd();
      query->IsStalling()
        ? Flush()
        : FlushImplicit(TRUE);
    }
  }


  void STDMETHODCALLTYPE D3D11ImmediateContext::Flush() {
    Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);
  }


  void STDMETHODCALLTYPE D3D11ImmediateContext::Flush1(
          D3D11_CONTEXT_TYPE          ContextType,
          HANDLE                      hEvent) {
    m_parent->FlushInitContext();

    if (hEvent)
      SignalEvent(hEvent);
    
    D3D10DeviceLock lock = LockContext();
    
    if (m_csIsBusy || !m_csChunk->empty()) {
      // Add commands to flush the threaded
      // context, then flush the command list
      EmitCs([] (DxvkContext* ctx) {
        ctx->flushCommandList();
      });
      
      FlushCsChunk();
      
      // Reset flush timer used for implicit flushes
      m_lastFlush = dxvk::high_resolution_clock::now();
      m_csIsBusy  = false;
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::Signal(
          ID3D11Fence*                pFence,
          UINT64                      Value) {
    Logger::err("D3D11ImmediateContext::Signal: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::Wait(
          ID3D11Fence*                pFence,
          UINT64                      Value) {
    Logger::err("D3D11ImmediateContext::Wait: Not implemented");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11ImmediateContext::ExecuteCommandList(
          ID3D11CommandList*  pCommandList,
          BOOL                RestoreContextState) {
    D3D10DeviceLock lock = LockContext();

    auto commandList = static_cast<D3D11CommandList*>(pCommandList);
    
    // Flush any outstanding commands so that
    // we don't mess up the execution order
    FlushCsChunk();
    
    // As an optimization, flush everything if the
    // number of pending draw calls is high enough.
    FlushImplicit(FALSE);
    
    // Dispatch command list to the CS thread and
    // restore the immediate context's state
    commandList->EmitToCsThread(&m_csThread);
    
    if (RestoreContextState)
      RestoreState();
    else
      ClearState();
    
    // Mark CS thread as busy so that subsequent
    // flush operations get executed correctly.
    m_csIsBusy = true;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::FinishCommandList(
          BOOL                RestoreDeferredContextState,
          ID3D11CommandList   **ppCommandList) {
    InitReturnPtr(ppCommandList);
    
    Logger::err("D3D11: FinishCommandList called on immediate context");
    return DXGI_ERROR_INVALID_CALL;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::Map(
          ID3D11Resource*             pResource,
          UINT                        Subresource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    D3D10DeviceLock lock = LockContext();

    if (unlikely(!pResource))
      return E_INVALIDARG;
    
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);

    HRESULT hr;
    
    if (likely(resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER)) {
      hr = MapBuffer(
        static_cast<D3D11Buffer*>(pResource),
        MapType, MapFlags, pMappedResource);
    } else {
      hr = MapImage(
        GetCommonTexture(pResource),
        Subresource, MapType, MapFlags,
        pMappedResource);
    }

    if (unlikely(FAILED(hr)))
      *pMappedResource = D3D11_MAPPED_SUBRESOURCE();

    return hr;
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::Unmap(
          ID3D11Resource*             pResource,
          UINT                        Subresource) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    if (unlikely(resourceDim != D3D11_RESOURCE_DIMENSION_BUFFER)) {
      D3D10DeviceLock lock = LockContext();
      UnmapImage(GetCommonTexture(pResource), Subresource);
    }
  }

  void STDMETHODCALLTYPE D3D11ImmediateContext::UpdateSubresource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D11_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch) {
    FlushImplicit(FALSE);

    D3D11DeviceContext::UpdateSubresource(
      pDstResource, DstSubresource, pDstBox,
      pSrcData, SrcRowPitch, SrcDepthPitch);
  }

  
  void STDMETHODCALLTYPE D3D11ImmediateContext::UpdateSubresource1(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
    const D3D11_BOX*                        pDstBox,
    const void*                             pSrcData,
          UINT                              SrcRowPitch,
          UINT                              SrcDepthPitch,
          UINT                              CopyFlags) {
    FlushImplicit(FALSE);

    D3D11DeviceContext::UpdateSubresource1(
      pDstResource, DstSubresource, pDstBox,
      pSrcData, SrcRowPitch, SrcDepthPitch,
      CopyFlags);
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::OMSetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    FlushImplicit(TRUE);
    
    D3D11DeviceContext::OMSetRenderTargets(
      NumViews, ppRenderTargetViews, pDepthStencilView);
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::OMSetRenderTargetsAndUnorderedAccessViews(
          UINT                              NumRTVs,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView,
          UINT                              UAVStartSlot,
          UINT                              NumUAVs,
          ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
    const UINT*                             pUAVInitialCounts) {
    FlushImplicit(TRUE);

    D3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(
      NumRTVs, ppRenderTargetViews, pDepthStencilView,
      UAVStartSlot, NumUAVs, ppUnorderedAccessViews,
      pUAVInitialCounts);
  }
  
  
  HRESULT D3D11ImmediateContext::MapBuffer(
          D3D11Buffer*                pResource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    if (unlikely(!pMappedResource))
      return E_INVALIDARG;

    if (unlikely(pResource->GetMapMode() == D3D11_COMMON_BUFFER_MAP_MODE_NONE)) {
      Logger::err("D3D11: Cannot map a device-local buffer");
      return E_INVALIDARG;
    }
    
    if (MapType == D3D11_MAP_WRITE_DISCARD) {
      // Allocate a new backing slice for the buffer and set
      // it as the 'new' mapped slice. This assumes that the
      // only way to invalidate a buffer is by mapping it.
      auto physSlice = pResource->DiscardSlice();
      pMappedResource->pData      = physSlice.mapPtr;
      pMappedResource->RowPitch   = pResource->Desc()->ByteWidth;
      pMappedResource->DepthPitch = pResource->Desc()->ByteWidth;
      
      EmitCs([
        cBuffer      = pResource->GetBuffer(),
        cBufferSlice = physSlice
      ] (DxvkContext* ctx) {
        ctx->invalidateBuffer(cBuffer, cBufferSlice);
      });

      return S_OK;
    } else {
      // Wait until the resource is no longer in use
      if (MapType != D3D11_MAP_WRITE_NO_OVERWRITE) {
        if (!WaitForResource(pResource->GetBuffer(), MapType, MapFlags))
          return DXGI_ERROR_WAS_STILL_DRAWING;
      }

      // Use map pointer from previous map operation. This
      // way we don't have to synchronize with the CS thread
      // if the map mode is D3D11_MAP_WRITE_NO_OVERWRITE.
      DxvkBufferSliceHandle physSlice = pResource->GetMappedSlice();
      
      pMappedResource->pData      = physSlice.mapPtr;
      pMappedResource->RowPitch   = pResource->Desc()->ByteWidth;
      pMappedResource->DepthPitch = pResource->Desc()->ByteWidth;
      return S_OK;
    }
  }
  
  
  HRESULT D3D11ImmediateContext::MapImage(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    const Rc<DxvkImage>  mappedImage  = pResource->GetImage();
    const Rc<DxvkBuffer> mappedBuffer = pResource->GetMappedBuffer(Subresource);

    auto mapMode = pResource->GetMapMode();
    
    if (unlikely(mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_NONE)) {
      Logger::err("D3D11: Cannot map a device-local image");
      return E_INVALIDARG;
    }

    if (unlikely(Subresource >= pResource->CountSubresources()))
      return E_INVALIDARG;
    
    if (likely(pMappedResource != nullptr)) {
      // Resources with an unknown memory layout cannot return a pointer
      if (pResource->Desc()->Usage         == D3D11_USAGE_DEFAULT
       && pResource->Desc()->TextureLayout == D3D11_TEXTURE_LAYOUT_UNDEFINED)
        return E_INVALIDARG;
    } else {
      if (pResource->Desc()->Usage != D3D11_USAGE_DEFAULT)
        return E_INVALIDARG;
    }

    VkFormat packedFormat = m_parent->LookupPackedFormat(
      pResource->Desc()->Format, pResource->GetFormatMode()).Format;
    
    auto formatInfo = imageFormatInfo(packedFormat);
    void* mapPtr;

    if (mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      // Wait for the resource to become available
      if (!WaitForResource(mappedImage, MapType, MapFlags))
        return DXGI_ERROR_WAS_STILL_DRAWING;
      
      // Query the subresource's memory layout and hope that
      // the application respects the returned pitch values.
      mapPtr = mappedImage->mapPtr(0);
    } else {
      if (MapType == D3D11_MAP_WRITE_DISCARD) {
        // We do not have to preserve the contents of the
        // buffer if the entire image gets discarded.
        DxvkBufferSliceHandle physSlice = pResource->DiscardSlice(Subresource);
        
        EmitCs([
          cImageBuffer = mappedBuffer,
          cBufferSlice = physSlice
        ] (DxvkContext* ctx) {
          ctx->invalidateBuffer(cImageBuffer, cBufferSlice);
        });

        mapPtr = physSlice.mapPtr;
      } else {
        bool wait = MapType != D3D11_MAP_WRITE_NO_OVERWRITE
                 || mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;
        
        // Wait for mapped buffer to become available
        if (wait && !WaitForResource(mappedBuffer, MapType, MapFlags))
          return DXGI_ERROR_WAS_STILL_DRAWING;
        
        mapPtr = pResource->GetMappedSlice(Subresource).mapPtr;
      }
    }

    // Mark the given subresource as mapped
    pResource->SetMapType(Subresource, MapType);

    if (pMappedResource) {
      auto layout = pResource->GetSubresourceLayout(formatInfo->aspectMask, Subresource);
      pMappedResource->pData      = reinterpret_cast<char*>(mapPtr) + layout.Offset;
      pMappedResource->RowPitch   = layout.RowPitch;
      pMappedResource->DepthPitch = layout.DepthPitch;
    }

    return S_OK;
  }
  
  
  void D3D11ImmediateContext::UnmapImage(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource) {
    D3D11_MAP mapType = pResource->GetMapType(Subresource);
    pResource->SetMapType(Subresource, D3D11_MAP(~0u));

    if (mapType == D3D11_MAP(~0u)
     || mapType == D3D11_MAP_READ)
      return;
    
    if (pResource->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER) {
      // Now that data has been written into the buffer,
      // we need to copy its contents into the image
      VkImageAspectFlags aspectMask = imageFormatInfo(pResource->GetPackedFormat())->aspectMask;
      VkImageSubresource subresource = pResource->GetSubresourceFromIndex(aspectMask, Subresource);

      UpdateImage(pResource, &subresource, VkOffset3D { 0, 0, 0 },
        pResource->MipLevelExtent(subresource.mipLevel),
        DxvkBufferSlice(pResource->GetMappedBuffer(Subresource)));
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::SwapDeviceContextState(
          ID3DDeviceContextState*           pState,
          ID3DDeviceContextState**          ppPreviousState) {
    InitReturnPtr(ppPreviousState);

    if (!pState)
      return;
    
    Com<D3D11DeviceContextState> oldState = std::move(m_stateObject);
    Com<D3D11DeviceContextState> newState = static_cast<D3D11DeviceContextState*>(pState);

    if (oldState == nullptr)
      oldState = new D3D11DeviceContextState(m_parent);
    
    if (ppPreviousState)
      *ppPreviousState = oldState.ref();
    
    m_stateObject = newState;

    oldState->SetState(m_state);
    newState->GetState(m_state);

    RestoreState();
  }


  void D3D11ImmediateContext::SynchronizeCsThread() {
    D3D10DeviceLock lock = LockContext();

    // Dispatch current chunk so that all commands
    // recorded prior to this function will be run
    FlushCsChunk();
    
    if (m_csThread.isBusy())
      m_csThread.synchronize();
  }
  
  
  void D3D11ImmediateContext::SynchronizeDevice() {
    m_device->waitForIdle();
  }
  
  
  bool D3D11ImmediateContext::WaitForResource(
    const Rc<DxvkResource>&                 Resource,
          D3D11_MAP                         MapType,
          UINT                              MapFlags) {
    // Determine access type to wait for based on map mode
    DxvkAccess access = MapType == D3D11_MAP_READ
      ? DxvkAccess::Write
      : DxvkAccess::Read;
    
    // Wait for the any pending D3D11 command to be executed
    // on the CS thread so that we can determine whether the
    // resource is currently in use or not.
    if (!Resource->isInUse(access))
      SynchronizeCsThread();
    
    if (Resource->isInUse(access)) {
      if (MapFlags & D3D11_MAP_FLAG_DO_NOT_WAIT) {
        // We don't have to wait, but misbehaving games may
        // still try to spin on `Map` until the resource is
        // idle, so we should flush pending commands
        FlushImplicit(FALSE);
        return false;
      } else {
        // Make sure pending commands using the resource get
        // executed on the the GPU if we have to wait for it
        Flush();
        SynchronizeCsThread();
        
        Resource->waitIdle(access);
      }
    }
    
    return true;
  }
  
  
  void D3D11ImmediateContext::EmitCsChunk(DxvkCsChunkRef&& chunk) {
    m_csThread.dispatchChunk(std::move(chunk));
    m_csIsBusy = true;
  }


  void D3D11ImmediateContext::FlushImplicit(BOOL StrongHint) {
    // Flush only if the GPU is about to go idle, in
    // order to keep the number of submissions low.
    uint32_t pending = m_device->pendingSubmissions();

    if (StrongHint || pending <= MaxPendingSubmits) {
      auto now = dxvk::high_resolution_clock::now();

      uint32_t delay = MinFlushIntervalUs
                     + IncFlushIntervalUs * pending;

      // Prevent flushing too often in short intervals.
      if (now - m_lastFlush >= std::chrono::microseconds(delay))
        Flush();
    }
  }


  void D3D11ImmediateContext::SignalEvent(HANDLE hEvent) {
    uint64_t value = ++m_eventCount;

    if (m_eventSignal == nullptr)
      m_eventSignal = new sync::Win32Fence();

    m_eventSignal->setEvent(hEvent, value);

    EmitCs([
      cSignal = m_eventSignal,
      cValue  = value
    ] (DxvkContext* ctx) {
      ctx->signal(cSignal, cValue);
    });
  }
  
}
