#include "d3d11_context_imm.h"
#include "d3d11_device.h"
#include "d3d11_swapchain.h"

#include <dxgi_presenter_frag.h>
#include <dxgi_presenter_vert.h>

namespace dxvk {

  static uint16_t MapGammaControlPoint(float x) {
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    return uint16_t(65535.0f * x);
  }


  D3D11SwapChain::D3D11SwapChain(
          D3D11Device*            pDevice,
          HWND                    hWnd,
    const DXGI_SWAP_CHAIN_DESC1*  pDesc)
  : m_parent  (pDevice),
    m_window  (hWnd),
    m_desc    (*pDesc),
    m_device  (pDevice->GetDXVKDevice()),
    m_context (m_device->createContext()) {
    
    if (FAILED(pDevice->QueryInterface(__uuidof(IDXGIVkDevice),
        reinterpret_cast<void**>(&m_dxgiDevice))))
      throw DxvkError("D3D11: Incompatible device for swap chain");
    
    if (!pDevice->GetOptions()->deferSurfaceCreation)
      CreateSurface();
    
    CreateBackBuffer();
    CreateHud();
    
    InitRenderState();
    InitSamplers();
    InitShaders();

    SetGammaControl(0, nullptr);
  }


  D3D11SwapChain::~D3D11SwapChain() {
    m_device->waitForIdle();
    
    if (m_backBuffer)
      m_backBuffer->ReleasePrivate();
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIVkSwapChain)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D11SwapChain::QueryInterface: Unknown interface query");
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
    Com<IDXGIDevice> dxgiDevice;

    HRESULT hr = GetDevice(__uuidof(IDXGIDevice),
      reinterpret_cast<void**>(&dxgiDevice));

    if (FAILED(hr))
      return hr;
    
