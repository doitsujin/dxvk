#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_swapchain.h"

#include "../dxvk/dxvk_latency_builtin.h"

#include "../util/util_win32_compat.h"

namespace dxvk {

  static uint16_t MapGammaControlPoint(float x) {
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return uint16_t(65535.0f * x);
  }

  static VkColorSpaceKHR ConvertColorSpace(DXGI_COLOR_SPACE_TYPE colorspace) {
    switch (colorspace) {
      case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:    return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020: return VK_COLOR_SPACE_HDR10_ST2084_EXT;
      case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:    return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
      default:
        Logger::warn(str::format("DXGI: ConvertColorSpace: Unknown colorspace ", colorspace));
        return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
  }

  static VkXYColorEXT ConvertXYColor(const UINT16 (&dxgiColor)[2]) {
    return VkXYColorEXT{ float(dxgiColor[0]) / 50000.0f, float(dxgiColor[1]) / 50000.0f };
  }

  static float ConvertMaxLuminance(UINT dxgiLuminance) {
    return float(dxgiLuminance);
  }

  static float ConvertMinLuminance(UINT dxgiLuminance) {
    return float(dxgiLuminance) * 0.0001f;
  }

  static float ConvertLevel(UINT16 dxgiLevel) {
    return float(dxgiLevel);
  }

  static VkHdrMetadataEXT ConvertHDRMetadata(const DXGI_HDR_METADATA_HDR10& dxgiMetadata) {
    VkHdrMetadataEXT vkMetadata = { VK_STRUCTURE_TYPE_HDR_METADATA_EXT };
    vkMetadata.displayPrimaryRed         = ConvertXYColor(dxgiMetadata.RedPrimary);
    vkMetadata.displayPrimaryGreen       = ConvertXYColor(dxgiMetadata.GreenPrimary);
    vkMetadata.displayPrimaryBlue        = ConvertXYColor(dxgiMetadata.BluePrimary);
    vkMetadata.whitePoint                = ConvertXYColor(dxgiMetadata.WhitePoint);
    vkMetadata.maxLuminance              = ConvertMaxLuminance(dxgiMetadata.MaxMasteringLuminance);
    vkMetadata.minLuminance              = ConvertMinLuminance(dxgiMetadata.MinMasteringLuminance);
    vkMetadata.maxContentLightLevel      = ConvertLevel(dxgiMetadata.MaxContentLightLevel);
    vkMetadata.maxFrameAverageLightLevel = ConvertLevel(dxgiMetadata.MaxFrameAverageLightLevel);
    return vkMetadata;
  }


  D3D11SwapChain::D3D11SwapChain(
          D3D11DXGIDevice*        pContainer,
          D3D11Device*            pDevice,
          IDXGIVkSurfaceFactory*  pSurfaceFactory,
    const DXGI_SWAP_CHAIN_DESC1*  pDesc)
  : m_dxgiDevice(pContainer),
    m_parent(pDevice),
    m_surfaceFactory(pSurfaceFactory),
    m_desc(*pDesc),
    m_device(pDevice->GetDXVKDevice()),
    m_frameLatencyCap(pDevice->GetOptions()->maxFrameLatency) {
    CreateFrameLatencyEvent();
    CreatePresenter();
    CreateBackBuffers();
    CreateBlitter();
  }


  D3D11SwapChain::~D3D11SwapChain() {
    // Avoids hanging when in this state, see comment
    // in DxvkDevice::~DxvkDevice.
    if (this_thread::isInModuleDetachment())
      return;

    m_presenter->destroyResources();
    
    DestroyFrameLatencyEvent();
    DestroyLatencyTracker();
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIVkSwapChain)
     || riid == __uuidof(IDXGIVkSwapChain1)
     || riid == __uuidof(IDXGIVkSwapChain2)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDXGIVkSwapChain), riid)) {
      Logger::warn("D3D11SwapChain::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetDesc(
          DXGI_SWAP_CHAIN_DESC1*    pDesc) {
    *pDesc = m_desc;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetAdapter(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_dxgiDevice->GetParent(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetDevice(
          REFIID                    riid,
          void**                    ppDevice) {
    return m_dxgiDevice->QueryInterface(riid, ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetImage(
          UINT                      BufferId,
          REFIID                    riid,
          void**                    ppBuffer) {
    InitReturnPtr(ppBuffer);

    if (BufferId >= m_backBuffers.size()) {
      Logger::err("D3D11: GetImage: Invalid buffer ID");
      return DXGI_ERROR_UNSUPPORTED;
    }

    return m_backBuffers[BufferId]->QueryInterface(riid, ppBuffer);
  }


  UINT STDMETHODCALLTYPE D3D11SwapChain::GetImageIndex() {
    return 0;
  }


  UINT STDMETHODCALLTYPE D3D11SwapChain::GetFrameLatency() {
    return m_frameLatency;
  }


  HANDLE STDMETHODCALLTYPE D3D11SwapChain::GetFrameLatencyEvent() {
    HANDLE result = nullptr;
    HANDLE processHandle = GetCurrentProcess();

    if (!DuplicateHandle(processHandle, m_frameLatencyEvent,
        processHandle, &result, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
      Logger::err("DxgiSwapChain::GetFrameLatencyWaitableObject: DuplicateHandle failed");
      return nullptr;
    }

    return result;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::ChangeProperties(
    const DXGI_SWAP_CHAIN_DESC1*    pDesc,
    const UINT*                     pNodeMasks,
          IUnknown* const*          ppPresentQueues) {
    if (m_desc.Format != pDesc->Format)
      m_presenter->setSurfaceFormat(GetSurfaceFormat(pDesc->Format));

    if (m_desc.Width != pDesc->Width || m_desc.Height != pDesc->Height)
      m_presenter->setSurfaceExtent({ m_desc.Width, m_desc.Height });

    m_desc = *pDesc;
    CreateBackBuffers();
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetPresentRegion(
    const RECT*                     pRegion) {
    // TODO implement
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetGammaControl(
          UINT                      NumControlPoints,
    const DXGI_RGB*                 pControlPoints) {
    bool isIdentity = true;

    if (NumControlPoints > 1) {
      std::array<DxvkGammaCp, 1025> cp;

      if (NumControlPoints > cp.size())
        return E_INVALIDARG;
      
      for (uint32_t i = 0; i < NumControlPoints; i++) {
        uint16_t identity = MapGammaControlPoint(float(i) / float(NumControlPoints - 1));

        cp[i].r = MapGammaControlPoint(pControlPoints[i].Red);
        cp[i].g = MapGammaControlPoint(pControlPoints[i].Green);
        cp[i].b = MapGammaControlPoint(pControlPoints[i].Blue);
        cp[i].a = 0;

        isIdentity &= cp[i].r == identity
                   && cp[i].g == identity
                   && cp[i].b == identity;
      }

      if (!isIdentity)
        m_blitter->setGammaRamp(NumControlPoints, cp.data());
    }

    if (isIdentity)
      m_blitter->setGammaRamp(0, nullptr);

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetFrameLatency(
          UINT                      MaxLatency) {
    if (MaxLatency == 0 || MaxLatency > DXGI_MAX_SWAP_CHAIN_BUFFERS)
      return DXGI_ERROR_INVALID_CALL;

    if (m_frameLatencyEvent) {
      // Windows DXGI does not seem to handle the case where the new maximum
      // latency is less than the current value, and some games relying on
      // this behaviour will hang if we attempt to decrement the semaphore.
      // Thus, only increment the semaphore as necessary.
      if (MaxLatency > m_frameLatency)
        ReleaseSemaphore(m_frameLatencyEvent, MaxLatency - m_frameLatency, nullptr);
    }

    m_frameLatency = MaxLatency;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::Present(
          UINT                      SyncInterval,
          UINT                      PresentFlags,
    const DXGI_PRESENT_PARAMETERS*  pPresentParameters) {
    HRESULT hr = S_OK;

    if (m_device->getDeviceStatus() != VK_SUCCESS)
      hr = DXGI_ERROR_DEVICE_RESET;

    if (PresentFlags & DXGI_PRESENT_TEST) {
      if (hr != S_OK)
        return hr;

      VkResult status = m_presenter->checkSwapChainStatus();
      return status == VK_SUCCESS ? S_OK : DXGI_STATUS_OCCLUDED;
    }

    if (hr != S_OK) {
      SyncFrameLatency();
      return hr;
    }

    try {
      hr = PresentImage(SyncInterval);
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      hr = E_FAIL;
    }

    // Ensure to synchronize and release the frame latency semaphore
    // even if presentation failed with STATUS_OCCLUDED, or otherwise
    // applications using the semaphore may deadlock. This works because
    // we do not increment the frame ID in those situations.
    SyncFrameLatency();

    // Ignore latency stuff if presentation failed
    DxvkLatencyStats latencyStats = { };

    if (hr == S_OK && m_latency) {
      latencyStats = m_latency->getStatistics(m_frameId);
      m_latency->sleepAndBeginFrame(m_frameId + 1, std::abs(m_targetFrameRate));
    }

    if (m_latencyHud)
      m_latencyHud->accumulateStats(latencyStats);

    return hr;
  }


  UINT STDMETHODCALLTYPE D3D11SwapChain::CheckColorSpaceSupport(
          DXGI_COLOR_SPACE_TYPE     ColorSpace) {
    UINT supportFlags = 0;

    VkColorSpaceKHR vkColorSpace = ConvertColorSpace(ColorSpace);

    if (m_presenter->supportsColorSpace(vkColorSpace))
      supportFlags |= DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT;

    return supportFlags;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetColorSpace(
          DXGI_COLOR_SPACE_TYPE     ColorSpace) {
    VkColorSpaceKHR colorSpace = ConvertColorSpace(ColorSpace);

    if (!m_presenter->supportsColorSpace(colorSpace))
      return E_INVALIDARG;

    m_colorSpace = colorSpace;

    m_presenter->setSurfaceFormat(GetSurfaceFormat(m_desc.Format));
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetHDRMetaData(
    const DXGI_VK_HDR_METADATA*     pMetaData) {
    // For some reason this call always seems to succeed on Windows
    if (pMetaData->Type == DXGI_HDR_METADATA_TYPE_HDR10)
      m_presenter->setHdrMetadata(ConvertHDRMetadata(pMetaData->HDR10));

    return S_OK;
  }


  void STDMETHODCALLTYPE D3D11SwapChain::GetLastPresentCount(
          UINT64*                   pLastPresentCount) {
    *pLastPresentCount = UINT64(m_frameId - DXGI_MAX_SWAP_CHAIN_BUFFERS);
  }


  void STDMETHODCALLTYPE D3D11SwapChain::GetFrameStatistics(
          DXGI_VK_FRAME_STATISTICS* pFrameStatistics) {
    std::lock_guard<dxvk::mutex> lock(m_frameStatisticsLock);
    *pFrameStatistics = m_frameStatistics;
  }


  void STDMETHODCALLTYPE D3D11SwapChain::SetTargetFrameRate(
          double                    FrameRate) {
    m_targetFrameRate = FrameRate;

    if (m_presenter != nullptr)
      m_presenter->setFrameRateLimit(m_targetFrameRate, GetActualFrameLatency());
  }


  Rc<DxvkImageView> D3D11SwapChain::GetBackBufferView() {
    Rc<DxvkImage> image = GetCommonTexture(m_backBuffers[0].ptr())->GetImage();

    DxvkImageViewKey key;
    key.viewType = VK_IMAGE_VIEW_TYPE_2D;
    key.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    key.format = image->info().format;
    key.aspects = VK_IMAGE_ASPECT_COLOR_BIT;
    key.mipIndex = 0u;
    key.mipCount = 1u;
    key.layerIndex = 0u;
    key.layerCount = 1u;

    return image->createView(key);
  }


  HRESULT D3D11SwapChain::PresentImage(UINT SyncInterval) {
    // Flush pending rendering commands before
    auto immediateContext = m_parent->GetContext();
    auto immediateContextLock = immediateContext->LockContext();

    immediateContext->EndFrame(m_latency);
    immediateContext->ExecuteFlush(GpuFlushType::ExplicitFlush, nullptr, true);

    m_presenter->setSyncInterval(SyncInterval);

    // Presentation semaphores and WSI swap chain image
    if (m_latency)
      m_latency->notifyCpuPresentBegin(m_frameId + 1u);

    PresenterSync sync;
    Rc<DxvkImage> backBuffer;

    VkResult status = m_presenter->acquireNextImage(sync, backBuffer);

    if (status != VK_SUCCESS && m_latency)
      m_latency->discardTimings();

    if (status < 0)
      return E_FAIL;

    if (status == VK_NOT_READY)
      return DXGI_STATUS_OCCLUDED;

    m_frameId += 1;

    // Present from CS thread so that we don't
    // have to synchronize with it first.
    DxvkImageViewKey viewInfo = { };
    viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.usage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    viewInfo.format     = backBuffer->info().format;
    viewInfo.aspects    = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.mipIndex   = 0u;
    viewInfo.mipCount   = 1u;
    viewInfo.layerIndex = 0u;
    viewInfo.layerCount = 1u;

    immediateContext->EmitCs([
      cDevice         = m_device,
      cBlitter        = m_blitter,
      cBackBuffer     = backBuffer->createView(viewInfo),
      cSwapImage      = GetBackBufferView(),
      cSync           = sync,
      cPresenter      = m_presenter,
      cLatency        = m_latency,
      cColorSpace     = m_colorSpace,
      cFrameId        = m_frameId
    ] (DxvkContext* ctx) {
      // Update back buffer color space as necessary
      if (cSwapImage->image()->info().colorSpace != cColorSpace) {
        DxvkImageUsageInfo usage = { };
        usage.colorSpace = cColorSpace;

        ctx->ensureImageCompatibility(cSwapImage->image(), usage);
      }

      // Blit the D3D back buffer onto the actual Vulkan
      // swap chain and render the HUD if we have one.
      auto contextObjects = ctx->beginExternalRendering();

      cBlitter->present(contextObjects,
        cBackBuffer, VkRect2D(),
        cSwapImage, VkRect2D());

      // Submit current command list and present
      ctx->synchronizeWsi(cSync);
      ctx->flushCommandList(nullptr, nullptr);

      cDevice->presentImage(cPresenter, cLatency, cFrameId, nullptr);
    });

    if (m_backBuffers.size() > 1u)
      RotateBackBuffers(immediateContext);

    immediateContext->FlushCsChunk();

    if (m_latency) {
      m_latency->notifyCpuPresentEnd(m_frameId);

      if (m_latency->needsAutoMarkers()) {
        immediateContext->EmitCs([
          cLatency = m_latency,
          cFrameId = m_frameId
        ] (DxvkContext* ctx) {
          ctx->beginLatencyTracking(cLatency, cFrameId + 1u);
        });
      }
    }

    return S_OK;
  }


  void D3D11SwapChain::RotateBackBuffers(D3D11ImmediateContext* ctx) {
    small_vector<Rc<DxvkImage>, 4> images;

    for (uint32_t i = 0; i < m_backBuffers.size(); i++)
      images.push_back(GetCommonTexture(m_backBuffers[i].ptr())->GetImage());

    ctx->EmitCs([
      cImages = std::move(images)
    ] (DxvkContext* ctx) {
      auto allocation = cImages[0]->storage();

      for (size_t i = 0u; i + 1 < cImages.size(); i++)
        ctx->invalidateImage(cImages[i], cImages[i + 1]->storage());

      ctx->invalidateImage(cImages[cImages.size() - 1u], std::move(allocation));
    });
  }


  void D3D11SwapChain::CreateFrameLatencyEvent() {
    m_frameLatencySignal = new sync::CallbackFence(m_frameId);

    if (m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
      m_frameLatencyEvent = CreateSemaphore(nullptr, m_frameLatency, DXGI_MAX_SWAP_CHAIN_BUFFERS, nullptr);
  }


  void D3D11SwapChain::CreatePresenter() {
    PresenterDesc presenterDesc = { };
    presenterDesc.deferSurfaceCreation = m_parent->GetOptions()->deferSurfaceCreation;

    m_presenter = new Presenter(m_device, m_frameLatencySignal, presenterDesc, [
      cAdapter  = m_device->adapter(),
      cFactory  = m_surfaceFactory
    ] (VkSurfaceKHR* surface) {
      return cFactory->CreateSurface(
        cAdapter->vki()->instance(),
        cAdapter->handle(), surface);
    });

    m_presenter->setSurfaceFormat(GetSurfaceFormat(m_desc.Format));
    m_presenter->setSurfaceExtent({ m_desc.Width, m_desc.Height });
    m_presenter->setFrameRateLimit(m_targetFrameRate, GetActualFrameLatency());

    m_latency = m_device->createLatencyTracker(m_presenter);

    Com<D3D11ReflexDevice> reflex = GetReflexDevice();
    reflex->RegisterLatencyTracker(m_latency);
  }


  void D3D11SwapChain::CreateBackBuffers() {
    // Explicitly destroy current swap image before
    // creating a new one to free up resources
    m_backBuffers.clear();

    bool sequential = m_desc.SwapEffect == DXGI_SWAP_EFFECT_SEQUENTIAL ||
                      m_desc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    uint32_t backBufferCount = sequential ? m_desc.BufferCount : 1u;

    // Create new back buffer
    D3D11_COMMON_TEXTURE_DESC desc;
    desc.Width              = std::max(m_desc.Width,  1u);
    desc.Height             = std::max(m_desc.Height, 1u);
    desc.Depth              = 1;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = m_desc.Format;
    desc.SampleDesc         = m_desc.SampleDesc;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = 0;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = 0;
    desc.TextureLayout      = D3D11_TEXTURE_LAYOUT_UNDEFINED;

    if (m_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT)
      desc.BindFlags |= D3D11_BIND_RENDER_TARGET;

    if (m_desc.BufferUsage & DXGI_USAGE_SHADER_INPUT)
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    if (m_desc.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
      desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    
    if (m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE)
      desc.MiscFlags |= D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
    
    DXGI_USAGE dxgiUsage = DXGI_USAGE_BACK_BUFFER;

    for (uint32_t i = 0; i < backBufferCount; i++) {
      if (m_desc.SwapEffect == DXGI_SWAP_EFFECT_DISCARD
       || m_desc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD)
         dxgiUsage |= DXGI_USAGE_DISCARD_ON_PRESENT;

      m_backBuffers.push_back(new D3D11Texture2D(
        m_parent, this, &desc, dxgiUsage));

      dxgiUsage |= DXGI_USAGE_READ_ONLY;
    }

    small_vector<Rc<DxvkImage>, 4> images;

    for (uint32_t i = 0; i < backBufferCount; i++)
      images.push_back(GetCommonTexture(m_backBuffers[i].ptr())->GetImage());

    // Initialize images so that we can use them. Clearing
    // to black prevents garbled output for the first frame.
    m_parent->GetContext()->InjectCs(DxvkCsQueue::HighPriority, [
      cImages = std::move(images)
    ] (DxvkContext* ctx) {
      for (size_t i = 0; i < cImages.size(); i++) {
        ctx->setDebugName(cImages[i], str::format("Back buffer ", i).c_str());
        ctx->initImage(cImages[i], VK_IMAGE_LAYOUT_UNDEFINED);
      }
    });
  }


  void D3D11SwapChain::CreateBlitter() {
    Rc<hud::Hud> hud = hud::Hud::createHud(m_device);

    if (hud) {
      hud->addItem<hud::HudClientApiItem>("api", 1, GetApiName());

      if (m_latency)
        m_latencyHud = hud->addItem<hud::HudLatencyItem>("latency", 4);
    }

    m_blitter = new DxvkSwapchainBlitter(m_device, std::move(hud));
  }


  void D3D11SwapChain::DestroyFrameLatencyEvent() {
    CloseHandle(m_frameLatencyEvent);
  }


  void D3D11SwapChain::DestroyLatencyTracker() {
    // Need to make sure the context stops using
    // the tracker for submissions
    m_parent->GetContext()->InjectCs(DxvkCsQueue::Ordered, [
      cLatency = m_latency
    ] (DxvkContext* ctx) {
      ctx->endLatencyTracking(cLatency);
    });

    Com<D3D11ReflexDevice> reflex = GetReflexDevice();
    reflex->UnregisterLatencyTracker(m_latency);
  }


  void D3D11SwapChain::SyncFrameLatency() {
    // Wait for the sync event so that we respect the maximum frame latency
    m_frameLatencySignal->wait(m_frameId - GetActualFrameLatency());

    m_frameLatencySignal->setCallback(m_frameId, [this,
      cFrameId           = m_frameId,
      cFrameLatencyEvent = m_frameLatencyEvent
    ] () {
      if (cFrameLatencyEvent)
        ReleaseSemaphore(cFrameLatencyEvent, 1, nullptr);

      std::lock_guard<dxvk::mutex> lock(m_frameStatisticsLock);
      m_frameStatistics.PresentCount = cFrameId - DXGI_MAX_SWAP_CHAIN_BUFFERS;
      m_frameStatistics.PresentQPCTime = dxvk::high_resolution_clock::get_counter();
    });
  }


  uint32_t D3D11SwapChain::GetActualFrameLatency() {
    // DXGI does not seem to implicitly synchronize waitable swap chains,
    // so in that case we should just respect the user config. For regular
    // swap chains, pick the latency from the DXGI device.
    uint32_t maxFrameLatency = DXGI_MAX_SWAP_CHAIN_BUFFERS;

    if (!(m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT))
      m_dxgiDevice->GetMaximumFrameLatency(&maxFrameLatency);

    if (m_frameLatencyCap)
      maxFrameLatency = std::min(maxFrameLatency, m_frameLatencyCap);

    maxFrameLatency = std::min(maxFrameLatency, m_desc.BufferCount);
    return maxFrameLatency;
  }


  VkSurfaceFormatKHR D3D11SwapChain::GetSurfaceFormat(DXGI_FORMAT Format) {
    switch (Format) {
      default:
        Logger::warn(str::format("D3D11SwapChain: Unexpected format: ", m_desc.Format));
        [[fallthrough]];

      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM:
        return { VK_FORMAT_R8G8B8A8_UNORM, m_colorSpace };

      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return { VK_FORMAT_R8G8B8A8_SRGB, m_colorSpace };

      case DXGI_FORMAT_R10G10B10A2_UNORM:
        return { VK_FORMAT_A2B10G10R10_UNORM_PACK32, m_colorSpace };

      case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return { VK_FORMAT_R16G16B16A16_SFLOAT, m_colorSpace };
    }
  }


  Com<D3D11ReflexDevice> D3D11SwapChain::GetReflexDevice() {
    Com<ID3DLowLatencyDevice> llDevice;
    m_parent->QueryInterface(__uuidof(ID3DLowLatencyDevice), reinterpret_cast<void**>(&llDevice));

    return static_cast<D3D11ReflexDevice*>(llDevice.ptr());
  }


  std::string D3D11SwapChain::GetApiName() const {
    Com<IDXGIDXVKDevice> device;
    m_parent->QueryInterface(__uuidof(IDXGIDXVKDevice), reinterpret_cast<void**>(&device));

    uint32_t apiVersion = device->GetAPIVersion();
    uint32_t featureLevel = m_parent->GetFeatureLevel();

    uint32_t flHi = (featureLevel >> 12);
    uint32_t flLo = (featureLevel >> 8) & 0x7;

    return str::format("D3D", apiVersion, " FL", flHi, "_", flLo);
  }

}
