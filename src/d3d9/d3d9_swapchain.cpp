#include "d3d9_swapchain.h"
#include "d3d9_surface.h"
#include "d3d9_monitor.h"

#include "d3d9_hud.h"
#include "d3d9_window.h"

namespace dxvk {

  static uint16_t MapGammaControlPoint(float x) {
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return uint16_t(65535.0f * x);
  }

  D3D9SwapChainEx::D3D9SwapChainEx(
          D3D9DeviceEx*          pDevice,
          D3DPRESENT_PARAMETERS* pPresentParams,
    const D3DDISPLAYMODEEX*      pFullscreenDisplayMode,
          bool                   EnableLatencyTracking)
    : D3D9SwapChainExBase(pDevice)
    , m_device           (pDevice->GetDXVKDevice())
    , m_frameLatencyCap  (pDevice->GetOptions()->maxFrameLatency)
    , m_latencyTracking  (EnableLatencyTracking)
    , m_swapchainExt     (this) {
    this->NormalizePresentParameters(pPresentParams);
    m_presentParams = *pPresentParams;
    m_window = m_presentParams.hDeviceWindow;

    UpdateWindowCtx();

    UpdatePresentRegion(nullptr, nullptr);

    if (FAILED(CreateBackBuffers(m_presentParams.BackBufferCount, m_presentParams.Flags)))
      throw DxvkError("D3D9: Failed to create swapchain backbuffers");

    CreateBlitter();

    InitRamp();

    // Apply initial window mode and fullscreen state
    if (!m_presentParams.Windowed && FAILED(EnterFullscreenMode(pPresentParams, pFullscreenDisplayMode)))
      throw DxvkError("D3D9: Failed to set initial fullscreen state");
  }


