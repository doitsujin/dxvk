#include "d3d9_presenter.h"

#include "dxgi_presenter_frag.h"
#include "dxgi_presenter_vert.h"

namespace dxvk {

  D3D9Presenter::D3D9Presenter(
        Direct3DDevice9Ex*  parent,
        HWND                window,
  const D3D9PresenterDesc*  desc,
        DWORD               gammaFlags,
  const D3DGAMMARAMP*       gammaRamp)
    : m_parent  ( parent )
    , m_window  ( window )
    , m_device  ( parent->GetDXVKDevice() )
    , m_context ( m_device->createContext() )
    , m_desc    ( *desc ) {
    createPresenter();

    createBackBuffer();
    createHud();

    initRenderState();
    initSamplers();
    initShaders();

    setGammaRamp(gammaFlags, gammaRamp);
  }

  void D3D9Presenter::setGammaRamp(
    DWORD               Flags,
    const D3DGAMMARAMP* pRamp) {
    std::array<D3D9_VK_GAMMA_CP, GammaPointCount> cp;

    for (uint32_t i = 0; i < GammaPointCount; i++) {
      cp[i].R = pRamp->red[i];
      cp[i].G = pRamp->green[i];
      cp[i].B = pRamp->blue[i];
      cp[i].A = 0;
    }

    createGammaTexture(cp.data());
  }

