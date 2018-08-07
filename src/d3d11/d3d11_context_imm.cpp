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
  : D3D11DeviceContext(pParent, Device),
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
  
  
  HRESULT STDMETHODCALLTYPE D3D11ImmediateContext::GetData(
          ID3D11Asynchronous*               pAsync,
          void*                             pData,
          UINT                              DataSize,
          UINT                              GetDataFlags) {
    // Make sure that we can safely write to the memory
    // location pointed to by pData if it is specified.
    if (DataSize == 0)
      pData = nullptr;
    
    if (pData != nullptr && pAsync->GetDataSize() != DataSize) {
      Logger::err(str::format(
        "D3D11: GetData: Data size mismatch",
        "\n  Expected: ", pAsync->GetDataSize(),
        "\n  Got:      ", DataSize));
      return E_INVALIDARG;
    }
    
    // Default error return for unsupported interfaces
    HRESULT hr = E_INVALIDARG;

    // This method can handle various incompatible interfaces,
    // so we have to find out what we are actually dealing with
    Com<ID3D11Query> query;
    
    if (SUCCEEDED(pAsync->QueryInterface(__uuidof(ID3D11Query), reinterpret_cast<void**>(&query))))
      hr = static_cast<D3D11Query*>(query.ptr())->GetData(pData, GetDataFlags);
    
    // If we're likely going to spin on the asynchronous object,
    // flush the context so that we're keeping the GPU busy
    if (hr == S_FALSE)
      FlushImplicit();
    
    // The requested interface is not supported
    if (FAILED(hr))
      Logger::err("D3D11: GetData: Unsupported Async type");
    
    return hr;
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::Flush() {
    m_parent->FlushInitContext();
    
    if (m_csIsBusy || m_csChunk->commandCount() != 0) {
      // Add commands to flush the threaded
      // context, then flush the command list
      EmitCs([dev = m_device] (DxvkContext* ctx) {
        dev->submitCommandList(
          ctx->endRecording(),
          nullptr, nullptr);
        
        ctx->beginRecording(
          dev->createCommandList());
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
    auto commandList = static_cast<D3D11CommandList*>(pCommandList);
    
    // Flush any outstanding commands so that
    // we don't mess up the execution order
    FlushCsChunk();
    
    // As an optimization, flush everything if the
    // number of pending draw calls is high enough.
    FlushImplicit();
    
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
    if (pResource == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    if (pMappedResource != nullptr) {
      pMappedResource->pData      = nullptr;
      pMappedResource->RowPitch   = 0;
      pMappedResource->DepthPitch = 0;
    }
    
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    if (resourceDim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      return MapBuffer(
        static_cast<D3D11Buffer*>(pResource),
        MapType, MapFlags, pMappedResource);
    } else {
      return MapImage(
        GetCommonTexture(pResource),
        Subresource, MapType, MapFlags,
        pMappedResource);
    }
  }
  
  
  void STDMETHODCALLTYPE D3D11ImmediateContext::Unmap(
          ID3D11Resource*             pResource,
          UINT                        Subresource) {
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
    FlushImplicit();

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
    FlushImplicit();

    D3D11DeviceContext::CopySubresourceRegion1(
      pDstResource, DstSubresource, DstX, DstY, DstZ,
      pSrcResource, SrcSubresource, pSrcBox, CopyFlags);
  }

  
  void STDMETHODCALLTYPE D3D11ImmediateContext::CopyResource(
          ID3D11Resource*                   pDstResource,
          ID3D11Resource*                   pSrcResource) {
    FlushImplicit();

    D3D11DeviceContext::CopyResource(
      pDstResource, pSrcResource);
  }

  
  void STDMETHODCALLTYPE D3D11ImmediateContext::GenerateMips(
          ID3D11ShaderResourceView*         pShaderResourceView) {
    FlushImplicit();

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
    FlushImplicit();

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
    FlushImplicit();

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
    FlushImplicit();

    D3D11DeviceContext::ResolveSubresource(
      pDstResource, DstSubresource,
      pSrcResource, SrcSubresource,
      Format);
  }


  void STDMETHODCALLTYPE D3D11ImmediateContext::OMSetRenderTargets(
          UINT                              NumViews,
          ID3D11RenderTargetView* const*    ppRenderTargetViews,
          ID3D11DepthStencilView*           pDepthStencilView) {
    FlushImplicit();
    
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
    FlushImplicit();

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
      auto physicalSlice = buffer->allocPhysicalSlice();
      pResource->SetMappedSlice(physicalSlice);
      
      EmitCs([
        cBuffer        = buffer,
        cPhysicalSlice = physicalSlice
      ] (DxvkContext* ctx) {
        ctx->invalidateBuffer(cBuffer, cPhysicalSlice);
      });
    } else if (MapType != D3D11_MAP_WRITE_NO_OVERWRITE) {
      if (!WaitForResource(buffer->resource(), MapFlags))
        return DXGI_ERROR_WAS_STILL_DRAWING;
    }
    
    // Use map pointer from previous map operation. This
    // way we don't have to synchronize with the CS thread
    // if the map mode is D3D11_MAP_WRITE_NO_OVERWRITE.
    const DxvkPhysicalBufferSlice physicalSlice = pResource->GetMappedSlice();
    
    pMappedResource->pData      = physicalSlice.mapPtr(0);
    pMappedResource->RowPitch   = physicalSlice.length();
    pMappedResource->DepthPitch = physicalSlice.length();
    return S_OK;
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
    
    if (formatInfo->aspectMask != VK_IMAGE_ASPECT_COLOR_BIT) {
      Logger::err("D3D11: Cannot map a depth-stencil texture");
      return E_INVALIDARG;
    }
    
    VkImageSubresource subresource =
      pResource->GetSubresourceFromIndex(
        formatInfo->aspectMask, Subresource);
    
    pResource->SetMappedSubresource(subresource);
    
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
    } else {
      const VkExtent3D levelExtent = mappedImage->mipLevelExtent(subresource.mipLevel);
      const VkExtent3D blockCount = util::computeBlockCount(levelExtent, formatInfo->blockSize);
      
      DxvkPhysicalBufferSlice physicalSlice;
      
      if (MapType == D3D11_MAP_WRITE_DISCARD) {
        // We do not have to preserve the contents of the
        // buffer if the entire image gets discarded.
        physicalSlice = mappedBuffer->allocPhysicalSlice();
        
        EmitCs([
          cImageBuffer   = mappedBuffer,
          cPhysicalSlice = physicalSlice
        ] (DxvkContext* ctx) {
          ctx->invalidateBuffer(cImageBuffer, cPhysicalSlice);
        });
      } else {
        // When using any map mode which requires the image contents
        // to be preserved, and if the GPU has write access to the
        // image, copy the current image contents into the buffer.
        const bool copyExistingData = pResource->Desc()->Usage == D3D11_USAGE_STAGING;
        
        if (copyExistingData) {
          const VkImageSubresourceLayers subresourceLayers = {
            subresource.aspectMask,
            subresource.mipLevel,
            subresource.arrayLayer, 1 };
          
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
        
        WaitForResource(mappedBuffer->resource(), 0);
        physicalSlice = mappedBuffer->slice();
      }
      
      // Set up map pointer. Data is tightly packed within the mapped buffer.
      pMappedResource->pData      = physicalSlice.mapPtr(0);
      pMappedResource->RowPitch   = formatInfo->elementSize * blockCount.width;
      pMappedResource->DepthPitch = formatInfo->elementSize * blockCount.width * blockCount.height;
      return S_OK;
    }
  }
  
  
  void D3D11ImmediateContext::UnmapImage(
          D3D11CommonTexture*         pResource,
          UINT                        Subresource) {
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
    FlushCsChunk();
    SynchronizeCsThread();
    
    if (Resource->isInUse()) {
      if (MapFlags & D3D11_MAP_FLAG_DO_NOT_WAIT) {
        // We don't have to wait, but misbehaving games may
        // still try to spin on `Map` until the resource is
        // idle, so we should flush pending commands
        FlushImplicit();
        return false;
      } else {
        // Make sure pending commands using the resource get
        // executed on the the GPU if we have to wait for it
        Flush();
        
        while (Resource->isInUse())
          dxvk::this_thread::yield();
      }
    }
    
    return true;
  }
  
  
  void D3D11ImmediateContext::EmitCsChunk(Rc<DxvkCsChunk>&& chunk) {
    m_csThread.dispatchChunk(std::move(chunk));
    m_csIsBusy = true;
  }


  void D3D11ImmediateContext::FlushImplicit() {
    // Flush only if the GPU is about to go idle, in
    // order to keep the number of submissions low.
    if (m_device->pendingSubmissions() <= MaxPendingSubmits) {
      auto now = std::chrono::high_resolution_clock::now();

      // Prevent flushing too often in short intervals.
      if (now - m_lastFlush >= std::chrono::microseconds(MinFlushIntervalUs))
        Flush();
    }
  }
  
}