  D3D9SwapChainEx::~D3D9SwapChainEx() {
    // Avoids hanging when in this state, see comment
    // in DxvkDevice::~DxvkDevice.
    if (this_thread::isInModuleDetachment())
      return;

    {
      // Locking here and in Device::GetFrontBufferData
      // ensures that other threads don't accidentally access a stale pointer.
      D3D9DeviceLock lock = m_parent->LockDevice();

      if (m_parent->GetMostRecentlyUsedSwapchain() == this) {
        m_parent->ResetMostRecentlyUsedSwapchain();
      }
    }

    DestroyBackBuffers();

    ResetWindowProc(m_window);
    RestoreDisplayMode(m_monitor);

    for (auto& p : m_presenters) {
      if (p.second.presenter) {
        p.second.presenter->destroyResources();
        p.second.presenter = nullptr;
      }
    }

    m_parent->DecrementLosableCounter();
  }


  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DSwapChain9)
     || (GetParent()->IsExtended() && riid == __uuidof(IDirect3DSwapChain9Ex))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3D9VkExtSwapchain)) {
      *ppvObject = ref(&m_swapchainExt);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDirect3DSwapChain9), riid)) {
      Logger::warn("D3D9SwapChainEx::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::Present(
    const RECT*    pSourceRect,
    const RECT*    pDestRect,
          HWND     hDestWindowOverride,
    const RGNDATA* pDirtyRegion,
          DWORD    dwFlags) {
    D3D9DeviceLock lock = m_parent->LockDevice();

    m_parent->SetMostRecentlyUsedSwapchain(this);

    if (unlikely(m_parent->IsDeviceLost()))
      return D3DERR_DEVICELOST;

    // If we have no backbuffers, error out.
    // This handles the case where a ::Reset failed due to OOM
    // or whatever.
    // I am not sure what the actual HRESULT returned here is
    // or should be, but it is better than crashing... probably!
    if (m_backBuffers.empty())
      return D3D_OK;

    uint32_t presentInterval = m_presentParams.PresentationInterval;

    // This is not true directly in d3d9 to to timing differences that don't matter for us.
    // For our purposes...
    // D3DPRESENT_INTERVAL_DEFAULT (0) == D3DPRESENT_INTERVAL_ONE (1) which means VSYNC.
    presentInterval = std::max(presentInterval, 1u);

    if (presentInterval == D3DPRESENT_INTERVAL_IMMEDIATE || (dwFlags & D3DPRESENT_FORCEIMMEDIATE))
      presentInterval = 0;

    auto options = m_parent->GetOptions();

    if (options->presentInterval >= 0)
      presentInterval = options->presentInterval;

    HWND window = m_presentParams.hDeviceWindow;

    if (hDestWindowOverride != nullptr)
      window = hDestWindowOverride;

    if (m_window != window) {
      m_window = window;
      m_displayRefreshRateDirty = true;
    }

    if (!UpdateWindowCtx())
      return D3D_OK;

    if (options->deferSurfaceCreation && IsDeviceReset(m_wctx))
      m_wctx->presenter->invalidateSurface();

    m_wctx->presenter->setSyncInterval(presentInterval);

    UpdatePresentRegion(pSourceRect, pDestRect);
    UpdatePresentParameters();

    if (!SwapWithFrontBuffer() && m_parent->GetOptions()->extraFrontbuffer) {
      // We never actually rotate in the front buffer.
      // Just blit to it for GetFrontBufferData.

      // When we have multiple buffers, the last buffer always acts as the front buffer.
      // (See comment in PresentImage for an explaination why.)
      // Games with a buffer count of 1 rely on the contents of the previous frame still
      // being there, so we can't just add another buffer to the rotation.
      // At the same time, they could call GetFrontBufferData after already rendering to the backbuffer.
      // So we have to do a copy of the backbuffer that will be copied to the Vulkan backbuffer
      // and keep that around for the next frame.

      const auto& backbuffer = m_backBuffers[0];
      const auto& frontbuffer = GetFrontBuffer();
      if (FAILED(m_parent->StretchRect(backbuffer.ptr(), nullptr, frontbuffer.ptr(), nullptr, D3DTEXF_NONE))) {
        Logger::err("Failed to blit to front buffer");
      }
    }

#ifdef _WIN32
    const bool useGDIFallback = m_partialCopy && !SwapWithFrontBuffer();
    if (useGDIFallback)
      return PresentImageGDI(m_window);
#endif

    try {
      UpdateWindowedRefreshRate();
      UpdateTargetFrameRate(presentInterval);
      PresentImage(presentInterval);
      return D3D_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
#ifdef _WIN32
      return PresentImageGDI(m_window);
#else
      return D3DERR_DEVICEREMOVED;
#endif
    }
  }

#ifdef _WIN32
  #define DCX_USESTYLE 0x00010000

  HRESULT D3D9SwapChainEx::PresentImageGDI(HWND Window) {
    m_parent->EndFrame(nullptr);
    m_parent->Flush();

    if (!std::exchange(m_warnedAboutGDIFallback, true))
      Logger::warn("Using GDI for swapchain presentation. This will impact performance.");

    HDC hDC;
    HRESULT result = m_backBuffers[0]->GetDC(&hDC);
    if (result) {
      Logger::err("D3D9SwapChainEx::BlitGDI Surface GetDC failed");
      return D3DERR_DEVICEREMOVED;
    }

    HDC dstDC = GetDCEx(Window, 0, DCX_CACHE | DCX_USESTYLE);
    if (!dstDC) {
      Logger::err("D3D9SwapChainEx::BlitGDI: GetDCEx failed");
      m_backBuffers[0]->ReleaseDC(hDC);
      return D3DERR_DEVICEREMOVED;
    }

    bool success = StretchBlt(dstDC, m_dstRect.left, m_dstRect.top, m_dstRect.right - m_dstRect.left,
            m_dstRect.bottom - m_dstRect.top, hDC, m_srcRect.left, m_srcRect.top,
            m_srcRect.right - m_srcRect.left, m_srcRect.bottom - m_srcRect.top, SRCCOPY);

    m_backBuffers[0]->ReleaseDC(hDC);
    ReleaseDC(Window, dstDC);

    if (!success) {
      Logger::err("D3D9SwapChainEx::BlitGDI: StretchBlt failed");
      return D3DERR_DEVICEREMOVED;
    }

    return S_OK;
  }
#endif

  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetFrontBufferData(IDirect3DSurface9* pDestSurface) {
    D3D9DeviceLock lock = m_parent->LockDevice();

    // This function can do absolutely everything!
    // Copies the front buffer between formats with an implicit resolve.
    // Oh, and the dest is systemmem...
    // This is a slow function anyway, it waits for the copy to finish.
    // so there's no reason to not just make and throwaway temp images.

    // If extent of dst > src, then we blit to a subrect of the size
    // of src onto a temp image of dst's extents,
    // then copy buffer back to dst (given dst is subresource)

    // For SWAPEFFECT_COPY and windowed SWAPEFFECT_DISCARD with 1 backbuffer, we just copy the backbuffer data instead.
    // We just copy from the backbuffer instead of the front buffer to avoid having to do another blit.
    // This mostly impacts windowed mode and our implementation was not accurate in that case anyway as Windows D3D9
    // takes a screenshot of the entire screen.

    D3D9Surface* dst = static_cast<D3D9Surface*>(pDestSurface);

    if (unlikely(dst == nullptr))
      return D3DERR_INVALIDCALL;

    D3D9CommonTexture* dstTexInfo = dst->GetCommonTexture();
    D3D9CommonTexture* srcTexInfo = GetFrontBuffer()->GetCommonTexture();

    if (unlikely(dstTexInfo->Desc()->Pool != D3DPOOL_SYSTEMMEM && dstTexInfo->Desc()->Pool != D3DPOOL_SCRATCH))
      return D3DERR_INVALIDCALL;
    
    if (unlikely(m_parent->IsDeviceLost())) {
      return D3DERR_DEVICELOST;
    }

    VkExtent3D dstTexExtent = dstTexInfo->GetExtentMip(dst->GetMipLevel());
    VkExtent3D srcTexExtent = srcTexInfo->GetExtentMip(0);

    const bool clearDst = dstTexInfo->Desc()->MipLevels > 1
                       || dstTexExtent.width > srcTexExtent.width
                       || dstTexExtent.height > srcTexExtent.height;

    dstTexInfo->CreateBuffer(clearDst);
    DxvkBufferSlice dstBufferSlice = dstTexInfo->GetBufferSlice(dst->GetSubresource());
    Rc<DxvkImage>   srcImage       = srcTexInfo->GetImage();

    if (srcImage->info().sampleCount != VK_SAMPLE_COUNT_1_BIT) {
      DxvkImageCreateInfo resolveInfo;
      resolveInfo.type          = VK_IMAGE_TYPE_2D;
      resolveInfo.format        = srcImage->info().format;
      resolveInfo.flags         = 0;
      resolveInfo.sampleCount   = VK_SAMPLE_COUNT_1_BIT;
      resolveInfo.extent        = srcImage->info().extent;
      resolveInfo.numLayers     = 1;
      resolveInfo.mipLevels     = 1;
      resolveInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT
                                | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      resolveInfo.stages        = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                | VK_PIPELINE_STAGE_TRANSFER_BIT;
      resolveInfo.access        = VK_ACCESS_SHADER_READ_BIT
                                | VK_ACCESS_TRANSFER_WRITE_BIT
                                | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      resolveInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      resolveInfo.layout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      
      Rc<DxvkImage> resolvedSrc = m_device->createImage(
        resolveInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      m_parent->EmitCs([
        cDstImage = resolvedSrc,
        cSrcImage = srcImage
      ] (DxvkContext* ctx) {
        VkImageSubresourceLayers resolveSubresource;
        resolveSubresource.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        resolveSubresource.mipLevel        = 0;
        resolveSubresource.baseArrayLayer  = 0;
        resolveSubresource.layerCount      = 1;

        VkImageResolve resolveRegion;
        resolveRegion.srcSubresource = resolveSubresource;
        resolveRegion.srcOffset      = VkOffset3D { 0, 0, 0 };
        resolveRegion.dstSubresource = resolveSubresource;
        resolveRegion.dstOffset      = VkOffset3D { 0, 0, 0 };
        resolveRegion.extent         = cSrcImage->info().extent;

        ctx->resolveImage(cDstImage, cSrcImage, resolveRegion,
          cSrcImage->info().format, VK_RESOLVE_MODE_AVERAGE_BIT, VK_RESOLVE_MODE_NONE);
      });

      srcImage = std::move(resolvedSrc);
    }

    D3D9Format srcFormat = srcTexInfo->Desc()->Format;
    D3D9Format dstFormat = dstTexInfo->Desc()->Format;

    bool similar = AreFormatsSimilar(srcFormat, dstFormat);

    if (!similar || srcImage->info().extent != dstTexInfo->GetExtent()) {
      DxvkImageCreateInfo blitCreateInfo;
      blitCreateInfo.type          = VK_IMAGE_TYPE_2D;
      blitCreateInfo.format        = dstTexInfo->GetFormatMapping().FormatColor;
      blitCreateInfo.flags         = 0;
      blitCreateInfo.sampleCount   = VK_SAMPLE_COUNT_1_BIT;
      blitCreateInfo.extent        = dstTexInfo->GetExtent();
      blitCreateInfo.numLayers     = 1;
      blitCreateInfo.mipLevels     = 1;
      blitCreateInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT
                                   | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                   | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                   | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      blitCreateInfo.stages        = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                   | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                   | VK_PIPELINE_STAGE_TRANSFER_BIT;
      blitCreateInfo.access        = VK_ACCESS_SHADER_READ_BIT
                                   | VK_ACCESS_TRANSFER_WRITE_BIT
                                   | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                   | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      blitCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      blitCreateInfo.layout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      
      Rc<DxvkImage> blittedSrc = m_device->createImage(
        blitCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      const DxvkFormatInfo* dstFormatInfo = lookupFormatInfo(blittedSrc->info().format);
      const DxvkFormatInfo* srcFormatInfo = lookupFormatInfo(srcImage->info().format);

      const VkImageSubresource dstSubresource = dstTexInfo->GetSubresourceFromIndex(dstFormatInfo->aspectMask, 0);
      const VkImageSubresource srcSubresource = srcTexInfo->GetSubresourceFromIndex(srcFormatInfo->aspectMask, 0);

      VkImageSubresourceLayers dstSubresourceLayers = {
        dstSubresource.aspectMask,
        dstSubresource.mipLevel,
        dstSubresource.arrayLayer, 1 };

      VkImageSubresourceLayers srcSubresourceLayers = {
        srcSubresource.aspectMask,
        srcSubresource.mipLevel,
        srcSubresource.arrayLayer, 1 };

      VkExtent3D srcExtent = srcImage->mipLevelExtent(srcSubresource.mipLevel);

      // Blit to a subrect of the src extents
      VkImageBlit blitInfo;
      blitInfo.dstSubresource = dstSubresourceLayers;
      blitInfo.srcSubresource = srcSubresourceLayers;
      blitInfo.dstOffsets[0] = VkOffset3D{ 0, 0, 0 };
      blitInfo.dstOffsets[1] = VkOffset3D{ int32_t(srcExtent.width),  int32_t(srcExtent.height),  1 };
      blitInfo.srcOffsets[0] = VkOffset3D{ 0, 0, 0 };
      blitInfo.srcOffsets[1] = VkOffset3D{ int32_t(srcExtent.width),  int32_t(srcExtent.height),  1 };

#ifdef _WIN32
      if (m_presentParams.Windowed) {
        // In windowed mode, GetFrontBufferData takes a screenshot of the entire screen.
        // So place the copy of the front buffer at the position of the window.
        POINT point = { 0, 0 };
        if (ClientToScreen(m_window, &point) != 0) {
          blitInfo.dstOffsets[0].x = point.x;
          blitInfo.dstOffsets[0].y = point.y;
          blitInfo.dstOffsets[1].x += point.x;
          blitInfo.dstOffsets[1].y += point.y;
        }
      }
#endif

      DxvkImageViewKey dstViewInfo;
      dstViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      dstViewInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      dstViewInfo.format = blittedSrc->info().format;
      dstViewInfo.aspects = blitInfo.dstSubresource.aspectMask;
      dstViewInfo.mipIndex = blitInfo.dstSubresource.mipLevel;
      dstViewInfo.mipCount = 1;
      dstViewInfo.layerIndex = blitInfo.dstSubresource.baseArrayLayer;
      dstViewInfo.layerCount = blitInfo.dstSubresource.layerCount;
      dstViewInfo.packedSwizzle = DxvkImageViewKey::packSwizzle(dstTexInfo->GetMapping().Swizzle);

      DxvkImageViewKey srcViewInfo;
      srcViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      srcViewInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      srcViewInfo.format = srcImage->info().format;
      srcViewInfo.aspects = blitInfo.srcSubresource.aspectMask;
      srcViewInfo.mipIndex = blitInfo.srcSubresource.mipLevel;
      srcViewInfo.mipCount = 1;
      srcViewInfo.layerIndex = blitInfo.srcSubresource.baseArrayLayer;
      srcViewInfo.layerCount = blitInfo.srcSubresource.layerCount;
      srcViewInfo.packedSwizzle = DxvkImageViewKey::packSwizzle(srcTexInfo->GetMapping().Swizzle);

      m_parent->EmitCs([
        cDstView  = blittedSrc->createView(dstViewInfo),
        cSrcView  = srcImage->createView(srcViewInfo),
        cBlitInfo = blitInfo
      ] (DxvkContext* ctx) {
        ctx->blitImageView(
          cDstView, cBlitInfo.dstOffsets,
          cSrcView, cBlitInfo.srcOffsets,
          VK_FILTER_NEAREST);
      });

      srcImage = std::move(blittedSrc);
    }

    const DxvkFormatInfo* srcFormatInfo = lookupFormatInfo(srcImage->info().format);
    const VkImageSubresource srcSubresource = srcTexInfo->GetSubresourceFromIndex(srcFormatInfo->aspectMask, 0);
    VkImageSubresourceLayers srcSubresourceLayers = {
      srcSubresource.aspectMask,
      srcSubresource.mipLevel,
      srcSubresource.arrayLayer, 1 };
    VkExtent3D srcExtent = srcImage->mipLevelExtent(srcSubresource.mipLevel);

    m_parent->EmitCs([
      cBufferSlice  = std::move(dstBufferSlice),
      cImage        = std::move(srcImage),
      cSubresources = srcSubresourceLayers,
      cLevelExtent  = srcExtent
    ] (DxvkContext* ctx) {
      ctx->copyImageToBuffer(cBufferSlice.buffer(),
        cBufferSlice.offset(), 4, 0, VK_FORMAT_UNDEFINED,
        cImage, cSubresources, VkOffset3D { 0, 0, 0 },
        cLevelExtent);
    });

    dstTexInfo->SetNeedsReadback(dst->GetSubresource(), true);
    m_parent->TrackTextureMappingBufferSequenceNumber(dstTexInfo, dst->GetSubresource());

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetBackBuffer(
          UINT                iBackBuffer,
          D3DBACKBUFFER_TYPE  Type,
          IDirect3DSurface9** ppBackBuffer) {
    // Could be doing a device reset...
    D3D9DeviceLock lock = m_parent->LockDevice();

    if (unlikely(ppBackBuffer == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(iBackBuffer >= m_presentParams.BackBufferCount)) {
      Logger::err(str::format("D3D9: GetBackBuffer: Invalid back buffer index: ", iBackBuffer));
      return D3DERR_INVALIDCALL;
    }

    if (m_backBuffers.empty()) {
      // The backbuffers were destroyed and not recreated.
      // This can happen when a call to Reset fails.
      *ppBackBuffer = nullptr;
      return D3D_OK;
    }

    *ppBackBuffer = ref(m_backBuffers[iBackBuffer].ptr());
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetRasterStatus(D3DRASTER_STATUS* pRasterStatus) {
    // We could use D3DKMTGetScanLine but Wine doesn't implement that.
    // So... we lie here and make some stuff up
    // enough that it makes games work.

    // Assume there's 20 lines in a vBlank.
    constexpr uint32_t vBlankLineCount = 20;

    if (pRasterStatus == nullptr)
      return D3DERR_INVALIDCALL;

    D3DDISPLAYMODEEX mode;
    mode.Size = sizeof(mode);
    if (FAILED(this->GetDisplayModeEx(&mode, nullptr)))
      return D3DERR_INVALIDCALL;

    uint32_t scanLineCount = mode.Height + vBlankLineCount;

    auto nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(
      dxvk::high_resolution_clock::now())
      .time_since_epoch();

    auto frametimeUs = std::chrono::microseconds(1000000u / mode.RefreshRate);
    auto scanLineUs  = frametimeUs / scanLineCount;

    pRasterStatus->ScanLine = (nowUs % frametimeUs) / scanLineUs;
    pRasterStatus->InVBlank = pRasterStatus->ScanLine >= mode.Height;

    if (pRasterStatus->InVBlank)
      pRasterStatus->ScanLine = 0;

    return D3D_OK;
  }

  
  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetDisplayMode(D3DDISPLAYMODE* pMode) {
    if (pMode == nullptr)
      return D3DERR_INVALIDCALL;

    *pMode = D3DDISPLAYMODE();

    D3DDISPLAYMODEEX mode;
    mode.Size = sizeof(mode);
    HRESULT hr = this->GetDisplayModeEx(&mode, nullptr);

    if (FAILED(hr))
      return hr;

    pMode->Width       = mode.Width;
    pMode->Height      = mode.Height;
    pMode->Format      = mode.Format;
    pMode->RefreshRate = mode.RefreshRate;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    if (pPresentationParameters == nullptr)
      return D3DERR_INVALIDCALL;

    *pPresentationParameters = m_presentParams;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetLastPresentCount(UINT* pLastPresentCount) {
    Logger::warn("D3D9SwapChainEx::GetLastPresentCount: Stub");
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetPresentStats(D3DPRESENTSTATS* pPresentationStatistics) {
    Logger::warn("D3D9SwapChainEx::GetPresentStats: Stub");
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9SwapChainEx::GetDisplayModeEx(D3DDISPLAYMODEEX* pMode, D3DDISPLAYROTATION* pRotation) {
    if (pMode == nullptr && pRotation == nullptr)
      return D3DERR_INVALIDCALL;

    if (pRotation != nullptr)
      *pRotation = D3DDISPLAYROTATION_IDENTITY;

    if (pMode != nullptr) {
      wsi::WsiMode devMode = { };

      if (!wsi::getCurrentDisplayMode(wsi::getDefaultMonitor(), &devMode)) {
        Logger::err("D3D9SwapChainEx::GetDisplayModeEx: Failed to enum display settings");
        return D3DERR_INVALIDCALL;
      }

      *pMode = ConvertDisplayMode(devMode);
    }

    return D3D_OK;
  }


  HRESULT D3D9SwapChainEx::Reset(
          D3DPRESENT_PARAMETERS* pPresentParams,
          D3DDISPLAYMODEEX*      pFullscreenDisplayMode) {
    D3D9DeviceLock lock = m_parent->LockDevice();

    HRESULT hr = D3D_OK;

    this->NormalizePresentParameters(pPresentParams);

    bool changeFullscreen = m_presentParams.Windowed != pPresentParams->Windowed;

    if (pPresentParams->Windowed) {
      if (changeFullscreen)
        this->LeaveFullscreenMode();
    }
    else {
      m_parent->NotifyFullscreen(m_window, true);

      if (changeFullscreen) {
        hr = this->EnterFullscreenMode(pPresentParams, pFullscreenDisplayMode);
        if (FAILED(hr))
          return hr;
      }

      D3D9WindowMessageFilter filter(m_window);

      if (!changeFullscreen) {
        hr = ChangeDisplayMode(pPresentParams, pFullscreenDisplayMode);
        if (FAILED(hr))
          return hr;

        wsi::updateFullscreenWindow(m_monitor, m_window, true);
      }
    }

    m_presentParams = *pPresentParams;

    if (changeFullscreen)
      SetGammaRamp(0, &m_ramp);

    UpdatePresentParameters();

    hr = CreateBackBuffers(m_presentParams.BackBufferCount, m_presentParams.Flags);
    if (FAILED(hr))
      return hr;

    return D3D_OK;
  }


  HRESULT D3D9SwapChainEx::WaitForVBlank() {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D9SwapChainEx::WaitForVBlank: Stub");

    return D3D_OK;
  }

  static bool validateGammaRamp(const WORD (&ramp)[256]) {
    if (ramp[0] >= ramp[std::size(ramp) - 1]) {
      Logger::warn("validateGammaRamp: ramp inverted or flat");
      return false;
    }

    for (size_t i = 1; i < std::size(ramp); i++) {
      if (ramp[i] < ramp[i - 1]) {
        Logger::warn("validateGammaRamp: ramp not monotonically increasing");
        return false;
      }
      if (ramp[i] - ramp[i - 1] >= UINT16_MAX / 2) {
        Logger::warn("validateGammaRamp: huuuge jump");
        return false;
      }
    }

    return true;
  }


  void    D3D9SwapChainEx::SetGammaRamp(
            DWORD         Flags,
      const D3DGAMMARAMP* pRamp) {
    D3D9DeviceLock lock = m_parent->LockDevice();

    if (unlikely(pRamp == nullptr))
      return;

    if (unlikely(!validateGammaRamp(pRamp->red)
              && !validateGammaRamp(pRamp->blue)
              && !validateGammaRamp(pRamp->green)))
      return;

    m_ramp = *pRamp;

    bool isIdentity = true;

    std::array<DxvkGammaCp, NumControlPoints> cp;
      
    for (uint32_t i = 0; i < NumControlPoints; i++) {
      uint16_t identity = MapGammaControlPoint(float(i) / float(NumControlPoints - 1));

      cp[i].r = pRamp->red[i];
      cp[i].g = pRamp->green[i];
      cp[i].b = pRamp->blue[i];
      cp[i].a = 0;

      isIdentity &= cp[i].r == identity
                 && cp[i].g == identity
                 && cp[i].b == identity;
    }

    if (!isIdentity && !m_presentParams.Windowed)
      m_blitter->setGammaRamp(NumControlPoints, cp.data());
    else
      m_blitter->setGammaRamp(0, nullptr);
  }


  void    D3D9SwapChainEx::GetGammaRamp(D3DGAMMARAMP* pRamp) {
    D3D9DeviceLock lock = m_parent->LockDevice();

    if (likely(pRamp != nullptr))
      *pRamp = m_ramp;
  }


  void    D3D9SwapChainEx::Invalidate(HWND hWindow) {
    if (!hWindow)
      hWindow = m_parent->GetWindow();

    auto entry = m_presenters.find(hWindow);

    if (entry != m_presenters.end()) {
      if (entry->second.presenter) {
        entry->second.presenter->destroyResources();
        entry->second.presenter = nullptr;

        if (m_presentParams.hDeviceWindow == hWindow)
          DestroyLatencyTracker();
      }

      if (m_wctx == &entry->second)
        m_wctx = nullptr;

      m_presenters.erase(entry);
    }
  }


  void D3D9SwapChainEx::SetCursorTexture(UINT Width, UINT Height, uint8_t* pCursorBitmap) {
      VkExtent2D cursorSize = { uint32_t(Width), uint32_t(Height) };

      m_blitter->setCursorTexture(
        cursorSize,
        VK_FORMAT_B8G8R8A8_SRGB,
        reinterpret_cast<void*>(pCursorBitmap));
  }


  void D3D9SwapChainEx::SetCursorPosition(int32_t X, int32_t Y, UINT Width, UINT Height) {
      VkOffset2D cursorPosition = { X, Y };
      VkExtent2D cursorSize     = { uint32_t(Width), uint32_t(Height) };

      VkRect2D   cursorRect     = { cursorPosition, cursorSize };

      m_parent->EmitCs([
        cBlitter = m_blitter,
        cRect    = cursorRect
      ] (DxvkContext* ctx) {
        cBlitter->setCursorPos(
          cRect);
      });
  }


  HRESULT D3D9SwapChainEx::SetDialogBoxMode(bool bEnableDialogs) {
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setdialogboxmode
    // The MSDN documentation says this will error out under many weird conditions.
    // However it doesn't appear to error at all in any of my tests of these
    // cases described in the documentation.
    return D3D_OK;
  }


  D3D9Surface* D3D9SwapChainEx::GetBackBuffer(UINT iBackBuffer) {
    if (iBackBuffer >= m_presentParams.BackBufferCount)
      return nullptr;

    return m_backBuffers[iBackBuffer].ptr();
  }


  void D3D9SwapChainEx::NormalizePresentParameters(D3DPRESENT_PARAMETERS* pPresentParams) {
    if (pPresentParams->hDeviceWindow == nullptr)
      pPresentParams->hDeviceWindow    = m_parent->GetWindow();

    pPresentParams->BackBufferCount    = std::max(pPresentParams->BackBufferCount, 1u);

    const int32_t forcedMSAA = m_parent->GetOptions()->forceSwapchainMSAA;
    if (forcedMSAA != -1) {
      pPresentParams->MultiSampleType    = D3DMULTISAMPLE_TYPE(forcedMSAA);
      pPresentParams->MultiSampleQuality = 0;
    }

    if (pPresentParams->Windowed) {
      wsi::getWindowSize(pPresentParams->hDeviceWindow,
        pPresentParams->BackBufferWidth  ? nullptr : &pPresentParams->BackBufferWidth,
        pPresentParams->BackBufferHeight ? nullptr : &pPresentParams->BackBufferHeight);
    }
    else {
      wsi::getMonitorClientSize(wsi::getDefaultMonitor(),
        pPresentParams->BackBufferWidth  ? nullptr : &pPresentParams->BackBufferWidth,
        pPresentParams->BackBufferHeight ? nullptr : &pPresentParams->BackBufferHeight);
    }

    if (pPresentParams->BackBufferFormat == D3DFMT_UNKNOWN)
      pPresentParams->BackBufferFormat = D3DFMT_X8R8G8B8;

    if (env::getEnvVar("DXVK_FORCE_WINDOWED") == "1")
      pPresentParams->Windowed         = TRUE;
  }


  void D3D9SwapChainEx::PresentImage(UINT SyncInterval) {
    m_parent->EndFrame(m_latencyTracker);
    m_parent->Flush();

    if (m_latencyTracker)
      m_latencyTracker->notifyCpuPresentBegin(m_wctx->frameId + 1u);

    // Retrieve the image and image view to present
    VkResult status = VK_SUCCESS;

    Rc<DxvkImage> swapImage = m_backBuffers[0]->GetCommonTexture()->GetImage();
    Rc<DxvkImageView> swapImageView = m_backBuffers[0]->GetImageView(false);

    // Presentation semaphores and WSI swap chain image
    PresenterSync sync = { };
    Rc<DxvkImage> backBuffer;

    status = m_wctx->presenter->acquireNextImage(sync, backBuffer);

    if (status >= 0 && status != VK_NOT_READY) {
      VkRect2D srcRect = {
        {  int32_t(m_srcRect.left),                    int32_t(m_srcRect.top)                    },
        { uint32_t(m_srcRect.right - m_srcRect.left), uint32_t(m_srcRect.bottom - m_srcRect.top) } };

      VkRect2D dstRect = {
        {  int32_t(m_dstRect.left),                    int32_t(m_dstRect.top)                    },
        { uint32_t(m_dstRect.right - m_dstRect.left), uint32_t(m_dstRect.bottom - m_dstRect.top) } };

      // Bump frame ID
      m_wctx->frameId += 1;

      // Present from CS thread so that we don't
      // have to synchronize with it first.
      DxvkImageViewKey viewInfo;
      viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.usage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      viewInfo.format     = backBuffer->info().format;
      viewInfo.aspects    = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.mipIndex   = 0u;
      viewInfo.mipCount   = 1u;
      viewInfo.layerIndex = 0u;
      viewInfo.layerCount = 1u;

      m_parent->EmitCs([
        cDevice         = m_device,
        cPresenter      = m_wctx->presenter,
        cBlitter        = m_blitter,
        cColorSpace     = m_colorspace,
        cSrcView        = swapImageView,
        cSrcRect        = srcRect,
        cDstView        = backBuffer->createView(viewInfo),
        cDstRect        = dstRect,
        cSync           = sync,
        cFrameId        = m_wctx->frameId,
        cLatency        = m_latencyTracker
      ] (DxvkContext* ctx) {
        // Update back buffer color space as necessary
        if (cSrcView->image()->info().colorSpace != cColorSpace) {
          DxvkImageUsageInfo usage = { };
          usage.colorSpace = cColorSpace;

          ctx->ensureImageCompatibility(cSrcView->image(), usage);
        }

        // Blit back buffer onto Vulkan swap chain
        auto contextObjects = ctx->beginExternalRendering();

        cBlitter->present(contextObjects,
          cDstView, cDstRect, cSrcView, cSrcRect);

        // Submit command list and present
        ctx->synchronizeWsi(cSync);
        ctx->flushCommandList(nullptr, nullptr);

        cDevice->presentImage(cPresenter, cLatency, cFrameId, nullptr);
      });

      m_parent->FlushCsChunk();
    }

    if (m_latencyTracker) {
      if (status == VK_SUCCESS)
        m_latencyTracker->notifyCpuPresentEnd(m_wctx->frameId);
      else
        m_latencyTracker->discardTimings();
    }

    SyncFrameLatency();

    DxvkLatencyStats latencyStats = { };

    if (m_latencyTracker && status == VK_SUCCESS) {
      latencyStats = m_latencyTracker->getStatistics(m_wctx->frameId);
      m_latencyTracker->sleepAndBeginFrame(m_wctx->frameId + 1, std::abs(m_targetFrameRate));

      m_parent->BeginFrame(m_latencyTracker, m_wctx->frameId + 1u);
    }

    if (m_latencyHud)
      m_latencyHud->accumulateStats(latencyStats);

    // Rotate swap chain buffers so that the back
    // buffer at index 0 becomes the front buffer.
    uint32_t rotatingBufferCount = m_backBuffers.size();
    if (!SwapWithFrontBuffer() && m_parent->GetOptions()->extraFrontbuffer) {
      // The front buffer only exists for GetFrontBufferData
      // and the application cannot obserse buffer swapping in GetBackBuffer()
      rotatingBufferCount -= 1;
    }

    // Backbuffer 0 is the one that gets copied to the Vulkan swapchain backbuffer.
    // => m_backBuffers[1] is the next one that gets presented
    // and the currente m_backBuffers[0] ends up at the end of the vector.
    for (uint32_t i = 1; i < rotatingBufferCount; i++)
      m_backBuffers[i]->Swap(m_backBuffers[i - 1].ptr());

    m_parent->m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
  }


  Rc<Presenter> D3D9SwapChainEx::CreatePresenter(HWND Window, Rc<sync::Signal> Signal) {
    PresenterDesc presenterDesc;
    presenterDesc.deferSurfaceCreation = m_parent->GetOptions()->deferSurfaceCreation;

    Rc<Presenter> presenter = new Presenter(m_device, Signal, presenterDesc, [
      cDevice = m_device,
      cWindow = Window
    ] (VkSurfaceKHR* surface) {
      auto vki = cDevice->adapter()->vki();

      return wsi::createSurface(cWindow,
        vki->getLoaderProc(),
        vki->instance(),
        surface);
    });

    presenter->setSurfaceExtent(m_swapchainExtent);
    presenter->setSurfaceFormat(GetSurfaceFormat());

    if (m_hdrMetadata)
      presenter->setHdrMetadata(*m_hdrMetadata);

    return presenter;
  }


  void D3D9SwapChainEx::DestroyBackBuffers() {
    for (auto& backBuffer : m_backBuffers)
      backBuffer->ClearContainer();

    m_backBuffers.clear();
  }


  bool D3D9SwapChainEx::UpdateWindowCtx() {
    if (!m_window)
      return false;

    auto entry = m_presenters.find(m_window);

    if (entry == m_presenters.end()) {
      entry = m_presenters.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(m_window),
        std::forward_as_tuple()).first;

      entry->second.frameLatencySignal = new sync::Fence(entry->second.frameId);
      entry->second.presenter = CreatePresenter(m_window, entry->second.frameLatencySignal);

      if (m_presentParams.hDeviceWindow == m_window && m_latencyTracking)
        m_latencyTracker = m_device->createLatencyTracker(entry->second.presenter);
    }

    m_wctx = &entry->second;
    return true;
  }


  HRESULT D3D9SwapChainEx::CreateBackBuffers(uint32_t NumBackBuffers, DWORD Flags) {
    // Explicitly destroy current swap image before
    // creating a new one to free up resources
    DestroyBackBuffers();

    int frontBufferCount = (SwapWithFrontBuffer() || m_parent->GetOptions()->extraFrontbuffer) ? 1 : 0;
    const uint32_t bufferCount = NumBackBuffers + frontBufferCount;

    m_backBuffers.reserve(bufferCount);

    // Create new back buffer
    D3D9_COMMON_TEXTURE_DESC desc;
    desc.Width              = std::max(m_presentParams.BackBufferWidth,  1u);
    desc.Height             = std::max(m_presentParams.BackBufferHeight, 1u);
    desc.Depth              = 1;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = EnumerateFormat(m_presentParams.BackBufferFormat);
    desc.MultiSample        = m_presentParams.MultiSampleType;
    desc.MultisampleQuality = m_presentParams.MultiSampleQuality;
    desc.Pool               = D3DPOOL_DEFAULT;
    desc.Usage              = D3DUSAGE_RENDERTARGET;
    desc.Discard            = FALSE;
    desc.IsBackBuffer       = TRUE;
    desc.IsAttachmentOnly   = FALSE;
    // we cannot respect D3DPRESENTFLAG_LOCKABLE_BACKBUFFER here because
    // we might need to lock for the BlitGDI fallback path
    desc.IsLockable         = true;

    for (uint32_t i = 0; i < bufferCount; i++) {
      D3D9Surface* surface;
      try {
        surface = new D3D9Surface(m_parent, &desc, m_parent->IsExtended(), this, nullptr);
        m_parent->IncrementLosableCounter();
      } catch (const DxvkError& e) {
        DestroyBackBuffers();
        Logger::err(e.message());
        return D3DERR_OUTOFVIDEOMEMORY;
      }

      m_backBuffers.emplace_back(surface);
    }

    // Initialize the image so that we can use it. Clearing
    // to black prevents garbled output for the first frame.
    small_vector<Rc<DxvkImage>, 4> images;

    for (size_t i = 0; i < m_backBuffers.size(); i++)
      images.push_back(m_backBuffers[i]->GetCommonTexture()->GetImage());

    m_parent->InjectCs([
      cImages = std::move(images)
    ] (DxvkContext* ctx) {
      for (size_t i = 0; i < cImages.size(); i++) {
        ctx->initImage(cImages[i], VK_IMAGE_LAYOUT_UNDEFINED);
      }
    });

    return D3D_OK;
  }


  void D3D9SwapChainEx::CreateBlitter() {
    Rc<hud::Hud> hud = hud::Hud::createHud(m_device);

    if (hud) {
      m_apiHud = hud->addItem<hud::HudClientApiItem>("api", 1, GetApiName());

      if (m_latencyTracking)
        m_latencyHud = hud->addItem<hud::HudLatencyItem>("latency", 4);

      hud->addItem<hud::HudSamplerCount>("samplers", -1, m_parent);
      hud->addItem<hud::HudFixedFunctionShaders>("ffshaders", -1, m_parent);
      hud->addItem<hud::HudSWVPState>("swvp", -1, m_parent);

#ifdef D3D9_ALLOW_UNMAPPING
      hud->addItem<hud::HudTextureMemory>("memory", -1, m_parent);
#endif
    }

    m_blitter = new DxvkSwapchainBlitter(m_device, std::move(hud));
  }


  void D3D9SwapChainEx::InitRamp() {
    for (uint32_t i = 0; i < NumControlPoints; i++) {
      DWORD identity = DWORD(MapGammaControlPoint(float(i) / float(NumControlPoints - 1)));

      m_ramp.red[i]   = identity;
      m_ramp.green[i] = identity;
      m_ramp.blue[i]  = identity;
    }
  }


  void D3D9SwapChainEx::DestroyLatencyTracker() {
    if (!m_latencyTracker)
      return;

    m_parent->InjectCs([
      cTracker = std::move(m_latencyTracker)
    ] (DxvkContext* ctx) {
      ctx->endLatencyTracking(cTracker);
    });
  }


  void D3D9SwapChainEx::UpdateTargetFrameRate(uint32_t SyncInterval) {
    double frameRateOption = double(m_parent->GetOptions()->maxFrameRate);
    double frameRate = std::max(frameRateOption, 0.0);

    if (frameRateOption == 0.0 && SyncInterval) {
      bool engageLimiter = SyncInterval > 1u || m_monitor ||
        m_device->config().latencySleep == Tristate::True;

      if (engageLimiter)
        frameRate = -m_displayRefreshRate / double(SyncInterval);
    }

    m_wctx->presenter->setFrameRateLimit(frameRate, GetActualFrameLatency());
    m_targetFrameRate = frameRate;
  }


  void D3D9SwapChainEx::SyncFrameLatency() {
    // Wait for the sync event so that we respect the maximum frame latency
    m_wctx->frameLatencySignal->wait(m_wctx->frameId - GetActualFrameLatency());
  }

  uint32_t D3D9SwapChainEx::GetActualFrameLatency() {
    uint32_t maxFrameLatency = m_parent->GetFrameLatency();

    if (m_frameLatencyCap)
      maxFrameLatency = std::min(maxFrameLatency, m_frameLatencyCap);

    maxFrameLatency = std::min(maxFrameLatency, m_presentParams.BackBufferCount + 1);
    return maxFrameLatency;
  }


  VkSurfaceFormatKHR D3D9SwapChainEx::GetSurfaceFormat() {
    D3D9Format format = EnumerateFormat(m_presentParams.BackBufferFormat);

    switch (format) {
      default:
        Logger::warn(str::format("D3D9SwapChainEx: Unexpected format: ", format));
        [[fallthrough]];

      case D3D9Format::A8R8G8B8:
      case D3D9Format::X8R8G8B8:
        return { VK_FORMAT_B8G8R8A8_UNORM, m_colorspace };

      case D3D9Format::A8B8G8R8:
      case D3D9Format::X8B8G8R8:
        return { VK_FORMAT_R8G8B8A8_UNORM, m_colorspace };

      case D3D9Format::A2R10G10B10:
        return { VK_FORMAT_A2R10G10B10_UNORM_PACK32, m_colorspace };

      case D3D9Format::A2B10G10R10:
        return { VK_FORMAT_A2B10G10R10_UNORM_PACK32, m_colorspace };

      case D3D9Format::X1R5G5B5:
      case D3D9Format::A1R5G5B5:
        return { VK_FORMAT_B5G5R5A1_UNORM_PACK16, m_colorspace };

      case D3D9Format::R5G6B5:
        return { VK_FORMAT_B5G6R5_UNORM_PACK16, m_colorspace };

      case D3D9Format::A16B16G16R16F: {
        if (!m_unlockAdditionalFormats) {
          Logger::warn(str::format("D3D9SwapChainEx: Unexpected format: ", format));
          return VkSurfaceFormatKHR { };
        }

        return { VK_FORMAT_R16G16B16A16_SFLOAT, m_colorspace };
      }
    }
  }


  void D3D9SwapChainEx::UpdateWindowedRefreshRate() {
    // Ignore call if we are in fullscreen mode and
    // know the active display mode already anyway
    if (!m_displayRefreshRateDirty || m_monitor)
      return;

    m_displayRefreshRate = 0.0;
    m_displayRefreshRateDirty = false;

    HMONITOR monitor = wsi::getWindowMonitor(m_window);

    if (!monitor)
      return;

    wsi::WsiMode mode = { };

    if (!wsi::getCurrentDisplayMode(monitor, &mode))
      return;

    if (mode.refreshRate.denominator) {
      m_displayRefreshRate = double(mode.refreshRate.numerator)
                           / double(mode.refreshRate.denominator);
    }
  }


  HRESULT D3D9SwapChainEx::EnterFullscreenMode(
          D3DPRESENT_PARAMETERS* pPresentParams,
    const D3DDISPLAYMODEEX*      pFullscreenDisplayMode) {    
    if (FAILED(ChangeDisplayMode(pPresentParams, pFullscreenDisplayMode))) {
      Logger::err("D3D9: EnterFullscreenMode: Failed to change display mode");
      return D3DERR_INVALIDCALL;
    }

    // Testing shows we shouldn't hook WM_NCCALCSIZE but we shouldn't change
    // windows style either.
    //
    // Some games restore window styles after we have changed it, so hooking is
    // also required. Doing it will allow us to create fullscreen windows
    // regardless of their style and it also appears to work on Windows.
    HookWindowProc(m_window, this);

    D3D9WindowMessageFilter filter(m_window);
    
    m_monitor = wsi::getDefaultMonitor();

    if (!wsi::enterFullscreenMode(m_monitor, m_window, &m_windowState, true)) {
        Logger::err("D3D9: EnterFullscreenMode: Failed to enter fullscreen mode");
        return D3DERR_INVALIDCALL;
    }
    
    m_parent->NotifyFullscreen(m_window, true);
    
    return D3D_OK;
  }
  
  
  HRESULT D3D9SwapChainEx::LeaveFullscreenMode() {
    if (!wsi::isWindow(m_window))
      return D3DERR_INVALIDCALL;
    
    if (FAILED(RestoreDisplayMode(m_monitor)))
      Logger::warn("D3D9: LeaveFullscreenMode: Failed to restore display mode");
    
    m_monitor = nullptr;

    ResetWindowProc(m_window);
    
    if (!wsi::leaveFullscreenMode(m_window, &m_windowState, false)) {
      Logger::err("D3D9: LeaveFullscreenMode: Failed to exit fullscreen mode");
      return D3DERR_NOTAVAILABLE;
    }

    m_parent->NotifyFullscreen(m_window, false);
    
    return D3D_OK;
  }
  
  
  HRESULT D3D9SwapChainEx::ChangeDisplayMode(
          D3DPRESENT_PARAMETERS* pPresentParams,
    const D3DDISPLAYMODEEX*      pFullscreenDisplayMode) {
    D3DDISPLAYMODEEX mode;

    if (pFullscreenDisplayMode) {
      mode = *pFullscreenDisplayMode;
    } else {
      mode.Width            = pPresentParams->BackBufferWidth;
      mode.Height           = pPresentParams->BackBufferHeight;
      mode.Format           = pPresentParams->BackBufferFormat;
      mode.RefreshRate      = pPresentParams->FullScreen_RefreshRateInHz;
      mode.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
      mode.Size             = sizeof(D3DDISPLAYMODEEX);
    }

    wsi::WsiMode wsiMode = ConvertDisplayMode(mode);
    
    HMONITOR monitor = wsi::getDefaultMonitor();

    if (!wsi::setWindowMode(monitor, m_window, &m_windowState, wsiMode))
      return D3DERR_NOTAVAILABLE;

    m_displayRefreshRate = 0.0;

    if (wsi::getCurrentDisplayMode(monitor, &wsiMode)) {
      m_displayRefreshRate = double(wsiMode.refreshRate.numerator)
                           / double(wsiMode.refreshRate.denominator);
    }

    m_displayRefreshRateDirty = false;
    return D3D_OK;
  }
  
  
  HRESULT D3D9SwapChainEx::RestoreDisplayMode(HMONITOR hMonitor) {
    if (hMonitor == nullptr)
      return D3DERR_INVALIDCALL;
    
    if (!wsi::restoreDisplayMode())
      return D3DERR_NOTAVAILABLE;

    m_displayRefreshRateDirty = true;
    return D3D_OK;
  }

  void D3D9SwapChainEx::UpdatePresentRegion(const RECT* pSourceRect, const RECT* pDestRect) {
    const bool isWindowed = m_presentParams.Windowed;

    // Tests show that present regions are ignored in fullscreen

    if (pSourceRect == nullptr || !isWindowed) {
      m_srcRect.top    = 0;
      m_srcRect.left   = 0;
      m_srcRect.right  = m_presentParams.BackBufferWidth;
      m_srcRect.bottom = m_presentParams.BackBufferHeight;
    }
    else
      m_srcRect = *pSourceRect;

    
    UINT width, height;
    wsi::getWindowSize(m_window, &width, &height);

    RECT dstRect;
    if (pDestRect == nullptr || !isWindowed) {
      // TODO: Should we hook WM_SIZE message for this?
      dstRect.top    = 0;
      dstRect.left   = 0;
      dstRect.right  = LONG(width);
      dstRect.bottom = LONG(height);
      
    }
    else
      dstRect = *pDestRect;

    m_partialCopy =
       dstRect.left != 0
    || dstRect.top != 0
    || dstRect.right  - dstRect.left != LONG(width)
    || dstRect.bottom - dstRect.top  != LONG(height);

    m_swapchainExtent = { width, height };
    m_dstRect = dstRect;
  }

  void D3D9SwapChainEx::UpdatePresentParameters() {
    if (m_wctx) {
      m_wctx->presenter->setSurfaceExtent(m_swapchainExtent);
      m_wctx->presenter->setSurfaceFormat(GetSurfaceFormat());
    }
  }

  VkExtent2D D3D9SwapChainEx::GetPresentExtent() {
    return m_swapchainExtent;
  }


  std::string D3D9SwapChainEx::GetApiName() {
    return this->GetParent()->IsD3D8Compatible() ? "D3D8" :
           this->GetParent()->IsExtended() ? "D3D9Ex" : "D3D9";
  }


  bool D3D9SwapChainEx::IsDeviceReset(D3D9WindowContext* wctx) {
    uint32_t counter = m_parent->GetResetCounter();

    if (counter == wctx->deviceResetCounter)
      return false;

    wctx->deviceResetCounter = counter;
    return true;
  }


  D3D9VkExtSwapchain::D3D9VkExtSwapchain(D3D9SwapChainEx *pSwapChain)
    : m_swapchain(pSwapChain) {

  }
  
  ULONG STDMETHODCALLTYPE D3D9VkExtSwapchain::AddRef() {
    return m_swapchain->AddRef();
  }
  
  ULONG STDMETHODCALLTYPE D3D9VkExtSwapchain::Release() {
    return m_swapchain->Release();
  }
  
  HRESULT STDMETHODCALLTYPE D3D9VkExtSwapchain::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_swapchain->QueryInterface(riid, ppvObject);
  }

  BOOL STDMETHODCALLTYPE D3D9VkExtSwapchain::CheckColorSpaceSupport(
          VkColorSpaceKHR           ColorSpace) {
    return m_swapchain->m_wctx->presenter->supportsColorSpace(ColorSpace);
  }

  HRESULT STDMETHODCALLTYPE D3D9VkExtSwapchain::SetColorSpace(
          VkColorSpaceKHR           ColorSpace) {
    if (!CheckColorSpaceSupport(ColorSpace))
      return D3DERR_INVALIDCALL;
    
    m_swapchain->m_colorspace = ColorSpace;

    if (m_swapchain->m_wctx)
      m_swapchain->m_wctx->presenter->setSurfaceFormat(m_swapchain->GetSurfaceFormat());

    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9VkExtSwapchain::SetHDRMetaData(
    const VkHdrMetadataEXT          *pHDRMetadata) {
    if (!pHDRMetadata)
      return D3DERR_INVALIDCALL;

    m_swapchain->m_hdrMetadata = *pHDRMetadata;

    if (m_swapchain->m_wctx)
      m_swapchain->m_wctx->presenter->setHdrMetadata(*pHDRMetadata);

    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9VkExtSwapchain::GetCurrentOutputDesc(
          D3D9VkExtOutputMetadata   *pOutputDesc) {
    HMONITOR monitor = m_swapchain->m_monitor;
    if (!monitor)
      monitor = wsi::getDefaultMonitor();
    // ^ this should be the display we are mostly covering someday.

    wsi::WsiEdidData edidData = wsi::getMonitorEdid(monitor);
    wsi::WsiDisplayMetadata metadata = {};
    {
      std::optional<wsi::WsiDisplayMetadata> r_metadata = std::nullopt;
      if (!edidData.empty())
        r_metadata = wsi::parseColorimetryInfo(edidData);

      if (r_metadata)
        metadata = *r_metadata;
      else
        Logger::err("D3D9: Failed to parse display metadata + colorimetry info, using blank.");
    }


    NormalizeDisplayMetadata(CheckColorSpaceSupport(VK_COLOR_SPACE_HDR10_ST2084_EXT), metadata);

    pOutputDesc->RedPrimary[0]         = metadata.redPrimary[0];
    pOutputDesc->RedPrimary[1]         = metadata.redPrimary[1];
    pOutputDesc->GreenPrimary[0]       = metadata.greenPrimary[0];
    pOutputDesc->GreenPrimary[1]       = metadata.greenPrimary[1];
    pOutputDesc->BluePrimary[0]        = metadata.bluePrimary[0];
    pOutputDesc->BluePrimary[1]        = metadata.bluePrimary[1];
    pOutputDesc->WhitePoint[0]         = metadata.whitePoint[0];
    pOutputDesc->WhitePoint[1]         = metadata.whitePoint[1];
    pOutputDesc->MinLuminance          = metadata.minLuminance;
    pOutputDesc->MaxLuminance          = metadata.maxLuminance;
    pOutputDesc->MaxFullFrameLuminance = metadata.maxFullFrameLuminance;
    return S_OK;
  }

  void STDMETHODCALLTYPE D3D9VkExtSwapchain::UnlockAdditionalFormats() {
    m_swapchain->m_unlockAdditionalFormats = true;
  }

}