  void D3D9Presenter::createGammaTexture(const D3D9_VK_GAMMA_CP* pControlPoints) {
    if (m_gammaTexture == nullptr) {
      DxvkImageCreateInfo imgInfo;
      imgInfo.type = VK_IMAGE_TYPE_1D;
      imgInfo.format = VK_FORMAT_R16G16B16A16_UNORM;
      imgInfo.flags = 0;
      imgInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
      imgInfo.extent = { GammaPointCount, 1, 1 };
      imgInfo.numLayers = 1;
      imgInfo.mipLevels = 1;
      imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT;
      imgInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
        | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      imgInfo.access = VK_ACCESS_TRANSFER_WRITE_BIT
        | VK_ACCESS_SHADER_READ_BIT;
      imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      imgInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      m_gammaTexture = m_device->createImage(
        imgInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      DxvkImageViewCreateInfo viewInfo;
      viewInfo.type = VK_IMAGE_VIEW_TYPE_1D;
      viewInfo.format = VK_FORMAT_R16G16B16A16_UNORM;
      viewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
      viewInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.minLevel = 0;
      viewInfo.numLevels = 1;
      viewInfo.minLayer = 0;
      viewInfo.numLayers = 1;

      m_gammaTextureView = m_device->createImageView(m_gammaTexture, viewInfo);
    }

    m_context->beginRecording(
      m_device->createCommandList());

    m_context->updateImage(m_gammaTexture,
      VkImageSubresourceLayers{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
      VkOffset3D{ 0, 0, 0 },
      VkExtent3D{ GammaPointCount, 1, 1 },
      pControlPoints, 0, 0);

    m_device->submitCommandList(
      m_context->endRecording(),
      VK_NULL_HANDLE,
      VK_NULL_HANDLE);
  }

  void D3D9Presenter::createBackBuffer() {
    m_swapImage = nullptr;
    m_swapImageResolve = nullptr;
    m_swapImageView = nullptr;
    m_backBuffer = nullptr;

    D3D9TextureDesc desc;
    desc.Depth = 1;
    desc.Discard = FALSE;
    desc.Format = m_desc.format;
    desc.Height = std::max(1u, m_desc.height);
    desc.Lockable = FALSE;
    desc.MipLevels = 1;
    desc.MultiSample = m_desc.multisample;
    desc.MultisampleQuality = 0;
    desc.Pool = D3DPOOL_DEFAULT;
    desc.Type = D3DRTYPE_SURFACE;
    desc.Usage = D3DUSAGE_RENDERTARGET;
    desc.Width = std::max(1u, m_desc.width);
    desc.Offscreen = FALSE;

    m_backBuffer = new Direct3DCommonTexture9{ m_parent, &desc };

    m_swapImage = m_backBuffer->GetImage();

    // If the image is multisampled, we need to create
    // another image which we'll use as a resolve target
    if (m_swapImage->info().sampleCount != VK_SAMPLE_COUNT_1_BIT) {
      DxvkImageCreateInfo resolveInfo;
      resolveInfo.type = VK_IMAGE_TYPE_2D;
      resolveInfo.format = m_swapImage->info().format;
      resolveInfo.flags = 0;
      resolveInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
      resolveInfo.extent = m_swapImage->info().extent;
      resolveInfo.numLayers = 1;
      resolveInfo.mipLevels = 1;
      resolveInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      resolveInfo.stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_TRANSFER_BIT;
      resolveInfo.access = VK_ACCESS_SHADER_READ_BIT
        | VK_ACCESS_TRANSFER_WRITE_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      resolveInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      resolveInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      m_swapImageResolve = m_device->createImage(
        resolveInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    // Create an image view that allows the
    // image to be bound as a shader resource.
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_swapImage->info().format;
    viewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    viewInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel = 0;
    viewInfo.numLevels = 1;
    viewInfo.minLayer = 0;
    viewInfo.numLayers = 1;

    m_swapImageView = m_device->createImageView(
      m_swapImageResolve != nullptr
      ? m_swapImageResolve
      : m_swapImage,
      viewInfo);

    // Initialize the image so that we can use it. Clearing
    // to black prevents garbled output for the first frame.
    VkImageSubresourceRange subresources;
    subresources.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresources.baseMipLevel = 0;
    subresources.levelCount = 1;
    subresources.baseArrayLayer = 0;
    subresources.layerCount = 1;

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
      VK_NULL_HANDLE,
      VK_NULL_HANDLE);
  }

  void D3D9Presenter::createHud() {
    m_hud = hud::Hud::createHud(m_device);
  }

  void D3D9Presenter::initRenderState() {
    m_iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    m_iaState.primitiveRestart = VK_FALSE;
    m_iaState.patchVertexCount = 0;

    m_rsState.polygonMode = VK_POLYGON_MODE_FILL;
    m_rsState.cullMode = VK_CULL_MODE_BACK_BIT;
    m_rsState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    m_rsState.depthClipEnable = VK_FALSE;
    m_rsState.depthBiasEnable = VK_FALSE;
    m_rsState.sampleCount = VK_SAMPLE_COUNT_1_BIT;

    m_msState.sampleMask = 0xffffffff;
    m_msState.enableAlphaToCoverage = VK_FALSE;

    VkStencilOpState stencilOp;
    stencilOp.failOp = VK_STENCIL_OP_KEEP;
    stencilOp.passOp = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask = 0xFFFFFFFF;
    stencilOp.writeMask = 0xFFFFFFFF;
    stencilOp.reference = 0;

    m_dsState.enableDepthTest = VK_FALSE;
    m_dsState.enableDepthWrite = VK_FALSE;
    m_dsState.enableStencilTest = VK_FALSE;
    m_dsState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    m_dsState.stencilOpFront = stencilOp;
    m_dsState.stencilOpBack = stencilOp;

    m_loState.enableLogicOp = VK_FALSE;
    m_loState.logicOp = VK_LOGIC_OP_NO_OP;

    m_blendMode.enableBlending = VK_FALSE;
    m_blendMode.colorSrcFactor = VK_BLEND_FACTOR_ONE;
    m_blendMode.colorDstFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_blendMode.colorBlendOp = VK_BLEND_OP_ADD;
    m_blendMode.alphaSrcFactor = VK_BLEND_FACTOR_ONE;
    m_blendMode.alphaDstFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_blendMode.alphaBlendOp = VK_BLEND_OP_ADD;
    m_blendMode.writeMask = VK_COLOR_COMPONENT_R_BIT
      | VK_COLOR_COMPONENT_G_BIT
      | VK_COLOR_COMPONENT_B_BIT
      | VK_COLOR_COMPONENT_A_BIT;
  }


  void D3D9Presenter::initSamplers() {
    DxvkSamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipmapLodBias = 0.0f;
    samplerInfo.mipmapLodMin = 0.0f;
    samplerInfo.mipmapLodMax = 0.0f;
    samplerInfo.useAnisotropy = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.compareToDepth = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.borderColor = VkClearColorValue();
    samplerInfo.usePixelCoord = VK_FALSE;
    m_samplerFitting = m_device->createSampler(samplerInfo);

    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    m_samplerScaling = m_device->createSampler(samplerInfo);

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    m_gammaSampler = m_device->createSampler(samplerInfo);
  }


  void D3D9Presenter::initShaders() {
    const SpirvCodeBuffer vsCode(dxgi_presenter_vert);
    const SpirvCodeBuffer fsCode(dxgi_presenter_frag);

    const std::array<DxvkResourceSlot, 2> fsResourceSlots = { {
      { BindingIds::Image,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  VK_IMAGE_VIEW_TYPE_2D },
      { BindingIds::Gamma,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  VK_IMAGE_VIEW_TYPE_1D },
    } };

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

  void D3D9Presenter::recreateSwapChain(const D3D9PresenterDesc* desc) {
    m_desc = *desc;

    vk::PresenterDesc presenterDesc;
    presenterDesc.imageExtent = { m_desc.width, m_desc.height };
    presenterDesc.imageCount = pickImageCount(m_desc.bufferCount);
    presenterDesc.numFormats = pickFormats(m_desc.format, presenterDesc.formats);
    presenterDesc.numPresentModes = pickPresentModes(m_desc.presentInterval != 0, presenterDesc.presentModes);

    if (m_presenter->recreateSwapChain(presenterDesc) != VK_SUCCESS)
      throw DxvkError("D3D9Presenter: Failed to recreate swap chain");

    createRenderTargetViews();
  }

  void D3D9Presenter::present() {
    // Wait for the sync event so that we
    // respect the maximum frame latency
    Rc<DxvkEvent> syncEvent = m_parent->GetFrameSyncEvent(m_desc.bufferCount);
    syncEvent->wait();

    if (m_hud != nullptr)
      m_hud->update();

    for (uint32_t i = 0; i < m_desc.presentInterval || i < 1; i++) {
      m_context->beginRecording(
        m_device->createCommandList());

      // Resolve back buffer if it is multisampled. We
      // only have to do it only for the first frame.
      if (m_swapImageResolve != nullptr && i == 0) {
        VkImageSubresourceLayers resolveSubresource;
        resolveSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        resolveSubresource.mipLevel = 0;
        resolveSubresource.baseArrayLayer = 0;
        resolveSubresource.layerCount = 1;
        
        VkImageResolve resolveRegion;
        resolveRegion.srcSubresource = resolveSubresource;
        resolveRegion.srcOffset      = VkOffset3D { 0, 0, 0 };
        resolveRegion.dstSubresource = resolveSubresource;
        resolveRegion.dstOffset      = VkOffset3D { 0, 0, 0 };
        resolveRegion.extent         = m_swapImage->info().extent;

        m_context->resolveImage(
          m_swapImageResolve, m_swapImage,
          resolveRegion, VK_FORMAT_UNDEFINED);
      }

      // Presentation semaphores and WSI swap chain image
      vk::PresenterInfo info = m_presenter->info();
      vk::PresenterSync sync = m_presenter->getSyncSemaphores();

      uint32_t imageIndex = 0;

      VkResult status = m_presenter->acquireNextImage(
        sync.acquire, VK_NULL_HANDLE, imageIndex);

      while (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR) {
        recreateSwapChain(&m_desc);

        info = m_presenter->info();
        sync = m_presenter->getSyncSemaphores();

        status = m_presenter->acquireNextImage(
          sync.acquire, VK_NULL_HANDLE, imageIndex);
      }

      // Use an appropriate texture filter depending on whether
      // the back buffer size matches the swap image size
      bool fitSize = m_swapImage->info().extent.width == info.imageExtent.width
        && m_swapImage->info().extent.height == info.imageExtent.height;

      m_context->bindShader(VK_SHADER_STAGE_VERTEX_BIT, m_vertShader);
      m_context->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader);

      DxvkRenderTargets renderTargets;
      renderTargets.color[0].view = m_imageViews.at(imageIndex);
      renderTargets.color[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      m_context->bindRenderTargets(renderTargets, false);

      VkViewport viewport;
      viewport.x = 0.0f;
      viewport.y = 0.0f;
      viewport.width = float(info.imageExtent.width);
      viewport.height = float(info.imageExtent.height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;

      VkRect2D scissor;
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      scissor.extent.width = info.imageExtent.width;
      scissor.extent.height = info.imageExtent.height;

      m_context->setViewports(1, &viewport, &scissor);

      m_context->setRasterizerState(m_rsState);
      m_context->setMultisampleState(m_msState);
      m_context->setDepthStencilState(m_dsState);
      m_context->setLogicOpState(m_loState);
      m_context->setBlendMode(0, m_blendMode);

      m_context->setInputAssemblyState(m_iaState);
      m_context->setInputLayout(0, nullptr, 0, nullptr);

      m_context->bindResourceSampler(BindingIds::Image, fitSize ? m_samplerFitting : m_samplerScaling);
      m_context->bindResourceSampler(BindingIds::Gamma, m_gammaSampler);

      m_context->bindResourceView(BindingIds::Image, m_swapImageView, nullptr);
      m_context->bindResourceView(BindingIds::Gamma, m_gammaTextureView, nullptr);

      m_context->draw(4, 1, 0, 0);

      if (m_hud != nullptr)
        m_hud->render(m_context, info.imageExtent);

      if (i + 1 >= m_desc.presentInterval) {
        DxvkEventRevision eventRev;
        eventRev.event = syncEvent;
        eventRev.revision = syncEvent->reset();
        m_context->signalEvent(eventRev);
      }

      m_device->submitCommandList(
        m_context->endRecording(),
        sync.acquire, sync.present);

      status = m_device->presentImage(
        m_presenter, sync.present);

      if (status != VK_SUCCESS)
        recreateSwapChain(&m_desc);
    }
  }

  VkFormat D3D9Presenter::makeSrgb(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_SRGB;
    default: return format; // TODO: make this srgb-ness more correct.
    }
  }

  uint32_t D3D9Presenter::pickFormats(
    D3D9Format          format,
    VkSurfaceFormatKHR* dstFormats) {
    uint32_t n = 0;

    switch (format) {
      default:
        Logger::warn(str::format("D3D9Presenter: Unexpected format: ", format));

      case D3D9Format::A8R8G8B8:
      case D3D9Format::X8R8G8B8:
      case D3D9Format::A8B8G8R8:
      case D3D9Format::X8B8G8R8: {
        dstFormats[n++] = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        dstFormats[n++] = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      } break;

      case D3D9Format::A2R10G10B10:
      case D3D9Format::A2B10G10R10: {
        dstFormats[n++] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        dstFormats[n++] = { VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      } break;

      case D3D9Format::X1R5G5B5:
      case D3D9Format::A1R5G5B5: {
        dstFormats[n++] = { VK_FORMAT_B5G5R5A1_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        dstFormats[n++] = { VK_FORMAT_R5G5B5A1_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        dstFormats[n++] = { VK_FORMAT_A1R5G5B5_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      }

      case D3D9Format::R5G6B5: {
        dstFormats[n++] = { VK_FORMAT_B5G6R5_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        dstFormats[n++] = { VK_FORMAT_R5G6B5_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      }
    }

    return n;
  }

  uint32_t D3D9Presenter::pickPresentModes(
    bool                vsync,
    VkPresentModeKHR*   dstModes) {
    uint32_t n = 0;

    if (vsync) {
      dstModes[n++] = VK_PRESENT_MODE_FIFO_KHR;
    }
    else {
      dstModes[n++] = VK_PRESENT_MODE_IMMEDIATE_KHR;
      dstModes[n++] = VK_PRESENT_MODE_MAILBOX_KHR;
      dstModes[n++] = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    }

    return n;
  }

  uint32_t D3D9Presenter::pickImageCount(
    uint32_t            preferred) {
    return preferred;
  }

  void D3D9Presenter::createPresenter() {
    DxvkDeviceQueue graphicsQueue = m_device->graphicsQueue();

    vk::PresenterDevice presenterDevice;
    presenterDevice.queueFamily = graphicsQueue.queueFamily;
    presenterDevice.queue = graphicsQueue.queueHandle;
    presenterDevice.adapter = m_device->adapter()->handle();

    vk::PresenterDesc presenterDesc;
    presenterDesc.imageExtent = { m_desc.width, m_desc.height };
    presenterDesc.imageCount = pickImageCount(m_desc.bufferCount + 1); // Account for front buffer
    presenterDesc.numFormats = pickFormats(m_desc.format, presenterDesc.formats);
    presenterDesc.numPresentModes = pickPresentModes(m_desc.presentInterval != 0, presenterDesc.presentModes);

    m_presenter = new vk::Presenter(m_window,
      m_device->adapter()->vki(),
      m_device->vkd(),
      presenterDevice,
      presenterDesc);

    createRenderTargetViews();
  }

  void D3D9Presenter::createRenderTargetViews() {
    vk::PresenterInfo info = m_presenter->info();

    m_imageViews.clear();

    m_imageViews.resize(info.imageCount);

    DxvkImageCreateInfo imageInfo;
    imageInfo.type = VK_IMAGE_TYPE_2D;
    imageInfo.format = info.format.format;
    imageInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.extent = { info.imageExtent.width, info.imageExtent.height, 1 };
    imageInfo.numLayers = 1;
    imageInfo.mipLevels = 1;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.stages = 0;
    imageInfo.access = 0;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imageInfo.flags = 0;

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = info.format.format;
    viewInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    viewInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel = 0;
    viewInfo.numLevels = 1;
    viewInfo.minLayer = 0;
    viewInfo.numLayers = 1;

    for (uint32_t i = 0; i < info.imageCount; i++) {
      VkImage imageHandle = m_presenter->getImage(i).image;

      Rc<DxvkImage> image = new DxvkImage(
        m_device->vkd(), imageInfo, imageHandle);

      m_imageViews[i] = new DxvkImageView(
        m_device->vkd(), image, viewInfo);
    }
  }

}