    return dxgiDevice->GetParent(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetDevice(
          REFIID                    riid,
          void**                    ppDevice) {
    return m_parent->QueryInterface(riid, ppDevice);
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


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::ChangeProperties(
    const DXGI_SWAP_CHAIN_DESC1*  pDesc) {

    m_dirty |= m_desc.Format      != pDesc->Format
            || m_desc.Width       != pDesc->Width
            || m_desc.Height      != pDesc->Height
            || m_desc.BufferCount != pDesc->BufferCount;

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
    if (NumControlPoints > 0) {
      std::array<D3D11_VK_GAMMA_CP, 1025> cp;

      if (NumControlPoints > cp.size())
        return E_INVALIDARG;
      
      for (uint32_t i = 0; i < NumControlPoints; i++) {
        cp[i].R = MapGammaControlPoint(pControlPoints[i].Red);
        cp[i].G = MapGammaControlPoint(pControlPoints[i].Green);
        cp[i].B = MapGammaControlPoint(pControlPoints[i].Blue);
        cp[i].A = 0;
      }

      CreateGammaTexture(NumControlPoints, cp.data());
    } else {
      std::array<D3D11_VK_GAMMA_CP, 256> cp;

      for (uint32_t i = 0; i < cp.size(); i++) {
        const uint16_t value = 257 * i;
        cp[i] = { value, value, value, 0 };
      }

      CreateGammaTexture(cp.size(), cp.data());
    }

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11SwapChain::Present(
          UINT                      SyncInterval,
          UINT                      PresentFlags,
    const DXGI_PRESENT_PARAMETERS*  pPresentParameters) {
    auto options = m_parent->GetOptions();

    if (options->syncInterval >= 0)
      SyncInterval = options->syncInterval;
    
    bool vsync = SyncInterval != 0;

    m_dirty |= vsync != m_vsync;
    m_vsync  = vsync;

    if (std::exchange(m_dirty, false))
      CreateSwapChain();
    
    FlushImmediateContext();
    PresentImage(SyncInterval);
    return S_OK;
  }


  void D3D11SwapChain::PresentImage(UINT SyncInterval) {
    // Wait for the sync event so that we
    // respect the maximum frame latency
    Rc<DxvkEvent> syncEvent = m_dxgiDevice->GetFrameSyncEvent();
    syncEvent->wait();
    
    if (m_hud != nullptr)
      m_hud->update();

    for (uint32_t i = 0; i < SyncInterval || i < 1; i++) {
      m_context->beginRecording(
        m_device->createCommandList());
      
      // Resolve back buffer if it is multisampled. We
      // only have to do it only for the first frame.
      if (m_swapImageResolve != nullptr && i == 0) {
        VkImageSubresourceLayers resolveSubresources;
        resolveSubresources.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        resolveSubresources.mipLevel        = 0;
        resolveSubresources.baseArrayLayer  = 0;
        resolveSubresources.layerCount      = 1;
        
        m_context->resolveImage(
          m_swapImageResolve, resolveSubresources,
          m_swapImage,        resolveSubresources,
          VK_FORMAT_UNDEFINED);
      }
      
      // Presentation semaphores and WSI swap chain image
      auto wsiSemas = m_swapchain->getSemaphorePair();
      auto wsiImage = m_swapchain->getImageView(wsiSemas.acquireSync);

      // Use an appropriate texture filter depending on whether
      // the back buffer size matches the swap image size
      bool fitSize = m_swapImage->info().extent == wsiImage->imageInfo().extent;

      m_context->bindShader(VK_SHADER_STAGE_VERTEX_BIT,   m_vertShader);
      m_context->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader);

      DxvkRenderTargets renderTargets;
      renderTargets.color[0].view   = wsiImage;
      renderTargets.color[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      m_context->bindRenderTargets(renderTargets, false);

      VkViewport viewport;
      viewport.x        = 0.0f;
      viewport.y        = 0.0f;
      viewport.width    = float(wsiImage->imageInfo().extent.width);
      viewport.height   = float(wsiImage->imageInfo().extent.height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      
      VkRect2D scissor;
      scissor.offset.x      = 0;
      scissor.offset.y      = 0;
      scissor.extent.width  = wsiImage->imageInfo().extent.width;
      scissor.extent.height = wsiImage->imageInfo().extent.height;

      m_context->setViewports(1, &viewport, &scissor);

      m_context->setRasterizerState(m_rsState);
      m_context->setMultisampleState(m_msState);
      m_context->setDepthStencilState(m_dsState);
      m_context->setLogicOpState(m_loState);
      m_context->setBlendMode(0, m_blendMode);
      
      m_context->setInputAssemblyState(m_iaState);
      m_context->setInputLayout(0, nullptr, 0, nullptr);

      m_context->bindResourceSampler(BindingIds::Sampler, fitSize ? m_samplerFitting : m_samplerScaling);
      m_context->bindResourceSampler(BindingIds::GammaSmp, m_gammaSampler);

      m_context->bindResourceView(BindingIds::Texture, m_swapImageView, nullptr);
      m_context->bindResourceView(BindingIds::GammaTex, m_gammaTextureView, nullptr);

      m_context->draw(4, 1, 0, 0);

      VkExtent2D hudSize = {
        wsiImage->imageInfo().extent.width,
        wsiImage->imageInfo().extent.height };

      if (m_hud != nullptr)
        m_hud->render(m_context, hudSize);
      
      if (i + 1 >= SyncInterval) {
        DxvkEventRevision eventRev;
        eventRev.event    = syncEvent;
        eventRev.revision = syncEvent->reset();
        m_context->signalEvent(eventRev);
      }

      m_device->submitCommandList(
        m_context->endRecording(),
        wsiSemas.acquireSync,
        wsiSemas.presentSync);
      
      m_swapchain->present(
        wsiSemas.presentSync);
    }
  }


  void D3D11SwapChain::FlushImmediateContext() {
    Com<ID3D11DeviceContext> deviceContext = nullptr;
    m_parent->GetImmediateContext(&deviceContext);
    
    // The presentation code is run from the main rendering thread
    // rather than the command stream thread, so we synchronize.
    auto immediateContext = static_cast<D3D11ImmediateContext*>(deviceContext.ptr());
    immediateContext->Flush();
    immediateContext->SynchronizeCsThread();
  }


  void D3D11SwapChain::CreateBackBuffer() {
    // Explicitly destroy current swap image before
    // creating a new one to free up resources
    if (m_backBuffer)
      m_backBuffer->ReleasePrivate();
    
    m_swapImage         = nullptr;
    m_swapImageResolve  = nullptr;
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
    desc.BindFlags          = D3D11_BIND_RENDER_TARGET
                            | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = 0;

    if (m_desc.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
      desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    
    m_backBuffer = new D3D11Texture2D(m_parent, &desc);
    m_backBuffer->AddRefPrivate();

    m_swapImage = GetCommonTexture(m_backBuffer)->GetImage();

    // If the image is multisampled, we need to create
    // another image which we'll use as a resolve target
    if (m_swapImage->info().sampleCount != VK_SAMPLE_COUNT_1_BIT) {
      DxvkImageCreateInfo resolveInfo;
      resolveInfo.type          = VK_IMAGE_TYPE_2D;
      resolveInfo.format        = m_swapImage->info().format;
      resolveInfo.flags         = 0;
      resolveInfo.sampleCount   = VK_SAMPLE_COUNT_1_BIT;
      resolveInfo.extent        = m_swapImage->info().extent;
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
      
      m_swapImageResolve = m_device->createImage(
        resolveInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

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
    
    m_swapImageView = m_device->createImageView(
      m_swapImageResolve != nullptr
        ? m_swapImageResolve
        : m_swapImage,
      viewInfo);
    
    // Initialize the image so that we can use it. Clearing
    // to black prevents garbled output for the first frame.
    VkImageSubresourceRange subresources;
    subresources.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    subresources.baseMipLevel   = 0;
    subresources.levelCount     = 1;
    subresources.baseArrayLayer = 0;
    subresources.layerCount     = 1;

    VkClearColorValue clearColor;
    clearColor.float32[0] = 0.0f;
    clearColor.float32[1] = 0.0f;
    clearColor.float32[2] = 0.0f;
    clearColor.float32[3] = 0.0f;

    m_context->beginRecording(
      m_device->createCommandList());
    
    m_context->clearColorImage(
      m_swapImage, clearColor, subresources);

    m_device->submitCommandList(
      m_context->endRecording(),
      nullptr, nullptr);
  }


  void D3D11SwapChain::CreateGammaTexture(
            UINT                NumControlPoints,
      const D3D11_VK_GAMMA_CP*  pControlPoints) {
    if (m_gammaTexture == nullptr
     || m_gammaTexture->info().extent.width != NumControlPoints) {
      DxvkImageCreateInfo imgInfo;
      imgInfo.type        = VK_IMAGE_TYPE_1D;
      imgInfo.format      = VK_FORMAT_R16G16B16A16_UNORM;
      imgInfo.flags       = 0;
      imgInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
      imgInfo.extent      = { NumControlPoints, 1, 1 };
      imgInfo.numLayers   = 1;
      imgInfo.mipLevels   = 1;
      imgInfo.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                          | VK_IMAGE_USAGE_SAMPLED_BIT;
      imgInfo.stages      = VK_PIPELINE_STAGE_TRANSFER_BIT
                          | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      imgInfo.access      = VK_ACCESS_TRANSFER_WRITE_BIT
                          | VK_ACCESS_SHADER_READ_BIT;
      imgInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
      imgInfo.layout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      
      m_gammaTexture = m_device->createImage(
        imgInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      DxvkImageViewCreateInfo viewInfo;
      viewInfo.type       = VK_IMAGE_VIEW_TYPE_1D;
      viewInfo.format     = VK_FORMAT_R16G16B16A16_UNORM;
      viewInfo.usage      = VK_IMAGE_USAGE_SAMPLED_BIT;
      viewInfo.aspect     = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.minLevel   = 0;
      viewInfo.numLevels  = 1;
      viewInfo.minLayer   = 0;
      viewInfo.numLayers  = 1;
      
      m_gammaTextureView = m_device->createImageView(m_gammaTexture, viewInfo);
    }

    m_context->beginRecording(
      m_device->createCommandList());
    
    m_context->updateImage(m_gammaTexture,
      VkImageSubresourceLayers { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
      VkOffset3D { 0, 0, 0 },
      VkExtent3D { NumControlPoints, 1, 1 },
      pControlPoints, 0, 0);
    
    m_device->submitCommandList(
      m_context->endRecording(),
      nullptr, nullptr);
  }


  void D3D11SwapChain::CreateSurface() {
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(m_window, GWLP_HINSTANCE));
    
    m_surface = m_device->adapter()->createSurface(instance, m_window);
  }


  void D3D11SwapChain::CreateSwapChain() {
    auto options = m_parent->GetOptions();

    if (m_surface == nullptr)
      CreateSurface();
    
    DxvkSwapchainProperties swapInfo;
    swapInfo.preferredSurfaceFormat     = PickSurfaceFormat();
    swapInfo.preferredPresentMode       = PickPresentMode();
    swapInfo.preferredBufferSize.width  = m_desc.Width;
    swapInfo.preferredBufferSize.height = m_desc.Height;
    swapInfo.preferredBufferCount       = m_desc.BufferCount;

    if (options->numBackBuffers > 0)
      swapInfo.preferredBufferCount = options->numBackBuffers;

    if (m_swapchain == nullptr)
      m_swapchain = m_device->createSwapchain(m_surface, swapInfo);
    else
      m_swapchain->changeProperties(swapInfo);
  }


  void D3D11SwapChain::CreateHud() {
    m_hud = hud::Hud::createHud(m_device);
  }


  void D3D11SwapChain::InitRenderState() {
    m_iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    m_iaState.primitiveRestart  = VK_FALSE;
    m_iaState.patchVertexCount  = 0;
    
    m_rsState.polygonMode        = VK_POLYGON_MODE_FILL;
    m_rsState.cullMode           = VK_CULL_MODE_BACK_BIT;
    m_rsState.frontFace          = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    m_rsState.depthClampEnable   = VK_FALSE;
    m_rsState.depthBiasEnable    = VK_FALSE;
    m_rsState.depthBiasConstant  = 0.0f;
    m_rsState.depthBiasClamp     = 0.0f;
    m_rsState.depthBiasSlope     = 0.0f;
    
    m_msState.sampleMask            = 0xffffffff;
    m_msState.enableAlphaToCoverage = VK_FALSE;
    m_msState.enableAlphaToOne      = VK_FALSE;
    
    VkStencilOpState stencilOp;
    stencilOp.failOp      = VK_STENCIL_OP_KEEP;
    stencilOp.passOp      = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp   = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask = 0xFFFFFFFF;
    stencilOp.writeMask   = 0xFFFFFFFF;
    stencilOp.reference   = 0;
    
    m_dsState.enableDepthTest   = VK_FALSE;
    m_dsState.enableDepthWrite  = VK_FALSE;
    m_dsState.enableStencilTest = VK_FALSE;
    m_dsState.depthCompareOp    = VK_COMPARE_OP_ALWAYS;
    m_dsState.stencilOpFront    = stencilOp;
    m_dsState.stencilOpBack     = stencilOp;
    
    m_loState.enableLogicOp = VK_FALSE;
    m_loState.logicOp       = VK_LOGIC_OP_NO_OP;
    
    m_blendMode.enableBlending  = VK_FALSE;
    m_blendMode.colorSrcFactor  = VK_BLEND_FACTOR_ONE;
    m_blendMode.colorDstFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_blendMode.colorBlendOp    = VK_BLEND_OP_ADD;
    m_blendMode.alphaSrcFactor  = VK_BLEND_FACTOR_ONE;
    m_blendMode.alphaDstFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_blendMode.alphaBlendOp    = VK_BLEND_OP_ADD;
    m_blendMode.writeMask       = VK_COLOR_COMPONENT_R_BIT
                                | VK_COLOR_COMPONENT_G_BIT
                                | VK_COLOR_COMPONENT_B_BIT
                                | VK_COLOR_COMPONENT_A_BIT;
  }


  void D3D11SwapChain::InitSamplers() {
    DxvkSamplerCreateInfo samplerInfo;
    samplerInfo.magFilter       = VK_FILTER_NEAREST;
    samplerInfo.minFilter       = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode      = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipmapLodBias   = 0.0f;
    samplerInfo.mipmapLodMin    = 0.0f;
    samplerInfo.mipmapLodMax    = 0.0f;
    samplerInfo.useAnisotropy   = VK_FALSE;
    samplerInfo.maxAnisotropy   = 1.0f;
    samplerInfo.addressModeU    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.compareToDepth  = VK_FALSE;
    samplerInfo.compareOp       = VK_COMPARE_OP_ALWAYS;
    samplerInfo.borderColor     = VkClearColorValue();
    samplerInfo.usePixelCoord   = VK_FALSE;
    m_samplerFitting = m_device->createSampler(samplerInfo);

    samplerInfo.magFilter       = VK_FILTER_LINEAR;
    samplerInfo.minFilter       = VK_FILTER_LINEAR;
    m_samplerScaling = m_device->createSampler(samplerInfo);

    samplerInfo.addressModeU    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    m_gammaSampler = m_device->createSampler(samplerInfo);
  }


  void D3D11SwapChain::InitShaders() {
    const SpirvCodeBuffer vsCode(dxgi_presenter_vert);
    const SpirvCodeBuffer fsCode(dxgi_presenter_frag);
    
    const std::array<DxvkResourceSlot, 4> fsResourceSlots = {{
      { BindingIds::Sampler,  VK_DESCRIPTOR_TYPE_SAMPLER,        VK_IMAGE_VIEW_TYPE_MAX_ENUM },
      { BindingIds::Texture,  VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  VK_IMAGE_VIEW_TYPE_2D       },
      { BindingIds::GammaSmp, VK_DESCRIPTOR_TYPE_SAMPLER,        VK_IMAGE_VIEW_TYPE_MAX_ENUM },
      { BindingIds::GammaTex, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  VK_IMAGE_VIEW_TYPE_1D       },
    }};

    m_vertShader = m_device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      0, nullptr, { 0u, 1u },
      vsCode);
    
    m_fragShader = m_device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      fsResourceSlots.size(),
      fsResourceSlots.data(),
      { 1u, 1u }, fsCode);
  }

  
  VkSurfaceFormatKHR D3D11SwapChain::PickSurfaceFormat() const {
    std::array<VkSurfaceFormatKHR, 2> formats;
    size_t n = 0;

    switch (m_desc.Format) {
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM: {
        formats[n++] = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        formats[n++] = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      } break;
      
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: {
        formats[n++] = { VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        formats[n++] = { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      } break;
      
      case DXGI_FORMAT_R10G10B10A2_UNORM: {
        formats[n++] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        formats[n++] = { VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      } break;
      
      case DXGI_FORMAT_R16G16B16A16_FLOAT: {
        formats[n++] = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      } break;
      
      default:
        Logger::warn(str::format("DxgiVkPresenter: Unknown format: ", m_desc.Format));
    }
    
    return m_surface->pickSurfaceFormat(n, formats.data());
  }

  
  VkPresentModeKHR D3D11SwapChain::PickPresentMode() const {
    auto options = m_parent->GetOptions();
    
    std::array<VkPresentModeKHR, 4> modes;
    size_t n = 0;
    
    if (m_vsync) {
      if (options->syncMode == D3D11SwapChainSyncMode::Mailbox)
        modes[n++] = VK_PRESENT_MODE_MAILBOX_KHR;
      modes[n++] = VK_PRESENT_MODE_FIFO_KHR;
    } else {
      modes[n++] = VK_PRESENT_MODE_IMMEDIATE_KHR;
      modes[n++] = VK_PRESENT_MODE_MAILBOX_KHR;
      modes[n++] = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    }
    
    return m_surface->pickPresentMode(n, modes.data());
  }

}