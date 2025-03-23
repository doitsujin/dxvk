#include "d3d11_cmdlist.h"
#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_fence.h"
#include "d3d11_texture.h"

#include "../util/util_win32_compat.h"

constexpr static uint32_t MinFlushIntervalUs = 750;
constexpr static uint32_t IncFlushIntervalUs = 250;
constexpr static uint32_t MaxPendingSubmits  = 6;

namespace dxvk {
  
  D3D11ImmediateContext::D3D11ImmediateContext(
          D3D11Device*    pParent,
    const Rc<DxvkDevice>& Device)
  : D3D11CommonContext<D3D11ImmediateContext>(pParent, Device, 0, DxvkCsChunkFlag::SingleUse),
    m_csThread(Device, Device->createContext()),
    m_submissionFence(new sync::CallbackFence()),
    m_flushTracker(GetMaxFlushType(pParent, Device)),
    m_stagingBufferFence(new sync::Fence(0)),
    m_multithread(this, false, pParent->GetOptions()->enableContextLock),
    m_videoContext(this, Device) {
    EmitCs([
      cDevice                 = m_device,
      cBarrierControlFlags    = pParent->GetOptionsBarrierControlFlags()
    ] (DxvkContext* ctx) {
      ctx->beginRecording(cDevice->createCommandList());

      ctx->setBarrierControl(cBarrierControlFlags);
    });

    // Stall here so that external submissions to the
    // CS thread can actually access the command list
    SynchronizeCsThread(DxvkCsThread::SynchronizeAll);
    
    ClearState();
  }
  
  
  D3D11ImmediateContext::~D3D11ImmediateContext() {
    // Avoids hanging when in this state, see comment
    // in DxvkDevice::~DxvkDevice.
    if (this_thread::isInModuleDetachment())
      return;

    ExecuteFlush(GpuFlushType::ExplicitFlush, nullptr, true);
    SynchronizeCsThread(DxvkCsThread::SynchronizeAll);
    SynchronizeDevice();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::QueryInterface(REFIID riid, void** ppvObject) {
    if (riid == __uuidof(ID3D10Multithread)) {
      *ppvObject = ref(&m_multithread);
      return S_OK;
    }

    if (riid == __uuidof(ID3D11VideoContext)) {
      *ppvObject = ref(&m_videoContext);
      return S_OK;
    }

    return D3D11CommonContext<D3D11ImmediateContext>::QueryInterface(riid, ppvObject);
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
      D3D10DeviceLock lock = LockContext();

      if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
        m_flushReason = "Query read-back";

      ConsiderFlush(GpuFlushType::ImplicitSynchronization);
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

    if (unlikely(query->TrackStalls())) {
      query->NotifyEnd();

      if (query->IsStalling())
        ExecuteFlush(GpuFlushType::ImplicitSynchronization, nullptr, false);
      else if (query->IsEvent())
        ConsiderFlush(GpuFlushType::ImplicitStrongHint);
    }
  }


  void STDMETHODCALLTYPE D3D11ImmediateContext::Flush() {
    D3D10DeviceLock lock = LockContext();

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      m_flushReason = "Explicit Flush";

    ExecuteFlush(GpuFlushType::ExplicitFlush, nullptr, true);
  }


  void STDMETHODCALLTYPE D3D11ImmediateContext::Flush1(
          D3D11_CONTEXT_TYPE          ContextType,
          HANDLE                      hEvent) {
    D3D10DeviceLock lock = LockContext();

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      m_flushReason = "Explicit Flush";

    ExecuteFlush(GpuFlushType::ExplicitFlush, hEvent, true);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::Signal(
          ID3D11Fence*                pFence,
          UINT64                      Value) {
    D3D10DeviceLock lock = LockContext();
    auto fence = static_cast<D3D11Fence*>(pFence);

    if (!fence)
      return E_INVALIDARG;

    EmitCs([
      cFence = fence->GetFence(),
      cValue = Value
    ] (DxvkContext* ctx) {
      ctx->signalFence(cFence, cValue);
    });

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      m_flushReason = "Fence signal";

    ExecuteFlush(GpuFlushType::ExplicitFlush, nullptr, true);
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::Wait(
          ID3D11Fence*                pFence,
          UINT64                      Value) {
    D3D10DeviceLock lock = LockContext();
    auto fence = static_cast<D3D11Fence*>(pFence);

    if (!fence)
      return E_INVALIDARG;

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      m_flushReason = "Fence wait";

    ExecuteFlush(GpuFlushType::ExplicitFlush, nullptr, true);

    EmitCs([
      cFence = fence->GetFence(),
      cValue = Value
    ] (DxvkContext* ctx) {
      ctx->waitFence(cFence, cValue);
    });

    return S_OK;
  }


  void STDMETHODCALLTYPE D3D11ImmediateContext::ExecuteCommandList(
          ID3D11CommandList*  pCommandList,
          BOOL                RestoreContextState) {
    D3D10DeviceLock lock = LockContext();

    auto commandList = static_cast<D3D11CommandList*>(pCommandList);

    // Reset dirty binding tracking before submitting any CS chunks.
    // This is needed so that any submission that might occur during
    // this call does not disrupt bindings set by the deferred context.
    ResetDirtyTracking();

    // Clear state so that the command list can't observe any
    // current context state. The command list itself will clean
    // up after execution to ensure that no state changes done
    // by the command list are visible to the immediate context.
    ResetCommandListState();

    // Flush any outstanding commands so that
    // we don't mess up the execution order
    FlushCsChunk();
    
    // As an optimization, flush everything if the
    // number of pending draw calls is high enough.
    ConsiderFlush(GpuFlushType::ImplicitWeakHint);

    // Dispatch command list to the CS thread
    commandList->EmitToCsThread([this] (DxvkCsChunkRef&& chunk, GpuFlushType flushType) {
      EmitCsChunk(std::move(chunk));

      // Return the sequence number from before the flush since
      // that is actually going to be needed for resource tracking
      uint64_t csSeqNum = m_csSeqNum;

      // Consider a flush after every chunk in case the app
      // submits a very large command list or the GPU is idle
      ConsiderFlush(flushType);
      return csSeqNum;
    });

    // Restore the immediate context's state
    if (RestoreContextState)
      RestoreCommandListState();
    else
      ResetContextState();
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

    if (likely(resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER)) {
      return MapBuffer(
        static_cast<D3D11Buffer*>(pResource),
        MapType, MapFlags, pMappedResource);
    } else {
      return MapImage(GetCommonTexture(pResource),
        Subresource, MapType, MapFlags, pMappedResource);
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::Unmap(
          ID3D11Resource*             pResource,
          UINT                        Subresource) {
    // Since it is very uncommon for images to be mapped compared
    // to buffers, we count the currently mapped images in order
    // to avoid a virtual method call in the common case.
    if (unlikely(m_mappedImageCount > 0)) {
      D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
      pResource->GetType(&resourceDim);

      if (resourceDim != D3D11_RESOURCE_DIMENSION_BUFFER) {
        D3D10DeviceLock lock = LockContext();
        UnmapImage(GetCommonTexture(pResource), Subresource);
      }
    }
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
      pMappedResource->pData = nullptr;
      return E_INVALIDARG;
    }

    VkDeviceSize bufferSize = pResource->Desc()->ByteWidth;

    if (likely(MapType == D3D11_MAP_WRITE_DISCARD)) {
      // Allocate a new backing slice for the buffer and set
      // it as the 'new' mapped slice. This assumes that the
      // only way to invalidate a buffer is by mapping it.
      auto bufferSlice = pResource->DiscardSlice(&m_allocationCache);
      pMappedResource->pData      = bufferSlice->mapPtr();
      pMappedResource->RowPitch   = bufferSize;
      pMappedResource->DepthPitch = bufferSize;
      
      EmitCs([
        cBuffer      = pResource->GetBuffer(),
        cBufferSlice = std::move(bufferSlice)
      ] (DxvkContext* ctx) mutable {
        ctx->invalidateBuffer(cBuffer, std::move(cBufferSlice));
      });

      // Ignore small buffers here. These are often updated per
      // draw and won't contribute much to memory waste anyway.
      if (unlikely(bufferSize > DxvkPageAllocator::PageSize))
        ThrottleDiscard(bufferSize);

      return S_OK;
    } else if (likely(MapType == D3D11_MAP_WRITE_NO_OVERWRITE)) {
      // Put this on a fast path without any extra checks since it's
      // a somewhat desired method to partially update large buffers
      pMappedResource->pData      = pResource->GetMapPtr();
      pMappedResource->RowPitch   = bufferSize;
      pMappedResource->DepthPitch = bufferSize;
      return S_OK;
    } else {
      // Quantum Break likes using MAP_WRITE on resources which would force
      // us to synchronize with the GPU multiple times per frame. In those
      // situations, if there are no pending GPU writes to the resource, we
      // can promote it to MAP_WRITE_DISCARD, but preserve the data by doing
      // a CPU copy from the previous buffer slice, to avoid the sync point.
      bool doInvalidatePreserve = false;

      auto buffer = pResource->GetBuffer();
      auto sequenceNumber = pResource->GetSequenceNumber();

      if (MapType != D3D11_MAP_READ && !MapFlags && bufferSize <= D3D11Initializer::MaxMemoryPerSubmission) {
        SynchronizeCsThread(sequenceNumber);

        bool hasWoAccess = buffer->isInUse(DxvkAccess::Write);
        bool hasRwAccess = buffer->isInUse(DxvkAccess::Read);

        if (hasRwAccess && !hasWoAccess) {
          // Uncached reads can be so slow that a GPU sync may actually be faster
          doInvalidatePreserve = buffer->memFlags() & VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        }
      }

      if (doInvalidatePreserve) {
        auto srcPtr = pResource->GetMapPtr();

        auto dstSlice = pResource->DiscardSlice(nullptr);
        auto dstPtr = dstSlice->mapPtr();

        EmitCs([
          cBuffer      = std::move(buffer),
          cBufferSlice = std::move(dstSlice)
        ] (DxvkContext* ctx) mutable {
          ctx->invalidateBuffer(cBuffer, std::move(cBufferSlice));
        });

        std::memcpy(dstPtr, srcPtr, bufferSize);
        pMappedResource->pData      = dstPtr;
        pMappedResource->RowPitch   = bufferSize;
        pMappedResource->DepthPitch = bufferSize;

        ThrottleDiscard(bufferSize);
        return S_OK;
      } else {
        if (!WaitForResource(*buffer, sequenceNumber, MapType, MapFlags)) {
          pMappedResource->pData = nullptr;
          return DXGI_ERROR_WAS_STILL_DRAWING;
        }

        pMappedResource->pData      = pResource->GetMapPtr();
        pMappedResource->RowPitch   = bufferSize;
        pMappedResource->DepthPitch = bufferSize;
        return S_OK;
      }
    }
  }
  
  
  HRESULT D3D11ImmediateContext::MapImage(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource,
          D3D11_MAP                   MapType,
          UINT                        MapFlags,
          D3D11_MAPPED_SUBRESOURCE*   pMappedResource) {
    auto mapMode = pResource->GetMapMode();

    if (pMappedResource)
      pMappedResource->pData = nullptr;

    if (unlikely(Subresource >= pResource->CountSubresources()))
      return E_INVALIDARG;

    switch (MapType) {
      case D3D11_MAP_READ: {
        if (!(pResource->Desc()->CPUAccessFlags & D3D11_CPU_ACCESS_READ))
          return E_INVALIDARG;
      } break;

      case D3D11_MAP_READ_WRITE: {
        if (!(pResource->Desc()->CPUAccessFlags & D3D11_CPU_ACCESS_READ)
         || !(pResource->Desc()->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE))
          return E_INVALIDARG;
      } break;

      case D3D11_MAP_WRITE: {
        if (!(pResource->Desc()->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
         || (pResource->Desc()->Usage == D3D11_USAGE_DYNAMIC))
          return E_INVALIDARG;
      } break;

      case D3D11_MAP_WRITE_DISCARD: {
        if (!(pResource->Desc()->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
         || pResource->Desc()->Usage != D3D11_USAGE_DYNAMIC)
          return E_INVALIDARG;
      } break;

      case D3D11_MAP_WRITE_NO_OVERWRITE: {
        // NO_OVERWRITE is explcitly banned for dynamic images
        if (!(pResource->Desc()->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
         || (pResource->Desc()->Usage != D3D11_USAGE_DEFAULT))
          return E_INVALIDARG;
      } break;
    }

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
    
    uint64_t sequenceNumber = pResource->GetSequenceNumber(Subresource);

    auto formatInfo = lookupFormatInfo(packedFormat);
    auto layout = pResource->GetSubresourceLayout(formatInfo->aspectMask, Subresource);

    if (mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      Rc<DxvkImage> mappedImage = pResource->GetImage();

      if (MapType == D3D11_MAP_WRITE_DISCARD) {
        EmitCs([
          cImage = std::move(mappedImage),
          cStorage = pResource->DiscardStorage()
        ] (DxvkContext* ctx) {
          ctx->invalidateImage(cImage, Rc<DxvkResourceAllocation>(cStorage));
          ctx->initImage(cImage, VK_IMAGE_LAYOUT_PREINITIALIZED);
        });

        ThrottleDiscard(layout.Size);
      } else if (MapType != D3D11_MAP_WRITE_NO_OVERWRITE) {
        if (!WaitForResource(*mappedImage, sequenceNumber, MapType, MapFlags))
          return DXGI_ERROR_WAS_STILL_DRAWING;
      }
    } else if (mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DYNAMIC) {
      // Nothing else to really do here, NotifyMap will ensure that we
      // actually get a staging buffer that isn't currently in use.
      ThrottleDiscard(layout.Size);
    } else {
      Rc<DxvkBuffer> mappedBuffer = pResource->GetMappedBuffer(Subresource);

      constexpr uint32_t DoInvalidate = (1u << 0);
      constexpr uint32_t DoPreserve   = (1u << 1);
      constexpr uint32_t DoWait       = (1u << 2);
      uint32_t doFlags;

      if (mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER) {
        // If the image can be written by the GPU, we need to update the
        // mapped staging buffer to reflect the current image contents.
        if (pResource->Desc()->Usage == D3D11_USAGE_DEFAULT) {
          bool needsReadback = !pResource->NeedsDirtyRegionTracking();

          needsReadback |= MapType == D3D11_MAP_READ
                        || MapType == D3D11_MAP_READ_WRITE;

          if (needsReadback)
            ReadbackImageBuffer(pResource, Subresource);
        }
      }

      if (MapType == D3D11_MAP_READ) {
        // Reads will not change the image content, so we only need
        // to wait for the GPU to finish writing to the mapped buffer.
        doFlags = DoWait;
      } else if (MapType == D3D11_MAP_WRITE_DISCARD) {
        doFlags = DoInvalidate;

        // If we know for sure that the mapped buffer is currently not
        // in use by the GPU, we don't have to allocate a new slice.
        if (m_csThread.lastSequenceNumber() >= sequenceNumber && !mappedBuffer->isInUse(DxvkAccess::Read))
          doFlags = 0;
      } else if (mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_STAGING && (MapFlags & D3D11_MAP_FLAG_DO_NOT_WAIT)) {
        // Always respect DO_NOT_WAIT for mapped staging images
        doFlags = DoWait;
      } else if (MapType != D3D11_MAP_WRITE_NO_OVERWRITE || mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER) {
        // Need to synchronize thread to determine pending GPU accesses
        SynchronizeCsThread(sequenceNumber);

        // Don't implicitly discard large very large resources
        // since that might lead to memory issues.
        VkDeviceSize bufferSize = mappedBuffer->info().size;

        if (bufferSize > D3D11Initializer::MaxMemoryPerSubmission) {
          // Don't check access flags, WaitForResource will return
          // early anyway if the resource is currently in use
          doFlags = DoWait;
        } else if (mappedBuffer->isInUse(DxvkAccess::Write)) {
          // There are pending GPU writes, need to wait for those
          doFlags = DoWait;
        } else if (mappedBuffer->isInUse(DxvkAccess::Read)) {
          // All pending GPU accesses are reads, so the buffer data
          // is still current, and we can prevent GPU synchronization
          // by creating a new slice with an exact copy of the data.
          doFlags = DoInvalidate | DoPreserve;
        } else {
          // There are no pending accesses, so we don't need to wait
          doFlags = 0;
        }
      } else {
        // No need to synchronize staging resources with NO_OVERWRITE
        // since the buffer will be used directly.
        doFlags = 0;
      }

      if (doFlags & DoInvalidate) {
        VkDeviceSize bufferSize = mappedBuffer->info().size;

        auto srcSlice = pResource->GetMappedSlice(Subresource);
        auto dstSlice = pResource->DiscardSlice(Subresource);

        auto srcPtr = srcSlice->mapPtr();
        auto dstPtr = dstSlice->mapPtr();

        EmitCs([
          cImageBuffer      = std::move(mappedBuffer),
          cImageBufferSlice = std::move(dstSlice)
        ] (DxvkContext* ctx) mutable {
          ctx->invalidateBuffer(cImageBuffer, std::move(cImageBufferSlice));
        });

        if (doFlags & DoPreserve)
          std::memcpy(dstPtr, srcPtr, bufferSize);

        ThrottleDiscard(bufferSize);
      } else {
        if (doFlags & DoWait) {
          // We cannot respect DO_NOT_WAIT for buffer-mapped resources since
          // our internal copies need to be transparent to the application.
          if (mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER)
            MapFlags &= ~D3D11_MAP_FLAG_DO_NOT_WAIT;

          // Wait for mapped buffer to become available
          if (!WaitForResource(*mappedBuffer, sequenceNumber, MapType, MapFlags))
            return DXGI_ERROR_WAS_STILL_DRAWING;
        }
      }
    }

    // Mark the subresource as successfully mapped
    pResource->NotifyMap(Subresource, MapType);

    if (pMappedResource) {
      pMappedResource->pData      = pResource->GetMapPtr(Subresource, layout.Offset);
      pMappedResource->RowPitch   = layout.RowPitch;
      pMappedResource->DepthPitch = layout.DepthPitch;
    }

    m_mappedImageCount += 1;
    return S_OK;
  }
  
  
  void D3D11ImmediateContext::UnmapImage(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource) {
    auto mapType = pResource->GetMapType(Subresource);
    auto mapMode = pResource->GetMapMode();

    if (mapType == D3D11CommonTexture::UnmappedSubresource)
      return;

    // Decrement mapped image counter only after making sure
    // the given subresource is actually mapped right now
    m_mappedImageCount -= 1;

    // If the texture has an image as well as a staging buffer,
    // upload the written buffer data to the image
    bool needsUpload = mapType != uint32_t(D3D11_MAP_READ)
      && (mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER || mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DYNAMIC);

    if (needsUpload) {
      if (pResource->NeedsDirtyRegionTracking()) {
        for (uint32_t i = 0; i < pResource->GetDirtyRegionCount(Subresource); i++) {
          D3D11_COMMON_TEXTURE_REGION region = pResource->GetDirtyRegion(Subresource, i);
          UpdateDirtyImageRegion(pResource, Subresource, &region);
        }
      } else {
        UpdateDirtyImageRegion(pResource, Subresource, nullptr);
      }
    }

    // Unmap the subresource. This will implicitly destroy the
    // staging buffer for dynamically mapped images.
    pResource->NotifyUnmap(Subresource);
  }
  
  
  void D3D11ImmediateContext::ReadbackImageBuffer(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource) {
    VkImageAspectFlags aspectMask = lookupFormatInfo(pResource->GetPackedFormat())->aspectMask;
    VkImageSubresource subresource = pResource->GetSubresourceFromIndex(aspectMask, Subresource);

    EmitCs([
      cSrcImage           = pResource->GetImage(),
      cSrcSubresource     = vk::makeSubresourceLayers(subresource),
      cDstBuffer          = pResource->GetMappedBuffer(Subresource),
      cPackedFormat       = pResource->GetPackedFormat()
    ] (DxvkContext* ctx) {
      VkOffset3D offset = { 0, 0, 0 };
      VkExtent3D extent = cSrcImage->mipLevelExtent(cSrcSubresource.mipLevel);

      ctx->copyImageToBuffer(cDstBuffer, 0, 0, 0, cPackedFormat,
        cSrcImage, cSrcSubresource, offset, extent);
    });

    if (pResource->HasSequenceNumber())
      TrackTextureSequenceNumber(pResource, Subresource);
  }


  void D3D11ImmediateContext::UpdateDirtyImageRegion(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource,
    const D3D11_COMMON_TEXTURE_REGION* pRegion) {
    auto formatInfo = lookupFormatInfo(pResource->GetPackedFormat());
    auto subresource = vk::makeSubresourceLayers(
      pResource->GetSubresourceFromIndex(formatInfo->aspectMask, Subresource));

    // Update the entire image if no dirty region was specified
    D3D11_COMMON_TEXTURE_REGION region;

    if (pRegion) {
      region = *pRegion;
    } else {
      region.Offset = VkOffset3D { 0, 0, 0 };
      region.Extent = pResource->MipLevelExtent(subresource.mipLevel);
    }

    auto subresourceLayout = pResource->GetSubresourceLayout(formatInfo->aspectMask, Subresource);

    // Update dirty region one aspect at a time, due to
    // how the data is laid out in the staging buffer.
    for (uint32_t i = 0; i < pResource->GetPlaneCount(); i++) {
      subresource.aspectMask = formatInfo->aspectMask;

      if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane))
        subresource.aspectMask = vk::getPlaneAspect(i);

      EmitCs([
        cDstImage       = pResource->GetImage(),
        cDstSubresource = subresource,
        cDstOffset      = region.Offset,
        cDstExtent      = region.Extent,
        cSrcBuffer      = pResource->GetMappedBuffer(Subresource),
        cSrcOffset      = pResource->ComputeMappedOffset(Subresource, i, region.Offset),
        cSrcRowPitch    = subresourceLayout.RowPitch,
        cSrcDepthPitch  = subresourceLayout.DepthPitch,
        cPackedFormat   = pResource->GetPackedFormat()
      ] (DxvkContext* ctx) {
        ctx->copyBufferToImage(
          cDstImage, cDstSubresource, cDstOffset, cDstExtent,
          cSrcBuffer, cSrcOffset, cSrcRowPitch, cSrcDepthPitch,
          cPackedFormat);
      });
    }

    if (pResource->HasSequenceNumber())
      TrackTextureSequenceNumber(pResource, Subresource);
  }


  void D3D11ImmediateContext::UpdateMappedBuffer(
          D3D11Buffer*                  pDstBuffer,
          UINT                          Offset,
          UINT                          Length,
    const void*                         pSrcData,
          UINT                          CopyFlags) {
    void* mapPtr = nullptr;

    if (likely(CopyFlags != D3D11_COPY_NO_OVERWRITE)) {
      auto bufferSlice = pDstBuffer->DiscardSlice(&m_allocationCache);
      mapPtr = bufferSlice->mapPtr();

      EmitCs([
        cBuffer      = pDstBuffer->GetBuffer(),
        cBufferSlice = std::move(bufferSlice)
      ] (DxvkContext* ctx) mutable {
        ctx->invalidateBuffer(cBuffer, std::move(cBufferSlice));
      });
    } else {
      mapPtr = pDstBuffer->GetMapPtr();
    }

    std::memcpy(reinterpret_cast<char*>(mapPtr) + Offset, pSrcData, Length);
  }


  void STDMETHODCALLTYPE D3D11ImmediateContext::SwapDeviceContextState(
          ID3DDeviceContextState*           pState,
          ID3DDeviceContextState**          ppPreviousState) {
    InitReturnPtr(ppPreviousState);

    if (!pState)
      return;

    // Clear dirty tracking here since all context state will be
    // re-applied anyway when the context state is swapped in again.
    ResetDirtyTracking();

    // Reset all state affected by the current context state.
    ResetCommandListState();

    Com<D3D11DeviceContextState, false> oldState = std::move(m_stateObject);
    Com<D3D11DeviceContextState, false> newState = static_cast<D3D11DeviceContextState*>(pState);

    if (oldState == nullptr)
      oldState = new D3D11DeviceContextState(m_parent);
    
    if (ppPreviousState)
      *ppPreviousState = oldState.ref();
    
    m_stateObject = newState;

    oldState->SetState(m_state);
    newState->GetState(m_state);

    // Restore all state affected by the new context state
    RestoreCommandListState();
  }


  void D3D11ImmediateContext::Acquire11on12Resource(
          ID3D11Resource*             pResource,
          VkImageLayout               SrcLayout) {
    D3D10DeviceLock lock = LockContext();

    auto texture = GetCommonTexture(pResource);
    auto buffer = GetCommonBuffer(pResource);

    if (buffer) {
      EmitCs([
        cBuffer   = buffer->GetBuffer()
      ] (DxvkContext* ctx) {
        ctx->emitBufferBarrier(cBuffer,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
          cBuffer->info().stages,
          cBuffer->info().access);
      });
    } else if (texture) {
      EmitCs([
        cImage    = texture->GetImage(),
        cLayout   = SrcLayout
      ] (DxvkContext* ctx) {
        ctx->emitImageBarrier(cImage, cLayout,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
          cImage->info().layout,
          cImage->info().stages,
          cImage->info().access);
      });
    }
  }


  void D3D11ImmediateContext::Release11on12Resource(
          ID3D11Resource*             pResource,
          VkImageLayout               DstLayout) {
    D3D10DeviceLock lock = LockContext();

    auto texture = GetCommonTexture(pResource);
    auto buffer = GetCommonBuffer(pResource);

    if (buffer) {
      EmitCs([
        cBuffer   = buffer->GetBuffer()
      ] (DxvkContext* ctx) {
        ctx->emitBufferBarrier(cBuffer,
          cBuffer->info().stages,
          cBuffer->info().access,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT);
      });
    } else if (texture) {
      EmitCs([
        cImage    = texture->GetImage(),
        cLayout   = DstLayout
      ] (DxvkContext* ctx) {
        ctx->emitImageBarrier(cImage,
          cImage->info().layout,
          cImage->info().stages,
          cImage->info().access,
          cLayout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT);
      });
    }
  }


  void D3D11ImmediateContext::SynchronizeCsThread(uint64_t SequenceNumber) {
    D3D10DeviceLock lock = LockContext();

    // Dispatch current chunk so that all commands
    // recorded prior to this function will be run
    if (SequenceNumber > m_csSeqNum)
      FlushCsChunk();
    
    m_csThread.synchronize(SequenceNumber);
  }
  
  
  void D3D11ImmediateContext::SynchronizeDevice() {
    m_device->waitForIdle();
  }
  
  
  void D3D11ImmediateContext::EndFrame(
          Rc<DxvkLatencyTracker>      LatencyTracker) {
    D3D10DeviceLock lock = LockContext();

    // Don't keep draw buffers alive indefinitely. This cannot be
    // done in ExecuteFlush because command recording itself might
    // flush, so no state changes are allowed to happen there.
    SetDrawBuffers(nullptr, nullptr);

    EmitCs<false>([
      cTracker = std::move(LatencyTracker)
    ] (DxvkContext* ctx) {
      ctx->endFrame();

      if (cTracker && cTracker->needsAutoMarkers())
        ctx->endLatencyTracking(cTracker);
    });
  }


  bool D3D11ImmediateContext::WaitForResource(
    const DxvkPagedResource&                Resource,
          uint64_t                          SequenceNumber,
          D3D11_MAP                         MapType,
          UINT                              MapFlags) {
    // Determine access type to wait for based on map mode
    DxvkAccess access = MapType == D3D11_MAP_READ
      ? DxvkAccess::Write
      : DxvkAccess::Read;
    
    // Wait for any CS chunk using the resource to execute, since
    // otherwise we cannot accurately determine if the resource is
    // actually being used by the GPU right now.
    if (!Resource.isInUse(access)) {
      SynchronizeCsThread(SequenceNumber);

      if (!Resource.isInUse(access))
        return true;
    }

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture))) {
      m_flushReason = str::format("Map ", Resource.getDebugName(), " (MAP",
        MapType != D3D11_MAP_WRITE ? "_READ" : "",
        MapType != D3D11_MAP_READ ? "_WRITE" : "", ")");
    }

    if (MapFlags & D3D11_MAP_FLAG_DO_NOT_WAIT) {
      // We don't have to wait, but misbehaving games may
      // still try to spin on `Map` until the resource is
      // idle, so we should flush pending commands
      ConsiderFlush(GpuFlushType::ImplicitSynchronization);
      return false;
    } else {
      // Make sure pending commands using the resource get
      // executed on the the GPU if we have to wait for it
      ExecuteFlush(GpuFlushType::ImplicitSynchronization, nullptr, false);
      SynchronizeCsThread(SequenceNumber);

      m_device->waitForResource(Resource, access);
      return true;
    }
  }


  void D3D11ImmediateContext::InjectCsChunk(
          DxvkCsQueue                 Queue,
          DxvkCsChunkRef&&            Chunk,
          bool                        Synchronize) {
    // Do not update the sequence number when emitting a chunk
    // from an external source since that would break tracking
    m_csThread.injectChunk(Queue, std::move(Chunk), Synchronize);
  }


  void D3D11ImmediateContext::EmitCsChunk(DxvkCsChunkRef&& chunk) {
    // Flush init commands so that the CS thread
    // can processe them before the first use.
    m_parent->FlushInitCommands();

    m_csSeqNum = m_csThread.dispatchChunk(std::move(chunk));
  }


  void D3D11ImmediateContext::TrackTextureSequenceNumber(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource) {
    uint64_t sequenceNumber = GetCurrentSequenceNumber();
    pResource->TrackSequenceNumber(Subresource, sequenceNumber);

    ConsiderFlush(GpuFlushType::ImplicitStrongHint);
  }


  void D3D11ImmediateContext::TrackBufferSequenceNumber(
          D3D11Buffer*                pResource) {
    uint64_t sequenceNumber = GetCurrentSequenceNumber();
    pResource->TrackSequenceNumber(sequenceNumber);

    ConsiderFlush(GpuFlushType::ImplicitStrongHint);
  }


  uint64_t D3D11ImmediateContext::GetCurrentSequenceNumber() {
    // We do not flush empty chunks, so if we are tracking a resource
    // immediately after a flush, we need to use the sequence number
    // of the previously submitted chunk to prevent deadlocks.
    return m_csChunk->empty() ? m_csSeqNum : m_csSeqNum + 1;
  }


  uint64_t D3D11ImmediateContext::GetPendingCsChunks() {
    return GetCurrentSequenceNumber() - m_flushSeqNum;
  }


  void D3D11ImmediateContext::ApplyDirtyNullBindings() {
    // At the end of a submission, set all bindings that have not been applied yet
    // to null on the DXVK context. This way, we avoid keeping resources alive that
    // are bound to the DXVK context but not to the immediate context.
    //
    // Note: This requires that all methods that may modify dirty bindings on the
    // DXVK context also reset the corresponding dirty bits *before* performing the
    // bind operation, or otherwise an implicit flush can potentially override them.
    auto& dirtyState = m_state.lazy.bindingsDirty;

    EmitCs<false>([
      cDirtyState = dirtyState
    ] (DxvkContext* ctx) {
      for (uint32_t i = 0; i < uint32_t(DxbcProgramType::Count); i++) {
        auto dxStage = DxbcProgramType(i);
        auto vkStage = GetShaderStage(dxStage);

        // Unbind all dirty constant buffers
        auto cbvSlot = computeConstantBufferBinding(dxStage, 0);

        for (uint32_t index : bit::BitMask(cDirtyState[dxStage].cbvMask))
          ctx->bindUniformBuffer(vkStage, cbvSlot + index, DxvkBufferSlice());

        // Unbind all dirty samplers
        auto samplerSlot = computeSamplerBinding(dxStage, 0);

        for (uint32_t index : bit::BitMask(cDirtyState[dxStage].samplerMask))
          ctx->bindResourceSampler(vkStage, samplerSlot + index, nullptr);

        // Unbind all dirty shader resource views
        auto srvSlot = computeSrvBinding(dxStage, 0);

        for (uint32_t m = 0; m < cDirtyState[dxStage].srvMask.size(); m++) {
          for (uint32_t index : bit::BitMask(cDirtyState[dxStage].srvMask[m]))
            ctx->bindResourceImageView(vkStage, srvSlot + index + m * 64u, nullptr);
        }

        // Unbind all dirty unordered access views
        VkShaderStageFlags uavStages = 0u;

        if (dxStage == DxbcProgramType::ComputeShader)
          uavStages = VK_SHADER_STAGE_COMPUTE_BIT;
        else if (dxStage == DxbcProgramType::PixelShader)
          uavStages = VK_SHADER_STAGE_ALL_GRAPHICS;

        if (uavStages) {
          auto uavSlot = computeUavBinding(dxStage, 0);
          auto ctrSlot = computeUavCounterBinding(dxStage, 0);

          for (uint32_t index : bit::BitMask(cDirtyState[dxStage].uavMask)) {
            ctx->bindResourceImageView(vkStage, uavSlot + index, nullptr);
            ctx->bindResourceBufferView(vkStage, ctrSlot + index, nullptr);
          }
        }
      }
    });

    // Since we set the DXVK context bindings to null, any bindings that are null
    // on the D3D context are no longer dirty, so we can clear the respective bits.
    for (uint32_t i = 0; i < uint32_t(DxbcProgramType::Count); i++) {
      auto stage = DxbcProgramType(i);

      for (uint32_t index : bit::BitMask(dirtyState[stage].cbvMask)) {
        if (!m_state.cbv[stage].buffers[index].buffer.ptr())
          dirtyState[stage].cbvMask &= ~(1u << index);
      }

      for (uint32_t index : bit::BitMask(dirtyState[stage].samplerMask)) {
        if (!m_state.samplers[stage].samplers[index])
          dirtyState[stage].samplerMask &= ~(1u << index);
      }

      for (uint32_t m = 0; m < dirtyState[stage].srvMask.size(); m++) {
        for (uint32_t index : bit::BitMask(dirtyState[stage].srvMask[m])) {
          if (!m_state.srv[stage].views[index + m * 64u].ptr())
            dirtyState[stage].srvMask[m] &= ~(uint64_t(1u) << index);
        }
      }

      if (stage == DxbcProgramType::ComputeShader || stage == DxbcProgramType::PixelShader) {
        auto& uavs = stage == DxbcProgramType::ComputeShader ? m_state.uav.views : m_state.om.uavs;

        for (uint32_t index : bit::BitMask(dirtyState[stage].uavMask)) {
          if (!uavs[index].ptr())
            dirtyState[stage].uavMask &= ~(uint64_t(1u) << index);
        }
      }

      if (dirtyState[stage].empty())
        m_state.lazy.shadersDirty.clr(stage);
    }
  }


  void D3D11ImmediateContext::ConsiderFlush(
          GpuFlushType                FlushType) {
    // In stress test mode, behave as if this would always flush
    if (DebugLazyBinding == Tristate::True)
      ApplyDirtyNullBindings();

    uint64_t chunkId = GetCurrentSequenceNumber();
    uint64_t submissionId = m_submissionFence->value();

    if (m_flushTracker.considerFlush(FlushType, chunkId, submissionId))
      ExecuteFlush(FlushType, nullptr, false);
  }


  void D3D11ImmediateContext::ExecuteFlush(
          GpuFlushType                FlushType,
          HANDLE                      hEvent,
          BOOL                        Synchronize) {
    bool synchronizeSubmission = Synchronize && m_parent->Is11on12Device();

    if (synchronizeSubmission)
      m_submitStatus.result = VK_NOT_READY;

    // Exit early if there's nothing to do
    if (!GetPendingCsChunks() && !hEvent)
      return;

    // Unbind unused resources
    ApplyDirtyNullBindings();

    // Signal the submission fence and flush the command list
    uint64_t submissionId = ++m_submissionId;

    if (hEvent) {
      m_submissionFence->setCallback(submissionId, [hEvent] {
        SetEvent(hEvent);
      });
    }

    EmitCs<false>([
      cSubmissionFence  = m_submissionFence,
      cSubmissionId     = submissionId,
      cSubmissionStatus = synchronizeSubmission ? &m_submitStatus : nullptr,
      cStagingFence     = m_stagingBufferFence,
      cStagingMemory    = GetStagingMemoryStatistics().allocatedTotal,
      cFlushReason      = std::exchange(m_flushReason, std::string())
    ] (DxvkContext* ctx) {
      auto debugLabel = vk::makeLabel(0xff5959, cFlushReason.c_str());

      ctx->signal(cSubmissionFence, cSubmissionId);
      ctx->signal(cStagingFence, cStagingMemory);
      ctx->flushCommandList(&debugLabel, cSubmissionStatus);
    });

    FlushCsChunk();

    // Notify flush tracker about the flush
    m_flushSeqNum = m_csSeqNum;
    m_flushTracker.notifyFlush(m_flushSeqNum, submissionId);

    // If necessary, block calling thread until the
    // Vulkan queue submission is performed.
    if (synchronizeSubmission)
      m_device->waitForSubmission(&m_submitStatus);

    // Free local staging buffer so that we don't
    // end up with a persistent allocation
    ResetStagingBuffer();

    // Reset counter for discarded memory in flight
    m_discardMemoryOnFlush = m_discardMemoryCounter;

    // Notify the device that the context has been flushed,
    // this resets some resource initialization heuristics.
    m_parent->NotifyContextFlush();

    // No point in tracking this across submissions
    m_hasPendingMsaaResolve = false;
  }


  void D3D11ImmediateContext::ThrottleAllocation() {
    DxvkStagingBufferStats stats = GetStagingMemoryStatistics();

    VkDeviceSize stagingMemoryInFlight = stats.allocatedTotal - m_stagingBufferFence->value();

    if (stagingMemoryInFlight > stats.allocatedSinceLastReset + D3D11Initializer::MaxMemoryInFlight) {
      // Stall calling thread to avoid situation where we keep growing the staging
      // buffer indefinitely, but ignore the newly allocated amount so that we don't
      // wait for the GPU to go fully idle in case of a large allocation.
      ExecuteFlush(GpuFlushType::ExplicitFlush, nullptr, false);

      m_device->waitForFence(*m_stagingBufferFence, stats.allocatedTotal -
        stats.allocatedSinceLastReset - D3D11Initializer::MaxMemoryInFlight);
    } else if (stats.allocatedSinceLastReset >= D3D11Initializer::MaxMemoryPerSubmission) {
      // Flush somewhat aggressively if there's a lot of memory in flight
      ExecuteFlush(GpuFlushType::ExplicitFlush, nullptr, false);
    }
  }


  void D3D11ImmediateContext::ThrottleDiscard(
          VkDeviceSize                Size) {
    m_discardMemoryCounter += Size;

    if (m_discardMemoryCounter - m_discardMemoryOnFlush >= D3D11Initializer::MaxMemoryPerSubmission)
      ThrottleAllocation();
  }


  DxvkStagingBufferStats D3D11ImmediateContext::GetStagingMemoryStatistics() {
    DxvkStagingBufferStats stats = m_staging.getStatistics();
    stats.allocatedTotal += m_discardMemoryCounter;
    stats.allocatedSinceLastReset += m_discardMemoryCounter - m_discardMemoryOnFlush;
    return stats;
  }


  GpuFlushType D3D11ImmediateContext::GetMaxFlushType(
          D3D11Device*    pParent,
    const Rc<DxvkDevice>& Device) {
    if (pParent->GetOptions()->reproducibleCommandStream)
      return GpuFlushType::ExplicitFlush;
    else if (Device->perfHints().preferRenderPassOps)
      return GpuFlushType::ImplicitMediumHint;
    else
      return GpuFlushType::ImplicitWeakHint;
  }

}
