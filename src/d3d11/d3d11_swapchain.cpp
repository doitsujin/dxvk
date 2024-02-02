#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_swapchain.h"

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
    m_context(m_device->createContext(DxvkContextType::Supplementary)),
    m_frameLatencyCap(pDevice->GetOptions()->maxFrameLatency) {
    CreateFrameLatencyEvent();
    CreatePresenter();
    CreateBackBuffer();
    CreateBlitter();
    CreateHud();

    if (!pDevice->GetOptions()->deferSurfaceCreation)
      RecreateSwapChain();
  }


  D3D11SwapChain::~D3D11SwapChain() {
    // Avoids hanging when in this state, see comment
    // in DxvkDevice::~DxvkDevice.
    if (this_thread::isInModuleDetachment())
      return;

    m_device->waitForSubmission(&m_presentStatus);
    m_device->waitForIdle();
    
    DestroyFrameLatencyEvent();
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIVkSwapChain)
     || riid == __uuidof(IDXGIVkSwapChain1)) {
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

    if (BufferId > 0) {
      Logger::err("D3D11: GetImage: BufferId > 0 not supported");
      return DXGI_ERROR_UNSUPPORTED;
    }

    return m_backBuffer->QueryInterface(riid, ppBuffer);
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
    m_dirty |= m_desc.Format      != pDesc->Format
            || m_desc.Width       != pDesc->Width
            || m_desc.Height      != pDesc->Height
            || m_desc.BufferCount != pDesc->BufferCount
            || m_desc.Flags       != pDesc->Flags;

    m_desc = *pDesc;
    CreateBackBuffer();
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
    if (!(PresentFlags & DXGI_PRESENT_TEST))
      m_dirty |= m_presenter->setSyncInterval(SyncInterval) != VK_SUCCESS;

    HRESULT hr = S_OK;

    if (!m_presenter->hasSwapChain()) {
      RecreateSwapChain();
      m_dirty = false;
    }

    if (!m_presenter->hasSwapChain())
      hr = DXGI_STATUS_OCCLUDED;

    if (m_device->getDeviceStatus() != VK_SUCCESS)
      hr = DXGI_ERROR_DEVICE_RESET;

    if (PresentFlags & DXGI_PRESENT_TEST)
      return hr;

    if (hr != S_OK) {
      SyncFrameLatency();
      return hr;
    }

    if (std::exchange(m_dirty, false))
      RecreateSwapChain();

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
    return hr;
  }


  UINT STDMETHODCALLTYPE D3D11SwapChain::CheckColorSpaceSupport(
          DXGI_COLOR_SPACE_TYPE     ColorSpace) {
    UINT supportFlags = 0;

    const VkColorSpaceKHR vkColorSpace = ConvertColorSpace(ColorSpace);
    if (m_presenter->supportsColorSpace(vkColorSpace))
      supportFlags |= DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT;

    return supportFlags;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetColorSpace(
          DXGI_COLOR_SPACE_TYPE     ColorSpace) {
    if (!(CheckColorSpaceSupport(ColorSpace) & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
      return E_INVALIDARG;

    const VkColorSpaceKHR vkColorSpace = ConvertColorSpace(ColorSpace);
    m_dirty |= vkColorSpace != m_colorspace;
    m_colorspace = vkColorSpace;

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetHDRMetaData(
    const DXGI_VK_HDR_METADATA*     pMetaData) {
    // For some reason this call always seems to succeed on Windows
    if (pMetaData->Type == DXGI_HDR_METADATA_TYPE_HDR10) {
      m_hdrMetadata = ConvertHDRMetadata(pMetaData->HDR10);
      m_dirtyHdrMetadata = true;
    }

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


  HRESULT D3D11SwapChain::PresentImage(UINT SyncInterval) {
    // Flush pending rendering commands before
    auto immediateContext = m_parent->GetContext();
    immediateContext->EndFrame();
    immediateContext->Flush();

    for (uint32_t i = 0; i < SyncInterval || i < 1; i++) {
      SynchronizePresent();

      if (!m_presenter->hasSwapChain())
        return i ? S_OK : DXGI_STATUS_OCCLUDED;

      // Presentation semaphores and WSI swap chain image
      PresenterInfo info = m_presenter->info();
      PresenterSync sync;

      uint32_t imageIndex = 0;

      VkResult status = m_presenter->acquireNextImage(sync, imageIndex);

      while (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR) {
        RecreateSwapChain();

        if (!m_presenter->hasSwapChain())
          return i ? S_OK : DXGI_STATUS_OCCLUDED;
        
        info = m_presenter->info();
        status = m_presenter->acquireNextImage(sync, imageIndex);
      }

      if (m_hdrMetadata && m_dirtyHdrMetadata) {
        m_presenter->setHdrMetadata(*m_hdrMetadata);
        m_dirtyHdrMetadata = false;
      }

      m_context->beginRecording(
        m_device->createCommandList());
      
      m_blitter->presentImage(m_context.ptr(),
        m_imageViews.at(imageIndex), VkRect2D(),
        m_swapImageView, VkRect2D());

      if (m_hud != nullptr)
        m_hud->render(m_context, info.format, info.imageExtent);
      
      SubmitPresent(immediateContext, sync, i);
    }

    return S_OK;
  }


  void D3D11SwapChain::SubmitPresent(
          D3D11ImmediateContext*  pContext,
    const PresenterSync&          Sync,
          uint32_t                Repeat) {
    auto lock = pContext->LockContext();

    // Bump frame ID as necessary
    if (!Repeat)
      m_frameId += 1;

    // Present from CS thread so that we don't
    // have to synchronize with it first.
    m_presentStatus.result = VK_NOT_READY;

    pContext->EmitCs([this,
      cRepeat      = Repeat,
      cSync        = Sync,
      cHud         = m_hud,
      cPresentMode = m_presenter->info().presentMode,
      cFrameId     = m_frameId,
      cCommandList = m_context->endRecording()
    ] (DxvkContext* ctx) {
      cCommandList->setWsiSemaphores(cSync);
      m_device->submitCommandList(cCommandList, nullptr);

      if (cHud != nullptr && !cRepeat)
        cHud->update();

      uint64_t frameId = cRepeat ? 0 : cFrameId;

      m_device->presentImage(m_presenter,
        cPresentMode, frameId, &m_presentStatus);
    });

    pContext->FlushCsChunk();
  }


  void D3D11SwapChain::SynchronizePresent() {
    // Recreate swap chain if the previous present call failed
    VkResult status = m_device->waitForSubmission(&m_presentStatus);
    
    if (status != VK_SUCCESS)
      RecreateSwapChain();
  }


  void D3D11SwapChain::RecreateSwapChain() {
    // Ensure that we can safely destroy the swap chain
    m_device->waitForSubmission(&m_presentStatus);
    m_device->waitForIdle();

    m_presentStatus.result = VK_SUCCESS;
    m_dirtyHdrMetadata = true;

    PresenterDesc presenterDesc;
    presenterDesc.imageExtent     = { m_desc.Width, m_desc.Height };
    presenterDesc.imageCount      = PickImageCount(m_desc.BufferCount + 1);
    presenterDesc.numFormats      = PickFormats(m_desc.Format, presenterDesc.formats);
    presenterDesc.fullScreenExclusive = PickFullscreenMode();

    VkResult vr = m_presenter->recreateSwapChain(presenterDesc);

    if (vr == VK_ERROR_SURFACE_LOST_KHR) {
      vr = m_presenter->recreateSurface([this] (VkSurfaceKHR* surface) {
        return CreateSurface(surface);
      });

      if (vr)
        throw DxvkError(str::format("D3D11SwapChain: Failed to recreate surface: ", vr));

      vr = m_presenter->recreateSwapChain(presenterDesc);
    }

    if (vr)
      throw DxvkError(str::format("D3D11SwapChain: Failed to recreate swap chain: ", vr));
    
    CreateRenderTargetViews();
  }


  void D3D11SwapChain::CreateFrameLatencyEvent() {
    m_frameLatencySignal = new sync::CallbackFence(m_frameId);

    if (m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
      m_frameLatencyEvent = CreateSemaphore(nullptr, m_frameLatency, DXGI_MAX_SWAP_CHAIN_BUFFERS, nullptr);
  }


  void D3D11SwapChain::CreatePresenter() {
    PresenterDesc presenterDesc;
    presenterDesc.imageExtent     = { m_desc.Width, m_desc.Height };
    presenterDesc.imageCount      = PickImageCount(m_desc.BufferCount + 1);
    presenterDesc.numFormats      = PickFormats(m_desc.Format, presenterDesc.formats);
    presenterDesc.fullScreenExclusive = PickFullscreenMode();

    m_presenter = new Presenter(m_device, m_frameLatencySignal, presenterDesc);
    m_presenter->setFrameRateLimit(m_parent->GetOptions()->maxFrameRate);
  }


  VkResult D3D11SwapChain::CreateSurface(VkSurfaceKHR* pSurface) {
    Rc<DxvkAdapter> adapter = m_device->adapter();

    return m_surfaceFactory->CreateSurface(
      adapter->vki()->instance(),
      adapter->handle(), pSurface);
  }


  void D3D11SwapChain::CreateRenderTargetViews() {
    PresenterInfo info = m_presenter->info();

    m_imageViews.clear();
    m_imageViews.resize(info.imageCount);

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
      VkImage imageHandle = m_presenter->getImage(i).image;
      
      Rc<DxvkImage> image = new DxvkImage(
        m_device.ptr(), imageInfo, imageHandle,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      m_imageViews[i] = new DxvkImageView(
        m_device->vkd(), image, viewInfo);
    }
  }


  void D3D11SwapChain::CreateBackBuffer() {
    // Explicitly destroy current swap image before
    // creating a new one to free up resources
    m_swapImage         = nullptr;
    m_swapImageView     = nullptr;
    m_backBuffer        = nullptr;

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

    if (m_desc.SwapEffect == DXGI_SWAP_EFFECT_DISCARD
     || m_desc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD)
      dxgiUsage |= DXGI_USAGE_DISCARD_ON_PRESENT;

    m_backBuffer = new D3D11Texture2D(m_parent, this, &desc, dxgiUsage);
    m_swapImage = GetCommonTexture(m_backBuffer.ptr())->GetImage();

    // Create an image view that allows the
    // image to be bound as a shader resource.
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format     = m_swapImage->info().format;
    viewInfo.usage      = VK_IMAGE_USAGE_SAMPLED_BIT;
    viewInfo.aspect     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel   = 0;
    viewInfo.numLevels  = 1;
    viewInfo.minLayer   = 0;
    viewInfo.numLayers  = 1;
    m_swapImageView = m_device->createImageView(m_swapImage, viewInfo);
    
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
    
    m_context->initImage(m_swapImage,
      subresources, VK_IMAGE_LAYOUT_UNDEFINED);

    m_device->submitCommandList(
      m_context->endRecording(),
      nullptr);
  }


  void D3D11SwapChain::CreateBlitter() {
    m_blitter = new DxvkSwapchainBlitter(m_device);    
  }


  void D3D11SwapChain::CreateHud() {
    m_hud = hud::Hud::createHud(m_device);

    if (m_hud != nullptr)
      m_hud->addItem<hud::HudClientApiItem>("api", 1, GetApiName());
  }


  void D3D11SwapChain::DestroyFrameLatencyEvent() {
    CloseHandle(m_frameLatencyEvent);
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


  uint32_t D3D11SwapChain::PickFormats(
          DXGI_FORMAT               Format,
          VkSurfaceFormatKHR*       pDstFormats) {
    uint32_t n = 0;

    switch (Format) {
      default:
        Logger::warn(str::format("D3D11SwapChain: Unexpected format: ", m_desc.Format));
      [[fallthrough]];
      
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM: {
        pDstFormats[n++] = { VK_FORMAT_R8G8B8A8_UNORM, m_colorspace };
        pDstFormats[n++] = { VK_FORMAT_B8G8R8A8_UNORM, m_colorspace };
      } break;
      
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: {
        pDstFormats[n++] = { VK_FORMAT_R8G8B8A8_SRGB, m_colorspace };
        pDstFormats[n++] = { VK_FORMAT_B8G8R8A8_SRGB, m_colorspace };
      } break;
      
      case DXGI_FORMAT_R10G10B10A2_UNORM: {
        pDstFormats[n++] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, m_colorspace };
        pDstFormats[n++] = { VK_FORMAT_A2R10G10B10_UNORM_PACK32, m_colorspace };
      } break;
      
      case DXGI_FORMAT_R16G16B16A16_FLOAT: {
        pDstFormats[n++] = { VK_FORMAT_R16G16B16A16_SFLOAT, m_colorspace };
      } break;
    }

    return n;
  }


  uint32_t D3D11SwapChain::PickImageCount(
          UINT                      Preferred) {
    int32_t option = m_parent->GetOptions()->numBackBuffers;
    return option > 0 ? uint32_t(option) : uint32_t(Preferred);
  }


  VkFullScreenExclusiveEXT D3D11SwapChain::PickFullscreenMode() {
    return m_desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
      ? VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT
      : VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT;
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
