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


  struct D3D9PresentInfo {
    float scale[2];
    float offset[2];
  };


  D3D9SwapChainEx::D3D9SwapChainEx(
          D3D9DeviceEx*          pDevice,
          D3DPRESENT_PARAMETERS* pPresentParams,
    const D3DDISPLAYMODEEX*      pFullscreenDisplayMode)
    : D3D9SwapChainExBase(pDevice)
    , m_device           (pDevice->GetDXVKDevice())
    , m_context          (m_device->createContext(DxvkContextType::Supplementary))
    , m_frameLatencyCap  (pDevice->GetOptions()->maxFrameLatency)
    , m_dialog           (pDevice->GetOptions()->enableDialogMode)
    , m_swapchainExt     (this) {
    this->NormalizePresentParameters(pPresentParams);
    m_presentParams = *pPresentParams;
    m_window = m_presentParams.hDeviceWindow;

    UpdateWindowCtx();

    UpdatePresentRegion(nullptr, nullptr);

    if (m_window) {
      CreatePresenter();

      if (!pDevice->GetOptions()->deferSurfaceCreation)
        RecreateSwapChain();
    }

    if (FAILED(CreateBackBuffers(m_presentParams.BackBufferCount)))
      throw DxvkError("D3D9: Failed to create swapchain backbuffers");

    CreateBlitter();
    CreateHud();

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

    m_device->waitForSubmission(&m_presentStatus);
    m_device->waitForIdle();

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

    m_window = m_presentParams.hDeviceWindow;
    if (hDestWindowOverride != nullptr)
      m_window = hDestWindowOverride;

    UpdateWindowCtx();

    bool recreate = false;
    recreate   |= m_wctx->presenter == nullptr;
    recreate   |= m_dialog != m_lastDialog;
    if (options->deferSurfaceCreation)
      recreate |= m_parent->IsDeviceReset();

    if (m_wctx->presenter != nullptr) {
      m_dirty  |= m_wctx->presenter->setSyncInterval(presentInterval) != VK_SUCCESS;
      m_dirty  |= !m_wctx->presenter->hasSwapChain();
    }

    m_dirty    |= UpdatePresentRegion(pSourceRect, pDestRect);
    m_dirty    |= recreate;

    m_lastDialog = m_dialog;

#ifdef _WIN32
    const bool useGDIFallback = m_partialCopy && !HasFrontBuffer();
    if (useGDIFallback)
      return PresentImageGDI(m_window);
#endif

    try {
      if (recreate)
        CreatePresenter();

      if (std::exchange(m_dirty, false))
        RecreateSwapChain();

      // We aren't going to device loss simply because
      // 99% of D3D9 games don't handle this properly and
      // just end up crashing (like with alt-tab loss)
      if (!m_wctx->presenter->hasSwapChain())
        return D3D_OK;

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
    m_parent->EndFrame();
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

        ctx->resolveImage(
          cDstImage, cSrcImage,
          resolveRegion, VK_FORMAT_UNDEFINED);
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

      m_parent->EmitCs([
        cDstImage = blittedSrc,
        cDstMap   = dstTexInfo->GetMapping().Swizzle,
        cSrcImage = srcImage,
        cSrcMap   = srcTexInfo->GetMapping().Swizzle,
        cBlitInfo = blitInfo
      ] (DxvkContext* ctx) {
        ctx->blitImage(
          cDstImage, cDstMap,
          cSrcImage, cSrcMap,
          cBlitInfo, VK_FILTER_NEAREST);
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
        cBufferSlice.offset(), 4, 0, cImage,
        cSubresources, VkOffset3D { 0, 0, 0 },
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

    this->SynchronizePresent();
    this->NormalizePresentParameters(pPresentParams);

    m_dirty    |= m_presentParams.BackBufferFormat   != pPresentParams->BackBufferFormat
               || m_presentParams.BackBufferCount    != pPresentParams->BackBufferCount;

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

    hr = CreateBackBuffers(m_presentParams.BackBufferCount);
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
      Logger::err("validateGammaRamp: ramp inverted or flat");
      return false;
    }

    for (size_t i = 1; i < std::size(ramp); i++) {
      if (ramp[i] < ramp[i - 1]) {
        Logger::err("validateGammaRamp: ramp not monotonically increasing");
        return false;
      }
      if (ramp[i] - ramp[i - 1] >= UINT16_MAX / 2) {
        Logger::err("validateGammaRamp: huuuge jump");
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
    if (hWindow == nullptr)
      hWindow = m_parent->GetWindow();

    if (m_presenters.count(hWindow)) {
      if (m_wctx == &m_presenters[hWindow])
        m_wctx = nullptr;
      m_presenters.erase(hWindow);

      m_device->waitForSubmission(&m_presentStatus);
      m_device->waitForIdle();
    }
  }


  HRESULT D3D9SwapChainEx::SetDialogBoxMode(bool bEnableDialogs) {
    D3D9DeviceLock lock = m_parent->LockDevice();

    // https://docs.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setdialogboxmode
    // The MSDN documentation says this will error out under many weird conditions.
    // However it doesn't appear to error at all in any of my tests of these
    // cases described in the documentation.

    m_dialog = bEnableDialogs;

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
    m_parent->EndFrame();
    m_parent->Flush();

    // Retrieve the image and image view to present
    Rc<DxvkImage> swapImage = m_backBuffers[0]->GetCommonTexture()->GetImage();
    Rc<DxvkImageView> swapImageView = m_backBuffers[0]->GetImageView(false);

    for (uint32_t i = 0; i < SyncInterval || i < 1; i++) {
      SynchronizePresent();

      // Presentation semaphores and WSI swap chain image
      PresenterInfo info = m_wctx->presenter->info();
      PresenterSync sync;

      uint32_t imageIndex = 0;

      VkResult status = m_wctx->presenter->acquireNextImage(sync, imageIndex);

      while (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR) {
        RecreateSwapChain();
        
        info = m_wctx->presenter->info();
        status = m_wctx->presenter->acquireNextImage(sync, imageIndex);
      }

      if (m_hdrMetadata && m_dirtyHdrMetadata) {
        m_wctx->presenter->setHdrMetadata(*m_hdrMetadata);
        m_dirtyHdrMetadata = false;
      }

      m_context->beginRecording(
        m_device->createCommandList());

      VkRect2D srcRect = {
        {  int32_t(m_srcRect.left),                    int32_t(m_srcRect.top)                    },
        { uint32_t(m_srcRect.right - m_srcRect.left), uint32_t(m_srcRect.bottom - m_srcRect.top) } };

      VkRect2D dstRect = {
        {  int32_t(m_dstRect.left),                    int32_t(m_dstRect.top)                    },
        { uint32_t(m_dstRect.right - m_dstRect.left), uint32_t(m_dstRect.bottom - m_dstRect.top) } };

      m_blitter->presentImage(m_context.ptr(),
        m_wctx->imageViews.at(imageIndex), dstRect,
        swapImageView, srcRect);

      if (m_hud != nullptr)
        m_hud->render(m_context, info.format, info.imageExtent);

      SubmitPresent(sync, i);
    }

    SyncFrameLatency();

    // Rotate swap chain buffers so that the back
    // buffer at index 0 becomes the front buffer.
    for (uint32_t i = 1; i < m_backBuffers.size(); i++)
      m_backBuffers[i]->Swap(m_backBuffers[i - 1].ptr());

    m_parent->m_flags.set(D3D9DeviceFlag::DirtyFramebuffer);
  }


  void D3D9SwapChainEx::SubmitPresent(const PresenterSync& Sync, uint32_t Repeat) {
    // Bump frame ID
    if (!Repeat)
      m_wctx->frameId += 1;

    // Present from CS thread so that we don't
    // have to synchronize with it first.
    m_presentStatus.result = VK_NOT_READY;

    m_parent->EmitCs([this,
      cRepeat      = Repeat,
      cSync        = Sync,
      cHud         = m_hud,
      cPresentMode = m_wctx->presenter->info().presentMode,
      cFrameId     = m_wctx->frameId,
      cCommandList = m_context->endRecording()
    ] (DxvkContext* ctx) {
      cCommandList->setWsiSemaphores(cSync);
      m_device->submitCommandList(cCommandList, nullptr);

      if (cHud != nullptr && !cRepeat)
        cHud->update();

      uint64_t frameId = cRepeat ? 0 : cFrameId;

      m_device->presentImage(m_wctx->presenter,
        cPresentMode, frameId, &m_presentStatus);
    });

    m_parent->FlushCsChunk();
  }


  void D3D9SwapChainEx::SynchronizePresent() {
    // Recreate swap chain if the previous present call failed
    VkResult status = m_device->waitForSubmission(&m_presentStatus);

    if (status != VK_SUCCESS)
      RecreateSwapChain();
  }

  void D3D9SwapChainEx::RecreateSwapChain() {
    // Ensure that we can safely destroy the swap chain
    m_device->waitForSubmission(&m_presentStatus);
    m_device->waitForIdle();

    m_presentStatus.result = VK_SUCCESS;

    PresenterDesc presenterDesc;
    presenterDesc.imageExtent     = GetPresentExtent();
    presenterDesc.imageCount      = PickImageCount(m_presentParams.BackBufferCount + 1);
    presenterDesc.numFormats      = PickFormats(EnumerateFormat(m_presentParams.BackBufferFormat), presenterDesc.formats);
    presenterDesc.fullScreenExclusive = PickFullscreenMode();

    VkResult vr = m_wctx->presenter->recreateSwapChain(presenterDesc);

    if (vr == VK_ERROR_SURFACE_LOST_KHR) {
      vr = m_wctx->presenter->recreateSurface([this] (VkSurfaceKHR* surface) {
        return CreateSurface(surface);
      });

      if (vr)
        throw DxvkError(str::format("D3D9SwapChainEx: Failed to recreate surface: ", vr));

      vr = m_wctx->presenter->recreateSwapChain(presenterDesc);
    }

    if (vr)
      throw DxvkError(str::format("D3D9SwapChainEx: Failed to recreate swap chain: ", vr));
    
    CreateRenderTargetViews();
  }


  void D3D9SwapChainEx::CreatePresenter() {
    // Ensure that we can safely destroy the swap chain
    m_device->waitForSubmission(&m_presentStatus);
    m_device->waitForIdle();

    m_presentStatus.result = VK_SUCCESS;

    PresenterDesc presenterDesc;
    presenterDesc.imageExtent     = GetPresentExtent();
    presenterDesc.imageCount      = PickImageCount(m_presentParams.BackBufferCount + 1);
    presenterDesc.numFormats      = PickFormats(EnumerateFormat(m_presentParams.BackBufferFormat), presenterDesc.formats);
    presenterDesc.fullScreenExclusive = PickFullscreenMode();

    m_wctx->presenter = new Presenter(m_device, m_wctx->frameLatencySignal, presenterDesc);
    m_wctx->presenter->setFrameRateLimit(m_parent->GetOptions()->maxFrameRate);
  }


  VkResult D3D9SwapChainEx::CreateSurface(VkSurfaceKHR* pSurface) {
    auto vki = m_device->adapter()->vki();

    return wsi::createSurface(m_window,
      vki->getLoaderProc(),
      vki->instance(),
      pSurface);
  }


  void D3D9SwapChainEx::CreateRenderTargetViews() {
    PresenterInfo info = m_wctx->presenter->info();

    m_wctx->imageViews.clear();
    m_wctx->imageViews.resize(info.imageCount);

    DxvkImageCreateInfo imageInfo;
    imageInfo.type        = VK_IMAGE_TYPE_2D;
    imageInfo.format      = info.format.format;
    imageInfo.flags       = 0;
    imageInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.extent      = { info.imageExtent.width, info.imageExtent.height, 1 };
    imageInfo.numLayers   = 1;
    imageInfo.mipLevels   = 1;
    imageInfo.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.stages      = 0;
    imageInfo.access      = 0;
    imageInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.layout      = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imageInfo.shared      = VK_TRUE;

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type         = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format       = info.format.format;
    viewInfo.usage        = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    viewInfo.aspect       = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel     = 0;
    viewInfo.numLevels    = 1;
    viewInfo.minLayer     = 0;
    viewInfo.numLayers    = 1;

    for (uint32_t i = 0; i < info.imageCount; i++) {
      VkImage imageHandle = m_wctx->presenter->getImage(i).image;
      
      Rc<DxvkImage> image = new DxvkImage(
        m_device.ptr(), imageInfo, imageHandle,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      m_wctx->imageViews[i] = new DxvkImageView(
        m_device->vkd(), image, viewInfo);
    }
  }


  void D3D9SwapChainEx::DestroyBackBuffers() {
    for (auto& backBuffer : m_backBuffers)
      backBuffer->ClearContainer();

    m_backBuffers.clear();
  }


  void D3D9SwapChainEx::UpdateWindowCtx() {
    if (!m_presenters.count(m_window)) {
      auto res = m_presenters.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(m_window),
        std::forward_as_tuple());

      auto& wctx = res.first->second;
      wctx.frameLatencySignal = new sync::Fence(wctx.frameId);
    }
    m_wctx = &m_presenters[m_window];
  }


  HRESULT D3D9SwapChainEx::CreateBackBuffers(uint32_t NumBackBuffers) {
    // Explicitly destroy current swap image before
    // creating a new one to free up resources
    DestroyBackBuffers();

    int NumFrontBuffer = HasFrontBuffer() ? 1 : 0;
    const uint32_t NumBuffers = NumBackBuffers + NumFrontBuffer;

    m_backBuffers.reserve(NumBuffers);

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
    // Docs: Also note that - unlike textures - swap chain back buffers, render targets [..] can be locked
    desc.IsLockable         = TRUE;

    for (uint32_t i = 0; i < NumBuffers; i++) {
      D3D9Surface* surface;
      try {
        surface = new D3D9Surface(m_parent, &desc, this, nullptr);
        m_parent->IncrementLosableCounter();
      } catch (const DxvkError& e) {
        DestroyBackBuffers();
        Logger::err(e.message());
        return D3DERR_OUTOFVIDEOMEMORY;
      }

      m_backBuffers.emplace_back(surface);
    }

    auto swapImage = m_backBuffers[0]->GetCommonTexture()->GetImage();

    // Initialize the image so that we can use it. Clearing
    // to black prevents garbled output for the first frame.
    VkImageSubresourceRange subresources;
    subresources.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    subresources.baseMipLevel   = 0;
    subresources.levelCount     = 1;
    subresources.baseArrayLayer = 0;
    subresources.layerCount     = 1;

    m_context->beginRecording(
      m_device->createCommandList());
    
    for (uint32_t i = 0; i < m_backBuffers.size(); i++) {
      m_context->initImage(
        m_backBuffers[i]->GetCommonTexture()->GetImage(),
        subresources, VK_IMAGE_LAYOUT_UNDEFINED);
    }

    m_device->submitCommandList(
      m_context->endRecording(),
      nullptr);

    return D3D_OK;
  }


  void D3D9SwapChainEx::CreateBlitter() {
    m_blitter = new DxvkSwapchainBlitter(m_device);
  }


  void D3D9SwapChainEx::CreateHud() {
    m_hud = hud::Hud::createHud(m_device);

    if (m_hud != nullptr) {
      m_hud->addItem<hud::HudClientApiItem>("api", 1, GetApiName());
      m_hud->addItem<hud::HudSamplerCount>("samplers", -1, m_parent);

#ifdef D3D9_ALLOW_UNMAPPING
      m_hud->addItem<hud::HudTextureMemory>("memory", -1, m_parent);
#endif
    }
  }


  void D3D9SwapChainEx::InitRamp() {
    for (uint32_t i = 0; i < NumControlPoints; i++) {
      DWORD identity = DWORD(MapGammaControlPoint(float(i) / float(NumControlPoints - 1)));

      m_ramp.red[i]   = identity;
      m_ramp.green[i] = identity;
      m_ramp.blue[i]  = identity;
    }
  }


  void D3D9SwapChainEx::SyncFrameLatency() {
    // Wait for the sync event so that we respect the maximum frame latency
    m_wctx->frameLatencySignal->wait(m_wctx->frameId - GetActualFrameLatency());
  }

  void D3D9SwapChainEx::SetApiName(const char* name) {
    m_apiName = name;
    CreateHud();
  }

  uint32_t D3D9SwapChainEx::GetActualFrameLatency() {
    uint32_t maxFrameLatency = m_parent->GetFrameLatency();

    if (m_frameLatencyCap)
      maxFrameLatency = std::min(maxFrameLatency, m_frameLatencyCap);

    maxFrameLatency = std::min(maxFrameLatency, m_presentParams.BackBufferCount + 1);
    return maxFrameLatency;
  }


  uint32_t D3D9SwapChainEx::PickFormats(
          D3D9Format                Format,
          VkSurfaceFormatKHR*       pDstFormats) {
    uint32_t n = 0;

    switch (Format) {
      default:
        Logger::warn(str::format("D3D9SwapChainEx: Unexpected format: ", Format));      
     [[fallthrough]];

      case D3D9Format::A8R8G8B8:
      case D3D9Format::X8R8G8B8:
      case D3D9Format::A8B8G8R8:
      case D3D9Format::X8B8G8R8: {
        pDstFormats[n++] = { VK_FORMAT_R8G8B8A8_UNORM, m_colorspace };
        pDstFormats[n++] = { VK_FORMAT_B8G8R8A8_UNORM, m_colorspace };
      } break;

      case D3D9Format::A2R10G10B10:
      case D3D9Format::A2B10G10R10: {
        pDstFormats[n++] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, m_colorspace };
        pDstFormats[n++] = { VK_FORMAT_A2R10G10B10_UNORM_PACK32, m_colorspace };
      } break;

      case D3D9Format::X1R5G5B5:
      case D3D9Format::A1R5G5B5: {
        pDstFormats[n++] = { VK_FORMAT_B5G5R5A1_UNORM_PACK16, m_colorspace };
        pDstFormats[n++] = { VK_FORMAT_R5G5B5A1_UNORM_PACK16, m_colorspace };
        pDstFormats[n++] = { VK_FORMAT_A1R5G5B5_UNORM_PACK16, m_colorspace };
      } break;

      case D3D9Format::R5G6B5: {
        pDstFormats[n++] = { VK_FORMAT_B5G6R5_UNORM_PACK16, m_colorspace };
        pDstFormats[n++] = { VK_FORMAT_R5G6B5_UNORM_PACK16, m_colorspace };
      } break;

      case D3D9Format::A16B16G16R16F: {
        if (m_unlockAdditionalFormats) {
          pDstFormats[n++] = { VK_FORMAT_R16G16B16A16_SFLOAT, m_colorspace };
        } else {
          Logger::warn(str::format("D3D9SwapChainEx: Unexpected format: ", Format));      
        }
        break;
      }
    }

    return n;
  }


  uint32_t D3D9SwapChainEx::PickImageCount(
          UINT                      Preferred) {
    int32_t option = m_parent->GetOptions()->numBackBuffers;
    return option > 0 ? uint32_t(option) : uint32_t(Preferred);
  }


  void D3D9SwapChainEx::NotifyDisplayRefreshRate(
          double                  RefreshRate) {
    m_displayRefreshRate = RefreshRate;
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

    if (!wsi::setWindowMode(monitor, m_window, wsiMode))
      return D3DERR_NOTAVAILABLE;
    
    if (wsi::getCurrentDisplayMode(monitor, &wsiMode))
      NotifyDisplayRefreshRate(double(wsiMode.refreshRate.numerator) / double(wsiMode.refreshRate.denominator));
    else
      NotifyDisplayRefreshRate(0.0);

    return D3D_OK;
  }
  
  
  HRESULT D3D9SwapChainEx::RestoreDisplayMode(HMONITOR hMonitor) {
    if (hMonitor == nullptr)
      return D3DERR_INVALIDCALL;
    
    if (!wsi::restoreDisplayMode())
      return D3DERR_NOTAVAILABLE;

    NotifyDisplayRefreshRate(0.0);
    return D3D_OK;
  }

  bool    D3D9SwapChainEx::UpdatePresentRegion(const RECT* pSourceRect, const RECT* pDestRect) {
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

    bool recreate =
       m_wctx->presenter == nullptr
    || m_wctx->presenter->info().imageExtent.width  != width
    || m_wctx->presenter->info().imageExtent.height != height;

    m_swapchainExtent = { width, height };
    m_dstRect = dstRect;

    return recreate;
  }

  VkExtent2D D3D9SwapChainEx::GetPresentExtent() {
    return m_swapchainExtent;
  }


  VkFullScreenExclusiveEXT D3D9SwapChainEx::PickFullscreenMode() {
    return m_dialog
      ? VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT
      : VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT;
  }


  std::string D3D9SwapChainEx::GetApiName() {
    if (m_apiName == nullptr) {
      return this->GetParent()->IsExtended() ? "D3D9Ex" : "D3D9";
    } else {
      return m_apiName;
    }
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
    
    m_swapchain->m_dirty |= ColorSpace != m_swapchain->m_colorspace;
    m_swapchain->m_colorspace = ColorSpace;

    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9VkExtSwapchain::SetHDRMetaData(
    const VkHdrMetadataEXT          *pHDRMetadata) {
    if (!pHDRMetadata)
      return D3DERR_INVALIDCALL;

    m_swapchain->m_hdrMetadata      = *pHDRMetadata;
    m_swapchain->m_dirtyHdrMetadata = true;

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
