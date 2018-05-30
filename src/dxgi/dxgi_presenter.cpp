#include "dxgi_presenter.h"

#include "../spirv/spirv_module.h"

#include <dxgi_presenter_frag.h>
#include <dxgi_presenter_vert.h>

namespace dxvk {
  
  DxgiVkPresenter::DxgiVkPresenter(
    const Rc<DxvkDevice>&         device,
          HWND                    window)
  : m_window  (window),
    m_device  (device),
    m_context (device->createContext()) {
    
    // Some games don't work with deferred surface creation,
    // so we should default to initializing it immediately.
    DxgiOptions dxgiOptions = getDxgiAppOptions(env::getExeName());
    
    if (!dxgiOptions.test(DxgiOption::DeferSurfaceCreation))
      m_surface = CreateSurface();
    
    // Reset options for the swap chain itself. We will
    // create a swap chain object before presentation.
    m_options.preferredSurfaceFormat = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    m_options.preferredPresentMode   = VK_PRESENT_MODE_FIFO_KHR;
    m_options.preferredBufferSize    = { 0u, 0u };
    
    // Samplers for presentation. We'll create one with point sampling that will
    // be used when the back buffer resolution matches the output resolution, and
    // one with linar sampling that will be used when the image will be scaled.
    m_samplerFitting = CreateSampler(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
    m_samplerScaling = CreateSampler(VK_FILTER_LINEAR,  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
    
    // Create objects required for the gamma ramp. This is implemented partially
    // with an UBO, which stores global parameters, and a lookup texture, which
    // stores the actual gamma ramp and can be sampled with a linear filter.
    m_gammaSampler      = CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    m_gammaTexture      = CreateGammaTexture();
    m_gammaTextureView  = CreateGammaTextureView();
    
    // Set up context state. The shader bindings and the
    // constant state objects will never be modified.
    DxvkInputAssemblyState iaState;
    iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    iaState.primitiveRestart  = VK_FALSE;
    iaState.patchVertexCount  = 0;
    m_context->setInputAssemblyState(iaState);
    
    m_context->setInputLayout(
      0, nullptr, 0, nullptr);
    
    DxvkRasterizerState rsState;
    rsState.enableDepthClamp   = VK_FALSE;
    rsState.enableDiscard      = VK_FALSE;
    rsState.polygonMode        = VK_POLYGON_MODE_FILL;
    rsState.cullMode           = VK_CULL_MODE_BACK_BIT;
    rsState.frontFace          = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.depthBiasEnable    = VK_FALSE;
    rsState.depthBiasConstant  = 0.0f;
    rsState.depthBiasClamp     = 0.0f;
    rsState.depthBiasSlope     = 0.0f;
    m_context->setRasterizerState(rsState);
    
    DxvkMultisampleState msState;
    msState.sampleMask            = 0xffffffff;
    msState.enableAlphaToCoverage = VK_FALSE;
    msState.enableAlphaToOne      = VK_FALSE;
    m_context->setMultisampleState(msState);
    
    VkStencilOpState stencilOp;
    stencilOp.failOp      = VK_STENCIL_OP_KEEP;
    stencilOp.passOp      = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp   = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask = 0xFFFFFFFF;
    stencilOp.writeMask   = 0xFFFFFFFF;
    stencilOp.reference   = 0;
    
    DxvkDepthStencilState dsState;
    dsState.enableDepthTest   = VK_FALSE;
    dsState.enableDepthWrite  = VK_FALSE;
    dsState.enableDepthBounds = VK_FALSE;
    dsState.enableStencilTest = VK_FALSE;
    dsState.depthCompareOp    = VK_COMPARE_OP_ALWAYS;
    dsState.stencilOpFront    = stencilOp;
    dsState.stencilOpBack     = stencilOp;
    dsState.depthBoundsMin    = 0.0f;
    dsState.depthBoundsMax    = 1.0f;
    m_context->setDepthStencilState(dsState);
    
    DxvkLogicOpState loState;
    loState.enableLogicOp = VK_FALSE;
    loState.logicOp       = VK_LOGIC_OP_NO_OP;
    m_context->setLogicOpState(loState);
    
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
    
    m_context->bindShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      CreateVertexShader());
    
    m_context->bindShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      CreateFragmentShader());
    
    m_hud = hud::Hud::createHud(m_device);
  }
  
  
  DxgiVkPresenter::~DxgiVkPresenter() {
    m_device->waitForIdle();
  }
  
  
  void DxgiVkPresenter::InitBackBuffer(const Rc<DxvkImage>& Image) {
    m_context->beginRecording(
      m_device->createCommandList());
    
    VkImageSubresourceRange sr;
    sr.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    sr.baseMipLevel   = 0;
    sr.levelCount     = Image->info().mipLevels;
    sr.baseArrayLayer = 0;
    sr.layerCount     = Image->info().numLayers;
    
    m_context->initImage(Image, sr);
    
    m_device->submitCommandList(
      m_context->endRecording(),
      nullptr, nullptr);
  }
  
  
  void DxgiVkPresenter::PresentImage() {
    if (m_hud != nullptr) {
      m_hud->render({
        m_options.preferredBufferSize.width,
        m_options.preferredBufferSize.height,
      });
    }
    
    const bool fitSize =
        m_backBuffer->info().extent.width  == m_options.preferredBufferSize.width
     && m_backBuffer->info().extent.height == m_options.preferredBufferSize.height;
    
    m_context->beginRecording(
      m_device->createCommandList());
    
    VkImageSubresourceLayers resolveSubresources;
    resolveSubresources.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    resolveSubresources.mipLevel        = 0;
    resolveSubresources.baseArrayLayer  = 0;
    resolveSubresources.layerCount      = 1;
    
    if (m_backBufferResolve != nullptr) {
      m_context->resolveImage(
        m_backBufferResolve, resolveSubresources,
        m_backBuffer,        resolveSubresources,
        VK_FORMAT_UNDEFINED);
    }
    
    auto swapSemas = m_swapchain->getSemaphorePair();
    auto swapImage = m_swapchain->getImageView(swapSemas.acquireSync);
    
    DxvkRenderTargets renderTargets;
    renderTargets.color[0].view   = swapImage;
    renderTargets.color[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    m_context->bindRenderTargets(renderTargets);
    
    VkViewport viewport;
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = float(swapImage->imageInfo().extent.width);
    viewport.height   = float(swapImage->imageInfo().extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor;
    scissor.offset.x      = 0;
    scissor.offset.y      = 0;
    scissor.extent.width  = swapImage->imageInfo().extent.width;
    scissor.extent.height = swapImage->imageInfo().extent.height;
    
    m_context->setViewports(1, &viewport, &scissor);
    
    m_context->bindResourceSampler(BindingIds::Sampler,
      fitSize ? m_samplerFitting : m_samplerScaling);
    
    m_blendMode.enableBlending = VK_FALSE;
    m_context->setBlendMode(0, m_blendMode);
    
    m_context->bindResourceView(BindingIds::Texture, m_backBufferView, nullptr);
    m_context->draw(4, 1, 0, 0);
    
    m_context->bindResourceSampler(BindingIds::GammaSmp, m_gammaSampler);
    m_context->bindResourceView   (BindingIds::GammaTex, m_gammaTextureView, nullptr);
    
    if (m_hud != nullptr) {
      m_blendMode.enableBlending = VK_TRUE;
      m_context->setBlendMode(0, m_blendMode);
      
      m_context->bindResourceView(BindingIds::Texture, m_hud->texture(), nullptr);
      m_context->draw(4, 1, 0, 0);
    }
    
    m_device->submitCommandList(
      m_context->endRecording(),
      swapSemas.acquireSync,
      swapSemas.presentSync);
    
    m_swapchain->present(
      swapSemas.presentSync);
  }
  
  
  void DxgiVkPresenter::UpdateBackBuffer(const Rc<DxvkImage>& Image) {
    // Explicitly destroy the old stuff
    m_backBuffer        = Image;
    m_backBufferResolve = nullptr;
    m_backBufferView    = nullptr;
    
    // If a multisampled back buffer was requested, we also need to
    // create a resolve image with otherwise identical properties.
    // Multisample images cannot be sampled from.
    if (Image->info().sampleCount != VK_SAMPLE_COUNT_1_BIT) {
      DxvkImageCreateInfo resolveInfo;
      resolveInfo.type          = VK_IMAGE_TYPE_2D;
      resolveInfo.format        = Image->info().format;
      resolveInfo.flags         = 0;
      resolveInfo.sampleCount   = VK_SAMPLE_COUNT_1_BIT;
      resolveInfo.extent        = Image->info().extent;
      resolveInfo.numLayers     = 1;
      resolveInfo.mipLevels     = 1;
      resolveInfo.usage         = VK_IMAGE_USAGE_SAMPLED_BIT
                                | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      resolveInfo.stages        = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                | VK_PIPELINE_STAGE_TRANSFER_BIT;
      resolveInfo.access        = VK_ACCESS_SHADER_READ_BIT
                                | VK_ACCESS_TRANSFER_WRITE_BIT;
      resolveInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
      resolveInfo.layout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      
      m_backBufferResolve = m_device->createImage(
        resolveInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    
    // Create an image view that allows the
    // image to be bound as a shader resource.
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format     = Image->info().format;
    viewInfo.aspect     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel   = 0;
    viewInfo.numLevels  = 1;
    viewInfo.minLayer   = 0;
    viewInfo.numLayers  = 1;
    
    m_backBufferView = m_device->createImageView(
      m_backBufferResolve != nullptr
        ? m_backBufferResolve
        : m_backBuffer,
      viewInfo);
    
    InitBackBuffer(m_backBuffer);
  }
  
  
  void DxgiVkPresenter::SetGammaControl(
    const DXGI_VK_GAMMA_CURVE*          pGammaCurve) {
    m_context->beginRecording(
      m_device->createCommandList());
    
    m_context->updateImage(m_gammaTexture,
      VkImageSubresourceLayers { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
      VkOffset3D { 0, 0, 0 },
      VkExtent3D { DXGI_VK_GAMMA_CP_COUNT, 1, 1 },
      pGammaCurve, 0, 0);
    
    m_device->submitCommandList(
      m_context->endRecording(),
      nullptr, nullptr);
  }
  
  
  void DxgiVkPresenter::RecreateSwapchain(DXGI_FORMAT Format, VkPresentModeKHR PresentMode, VkExtent2D WindowSize) {
    if (m_surface == nullptr)
      m_surface = CreateSurface();
    
    DxvkSwapchainProperties options;
    options.preferredSurfaceFormat  = PickSurfaceFormat(Format);
    options.preferredPresentMode    = PickPresentMode(PresentMode);
    options.preferredBufferSize     = WindowSize;
    
    const bool doRecreate =
         options.preferredSurfaceFormat.format      != m_options.preferredSurfaceFormat.format
      || options.preferredSurfaceFormat.colorSpace  != m_options.preferredSurfaceFormat.colorSpace
      || options.preferredPresentMode               != m_options.preferredPresentMode
      || options.preferredBufferSize.width          != m_options.preferredBufferSize.width
      || options.preferredBufferSize.height         != m_options.preferredBufferSize.height;
    
    if (doRecreate) {
      Logger::info(str::format(
        "DxgiVkPresenter: Recreating swap chain: ",
        "\n  Format:       ", options.preferredSurfaceFormat.format,
        "\n  Present mode: ", options.preferredPresentMode,
        "\n  Buffer size:  ", options.preferredBufferSize.width, "x", options.preferredBufferSize.height));
      
      if (m_swapchain == nullptr)
        m_swapchain = m_device->createSwapchain(m_surface, options);
      else
        m_swapchain->changeProperties(options);
      
      m_options = options;
    }
  }
  
  
  VkSurfaceFormatKHR DxgiVkPresenter::PickSurfaceFormat(DXGI_FORMAT Fmt) const {
    std::vector<VkSurfaceFormatKHR> formats;
    
    switch (Fmt) {
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_B8G8R8A8_UNORM: {
        formats.push_back({ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
        formats.push_back({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
      } break;
      
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: {
        formats.push_back({ VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
        formats.push_back({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
      } break;
      
      case DXGI_FORMAT_R10G10B10A2_UNORM: {
        formats.push_back({ VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
        formats.push_back({ VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
      } break;
      
      case DXGI_FORMAT_R16G16B16A16_FLOAT: {
        formats.push_back({ VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR });
      } break;
      
      default:
        Logger::warn(str::format("DxgiVkPresenter: Unknown format: ", Fmt));
    }
    
    return m_surface->pickSurfaceFormat(
      formats.size(), formats.data());
  }
  
  
  VkPresentModeKHR DxgiVkPresenter::PickPresentMode(VkPresentModeKHR Preferred) const {
    return m_surface->pickPresentMode(1, &Preferred);
  }
  
  
  Rc<DxvkSurface> DxgiVkPresenter::CreateSurface() {
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(m_window, GWLP_HINSTANCE));
    
    return m_device->adapter()->createSurface(instance, m_window);
  }
  
  
  Rc<DxvkSampler> DxgiVkPresenter::CreateSampler(
          VkFilter              Filter,
          VkSamplerAddressMode  AddressMode) {
    DxvkSamplerCreateInfo samplerInfo;
    samplerInfo.magFilter       = Filter;
    samplerInfo.minFilter       = Filter;
    samplerInfo.mipmapMode      = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipmapLodBias   = 0.0f;
    samplerInfo.mipmapLodMin    = 0.0f;
    samplerInfo.mipmapLodMax    = 0.0f;
    samplerInfo.useAnisotropy   = VK_FALSE;
    samplerInfo.maxAnisotropy   = 1.0f;
    samplerInfo.addressModeU    = AddressMode;
    samplerInfo.addressModeV    = AddressMode;
    samplerInfo.addressModeW    = AddressMode;
    samplerInfo.compareToDepth  = VK_FALSE;
    samplerInfo.compareOp       = VK_COMPARE_OP_ALWAYS;
    samplerInfo.borderColor     = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerInfo.usePixelCoord   = VK_FALSE;
    return m_device->createSampler(samplerInfo);
  }
  
  
  Rc<DxvkImage> DxgiVkPresenter::CreateGammaTexture() {
    DxvkImageCreateInfo info;
    info.type         = VK_IMAGE_TYPE_1D;
    info.format       = VK_FORMAT_R16G16B16A16_UNORM;
    info.flags        = 0;
    info.sampleCount  = VK_SAMPLE_COUNT_1_BIT;
    info.extent       = { DXGI_VK_GAMMA_CP_COUNT, 1, 1 };
    info.numLayers    = 1;
    info.mipLevels    = 1;
    info.usage        = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                      | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.stages       = VK_PIPELINE_STAGE_TRANSFER_BIT
                      | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    info.access       = VK_ACCESS_TRANSFER_WRITE_BIT
                      | VK_ACCESS_SHADER_READ_BIT;
    info.tiling       = VK_IMAGE_TILING_OPTIMAL;
    info.layout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return m_device->createImage(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }
  
  
  Rc<DxvkImageView> DxgiVkPresenter::CreateGammaTextureView() {
    DxvkImageViewCreateInfo info;
    info.type         = VK_IMAGE_VIEW_TYPE_1D;
    info.format       = VK_FORMAT_R16G16B16A16_UNORM;
    info.aspect       = VK_IMAGE_ASPECT_COLOR_BIT;
    info.minLevel     = 0;
    info.numLevels    = 1;
    info.minLayer     = 0;
    info.numLayers    = 1;
    return m_device->createImageView(m_gammaTexture, info);
  }
  
  
  Rc<DxvkShader> DxgiVkPresenter::CreateVertexShader() {
    const SpirvCodeBuffer codeBuffer(dxgi_presenter_vert);
    
    return m_device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      0, nullptr, { 0u, 1u },
      codeBuffer);
  }
  
  
  Rc<DxvkShader> DxgiVkPresenter::CreateFragmentShader() {
    const SpirvCodeBuffer codeBuffer(dxgi_presenter_frag);
    
    // Shader resource slots
    const std::array<DxvkResourceSlot, 4> resourceSlots = {{
      { BindingIds::Sampler,  VK_DESCRIPTOR_TYPE_SAMPLER,        VK_IMAGE_VIEW_TYPE_MAX_ENUM },
      { BindingIds::Texture,  VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  VK_IMAGE_VIEW_TYPE_2D       },
      { BindingIds::GammaSmp, VK_DESCRIPTOR_TYPE_SAMPLER,        VK_IMAGE_VIEW_TYPE_MAX_ENUM },
      { BindingIds::GammaTex, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  VK_IMAGE_VIEW_TYPE_1D       },
    }};
    
    // Create the actual shader module
    return m_device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      resourceSlots.size(),
      resourceSlots.data(),
      { 1u, 1u },
      codeBuffer);
  }
  
}
