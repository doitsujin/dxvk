#include "d3d11_cmdlist.h"
#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_texture.h"

constexpr static uint32_t MinFlushIntervalUs = 1250;
constexpr static uint32_t MaxPendingSubmits  = 3;

namespace dxvk {
  
  D3D11ImmediateContext::D3D11ImmediateContext(
          D3D11Device*    pParent,
    const Rc<DxvkDevice>& Device)
  : D3D11DeviceContext(pParent, Device, DxvkCsChunkFlag::SingleUse),
    m_csThread(Device->createContext()) {
    EmitCs([cDevice = m_device] (DxvkContext* ctx) {
      ctx->beginRecording(cDevice->createCommandList());
    });
    
    ClearState();
  }
  
  
  D3D11ImmediateContext::~D3D11ImmediateContext() {
    Flush();
    SynchronizeCsThread();
    SynchronizeDevice();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11ImmediateContext::AddRef() {
    return m_parent->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11ImmediateContext::Release() {
    return m_parent->Release();
  }
  
  
  D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE D3D11ImmediateContext::GetType() {
    return D3D11_DEVICE_CONTEXT_IMMEDIATE;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11ImmediateContext::GetContextFlags() {
    return 0;
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::End(
          ID3D11Asynchronous*               pAsync) {
    D3D11DeviceContext::End(pAsync);

    if (pAsync) {
      D3D11_QUERY_DESC desc;
      static_cast<D3D11Query*>(pAsync)->GetDesc(&desc);
      
      if (desc.Query == D3D11_QUERY_EVENT)
        FlushImplicit(TRUE);
    }
  }


  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::GetData(
          ID3D11Asynchronous*               pAsync,
          void*                             pData,
          UINT                              DataSize,
          UINT                              GetDataFlags) {
    if (!pAsync)
      return E_INVALIDARG;
    
    // Make sure that we can safely write to the memory
    // location pointed to by pData if it is specified.
    if (DataSize == 0)
      pData = nullptr;
    
    if (pData && pAsync->GetDataSize() != DataSize) {
      Logger::err(str::format(
        "D3D11: GetData: Data size mismatch",
        "\n  Expected: ", pAsync->GetDataSize(),
        "\n  Got:      ", DataSize));
      return E_INVALIDARG;
    }
    
    // Get query status directly from the query object
    HRESULT hr = static_cast<D3D11Query*>(pAsync)->GetData(pData, GetDataFlags);
    
    // If we're likely going to spin on the asynchronous object,
    // flush the context so that we're keeping the GPU busy
    if (hr == S_FALSE)
      FlushImplicit(FALSE);
    
    return hr;
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::Flush() {
    m_parent->FlushInitContext();
    
    D3D10DeviceLock lock = LockContext();
    
    if (m_csIsBusy || m_csChunk->commandCount() != 0) {
      // Add commands to flush the threaded
      // context, then flush the command list
      EmitCs([] (DxvkContext* ctx) {
        ctx->flushCommandList();
      });
      
      FlushCsChunk();
      
      // Reset flush timer used for implicit flushes
      m_lastFlush = std::chrono::high_resolution_clock::now();
      m_csIsBusy  = false;
    }
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

    if (!pResource || !pMappedResource)
      return E_INVALIDARG;
    
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);

    HRESULT hr;
    
    if (resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      hr = MapBuffer(
        static_cast<D3D11Buffer*>(pResource),
        MapType, MapFlags, pMappedResource);
    } else {
      hr = MapImage(
        GetCommonTexture(pResource),
        Subresource, MapType, MapFlags,
        pMappedResource);
    }

    if (FAILED(hr)) {
      pMappedResource->pData      = nullptr;
      pMappedResource->RowPitch   = 0;
      pMappedResource->DepthPitch = 0;
    }

    return hr;
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::Unmap(
          ID3D11Resource*             pResource,
          UINT                        Subresource) {
    D3D10DeviceLock lock = LockContext();

    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    if (resourceDim != D3D11_RESOURCE_DIMENSION_BUFFER)
      UnmapImage(GetCommonTexture(pResource), Subresource);
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::CopySubresourceRegion(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
          UINT                              DstX,
          UINT                              DstY,
          UINT                              DstZ,
          ID3D11Resource*                   pSrcResource,
          UINT                              SrcSubresource,
    const D3D11_BOX*                        pSrcBox) {
    FlushImplicit(FALSE);

    D3D11DeviceContext::CopySubresourceRegion(
      pDstResource, DstSubresource, DstX, DstY, DstZ,
      pSrcResource, SrcSubresource, pSrcBox);
  }
  

  void STDMETHODCALLTYPE D3D11ImmediateContext::CopySubresourceRegion1(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
          UINT                              DstX,
          UINT                              DstY,
          UINT                              DstZ,
          ID3D11Resource*                   pSrcResource,
          UINT                              SrcSubresource,
    const D3D11_BOX*                        pSrcBox,
          UINT                              CopyFlags) {
    FlushImplicit(FALSE);

    D3D11DeviceContext::CopySubresourceRegion1(
      pDstResource, DstSubresource, DstX, DstY, DstZ,
      pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
  }

  
  void STDMETHODCALLTYPE D3D11ImmediateContext::CopyResource(
          ID3D11Resource*                   pDstResource,
          ID3D11Resource*                   pSrcResource) {
    FlushImplicit(FALSE);

    D3D11DeviceContext::CopyResource(
      pDstResource, pSrcResource);
  }

  
  void STDMETHODCALLTYPE D3D11ImmediateContext::GenerateMips(
          ID3D11ShaderResourceView*         pShaderResourceView) {
    FlushImplicit(FALSE);

    D3D11DeviceContext::GenerateMips(
      pShaderResourceView);
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
  

  void STDMETHODCALLTYPE D3D11ImmediateContext::ResolveSubresource(
          ID3D11Resource*                   pDstResource,
          UINT                              DstSubresource,
          ID3D11Resource*                   pSrcResource,
          UINT                              SrcSubresource,
          DXGI_FORMAT                       Format) {
    FlushImplicit(FALSE);

    D3D11DeviceContext::ResolveSubresource(
      pDstResource, DstSubresource,
      pSrcResource, SrcSubresource,
      Format);
  }


  void STDMETHODCALLTYPE D3D11ImmediateContext::OMSetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    FlushImplicit(FALSE);
    
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
    FlushImplicit(FALSE);

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
    Rc<DxvkBuffer> buffer = pResource->GetBuffer();

    if (!(buffer->memFlags() & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
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
        cBuffer      = std::move(buffer),
        cBufferSlice = std::move(physSlice)
      ] (DxvkContext* ctx) {
        ctx->invalidateBuffer(cBuffer, cBufferSlice);
      });

      return S_OK;
    } else {
      // Wait until the resource is no longer in use
      if (MapType != D3D11_MAP_WRITE_NO_OVERWRITE) {
        if (!WaitForResource(buffer, MapFlags))
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
    const Rc<DxvkBuffer> mappedBuffer = pResource->GetMappedBuffer();
    
    if (pResource->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_NONE) {
      Logger::err("D3D11: Cannot map a device-local image");
      return E_INVALIDARG;
    }
    
    auto formatInfo = imageFormatInfo(mappedImage->info().format);
    auto subresource = pResource->GetSubresourceFromIndex(
        formatInfo->aspectMask, Subresource);
    
    pResource->SetMappedSubresource(subresource, MapType);
    
    if (pResource->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      const VkImageType imageType = mappedImage->info().type;
      
      // Wait for the resource to become available
      if (!WaitForResource(mappedImage, MapFlags))
        return DXGI_ERROR_WAS_STILL_DRAWING;
      
      // Query the subresource's memory layout and hope that
      // the application respects the returned pitch values.
      VkSubresourceLayout layout  = mappedImage->querySubresourceLayout(subresource);
      pMappedResource->pData      = mappedImage->mapPtr(layout.offset);
      pMappedResource->RowPitch   = imageType >= VK_IMAGE_TYPE_2D ? layout.rowPitch   : layout.size;
      pMappedResource->DepthPitch = imageType >= VK_IMAGE_TYPE_3D ? layout.depthPitch : layout.size;
      return S_OK;
    } else if (formatInfo->aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      VkExtent3D levelExtent = mappedImage->mipLevelExtent(subresource.mipLevel);

      if (MapType != D3D11_MAP_READ) {
        Logger::err(str::format("D3D11: Map type ", MapType, " not supported for depth-stencil images"));
        return E_INVALIDARG;
      }

      // The actual Vulkan image format may differ
      // from the format requested by the application
      VkFormat packFormat = GetPackedDepthStencilFormat(pResource->Desc()->Format);
      auto packFormatInfo = imageFormatInfo(packFormat);

      // This is slow, but we have to dispatch a pack
      // operation and then immediately synchronize.
      EmitCs([
        cImageBuffer = mappedBuffer,
        cImage       = mappedImage,
        cSubresource = subresource,
        cFormat      = packFormat
      ] (DxvkContext* ctx) {
        auto layers = vk::makeSubresourceLayers(cSubresource);
        auto x = cImage->mipLevelExtent(cSubresource.mipLevel);

        VkOffset2D offset = { 0, 0 };
        VkExtent2D extent = { x.width, x.height };

        ctx->copyDepthStencilImageToPackedBuffer(
          cImageBuffer, 0, cImage, layers, offset, extent, cFormat);
      });

      WaitForResource(mappedBuffer, 0);

      DxvkBufferSliceHandle physSlice = mappedBuffer->getSliceHandle();
      pMappedResource->pData      = physSlice.mapPtr;
      pMappedResource->RowPitch   = packFormatInfo->elementSize * levelExtent.width;
      pMappedResource->DepthPitch = packFormatInfo->elementSize * levelExtent.width * levelExtent.height;
      return S_OK;
    } else {
      VkExtent3D levelExtent = mappedImage->mipLevelExtent(subresource.mipLevel);
      VkExtent3D blockCount = util::computeBlockCount(levelExtent, formatInfo->blockSize);
      
      DxvkBufferSliceHandle physSlice;
      
      if (MapType == D3D11_MAP_WRITE_DISCARD) {
        // We do not have to preserve the contents of the
        // buffer if the entire image gets discarded.
        physSlice = mappedBuffer->allocSlice();
        
        EmitCs([
          cImageBuffer = mappedBuffer,
          cBufferSlice = physSlice
        ] (DxvkContext* ctx) {
          ctx->invalidateBuffer(cImageBuffer, cBufferSlice);
        });
      } else {
        // When using any map mode which requires the image contents
        // to be preserved, and if the GPU has write access to the
        // image, copy the current image contents into the buffer.
        const bool copyExistingData = pResource->Desc()->Usage == D3D11_USAGE_STAGING;
        
        if (copyExistingData) {
          auto subresourceLayers = vk::makeSubresourceLayers(subresource);
          
          EmitCs([
            cImageBuffer  = mappedBuffer,
            cImage        = mappedImage,
            cSubresources = subresourceLayers,
            cLevelExtent  = levelExtent
          ] (DxvkContext* ctx) {
            ctx->copyImageToBuffer(
              cImageBuffer, 0, VkExtent2D { 0u, 0u },
              cImage, cSubresources, VkOffset3D { 0, 0, 0 },
              cLevelExtent);
          });
        }
        
        WaitForResource(mappedBuffer, 0);
        physSlice = mappedBuffer->getSliceHandle();
      }
      
      // Set up map pointer. Data is tightly packed within the mapped buffer.
      pMappedResource->pData      = physSlice.mapPtr;
      pMappedResource->RowPitch   = formatInfo->elementSize * blockCount.width;
      pMappedResource->DepthPitch = formatInfo->elementSize * blockCount.width * blockCount.height;
      return S_OK;
    }
  }
  
  
  void D3D11ImmediateContext::UnmapImage(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource) {
    if (pResource->GetMapType() == D3D11_MAP_READ)
      return;
    
    if (pResource->GetMapMode() == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER) {
      // Now that data has been written into the buffer,
      // we need to copy its contents into the image
      const Rc<DxvkImage>  mappedImage  = pResource->GetImage();
      const Rc<DxvkBuffer> mappedBuffer = pResource->GetMappedBuffer();
      
      VkImageSubresource subresource = pResource->GetMappedSubresource();
      
      VkExtent3D levelExtent = mappedImage
        ->mipLevelExtent(subresource.mipLevel);
      
      VkImageSubresourceLayers subresourceLayers = {
        subresource.aspectMask,
        subresource.mipLevel,
        subresource.arrayLayer, 1 };
      
      EmitCs([
        cSrcBuffer      = mappedBuffer,
        cDstImage       = mappedImage,
        cDstLayers      = subresourceLayers,
        cDstLevelExtent = levelExtent
      ] (DxvkContext* ctx) {
        ctx->copyBufferToImage(cDstImage, cDstLayers,
          VkOffset3D { 0, 0, 0 }, cDstLevelExtent,
          cSrcBuffer, 0, { 0u, 0u });
      });
    }
    
    pResource->ClearMappedSubresource();
  }
  
  
  void D3D11ImmediateContext::SynchronizeCsThread() {
    D3D10DeviceLock lock = LockContext();

    // Dispatch current chunk so that all commands
    // recorded prior to this function will be run
    FlushCsChunk();
    
    m_csThread.synchronize();
  }
  
  
  void D3D11ImmediateContext::SynchronizeDevice() {
    m_device->waitForIdle();
  }
  
  
  bool D3D11ImmediateContext::WaitForResource(
    const Rc<DxvkResource>&                 Resource,
          UINT                              MapFlags) {
    // Some games (e.g. The Witcher 3) do not work correctly
    // when a map fails with D3D11_MAP_FLAG_DO_NOT_WAIT set
    if (!m_parent->GetOptions()->allowMapFlagNoWait)
      MapFlags &= ~D3D11_MAP_FLAG_DO_NOT_WAIT;
    
    // Wait for the any pending D3D11 command to be executed
    // on the CS thread so that we can determine whether the
    // resource is currently in use or not.
    SynchronizeCsThread();
    
    if (Resource->isInUse()) {
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
        
        while (Resource->isInUse())
          dxvk::this_thread::yield();
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
    if (StrongHint || m_device->pendingSubmissions() <= MaxPendingSubmits) {
      auto now = std::chrono::high_resolution_clock::now();

      // Prevent flushing too often in short intervals.
      if (now - m_lastFlush >= std::chrono::microseconds(MinFlushIntervalUs))
        Flush();
    }
  }
  
}
