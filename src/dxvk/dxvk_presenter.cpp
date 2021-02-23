#include "dxvk_presenter.h"

#include <dxvk_present_frag.h>
#include <dxvk_present_frag_blit.h>
#include <dxvk_present_frag_ms.h>
#include <dxvk_present_frag_ms_amd.h>
#include <dxvk_present_vert.h>

namespace dxvk {
  
  DxvkPresenter::DxvkPresenter(
    const Rc<DxvkDevice>&     device,
          HWND                window,
    const vk::PresenterDesc&  desc,
    const std::string&        apiName)
  : m_device(device), m_context(m_device->createContext()),
    m_hud(hud::Hud::createHud(m_device)),
    m_presenterDesc(desc) {
    this->createPresenter(window);
    this->createRenderTargetViews();
    this->createSampler();
    this->createShaders();

    if (m_hud != nullptr)
      m_hud->addItem<hud::HudClientApiItem>("api", 1, apiName);
  }


  DxvkPresenter::~DxvkPresenter() {
    m_device->waitForSubmission(&m_status);
    m_device->waitForIdle();
  }


  bool DxvkPresenter::recordPresentCommands(
    const Rc<DxvkImageView>&  backBuffer,
          VkRect2D            dstRect,
          VkRect2D            srcRect,
    const Rc<sync::Signal>&   signal,
          uint32_t            frameId,
          uint32_t            repeat) {
    this->synchronizePresent();

    if (!m_presenter->hasSwapChain())
      return false;

    // Recreate Vulkan swap chain if necessary
    vk::PresenterInfo info = m_presenter->info();
    vk::PresenterSync sync = m_presenter->getSyncSemaphores();

    uint32_t imageIndex = 0;

    VkResult status = m_presenter->acquireNextImage(
      sync.acquire, VK_NULL_HANDLE, imageIndex);

    while (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR) {
      recreateSwapChain();

      if (!m_presenter->hasSwapChain())
        return false;
      
      info = m_presenter->info();
      sync = m_presenter->getSyncSemaphores();

      status = m_presenter->acquireNextImage(
        sync.acquire, VK_NULL_HANDLE, imageIndex);
    }

    Rc<DxvkImageView> view = m_renderTargetViews.at(imageIndex);

    // Update gamma texture if necessary
    m_context->beginRecording(m_device->createCommandList());

    if (m_gammaDirty) {
      uint32_t n = uint32_t(m_gammaRamp.size());

      m_context->updateImage(m_gammaImage,
        VkImageSubresourceLayers { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        VkOffset3D { 0, 0, 0 },
        VkExtent3D { n, 1, 1 },
        m_gammaRamp.data(),
        sizeof(DxvkGammaCp) * n,
        sizeof(DxvkGammaCp) * n);

      m_gammaDirty = false;
    }

    // Fix up default present areas if necessary
    if (!dstRect.extent.width || !dstRect.extent.height) {
      dstRect.offset = { 0, 0 };
      dstRect.extent = info.imageExtent;
    }

    if (!srcRect.extent.width || !srcRect.extent.height) {
      srcRect.offset = { 0, 0 };
      srcRect.extent = {
        backBuffer->imageInfo().extent.width,
        backBuffer->imageInfo().extent.height };
    }

    // Set up render state
    DxvkInputAssemblyState  iaState;
    iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    iaState.primitiveRestart  = VK_FALSE;
    iaState.patchVertexCount  = 0;
    m_context->setInputAssemblyState(iaState);
    m_context->setInputLayout(0, nullptr, 0, nullptr);
    
    DxvkRasterizerState rsState;
    rsState.polygonMode        = VK_POLYGON_MODE_FILL;
    rsState.cullMode           = VK_CULL_MODE_BACK_BIT;
    rsState.frontFace          = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.depthClipEnable    = VK_FALSE;
    rsState.depthBiasEnable    = VK_FALSE;
    rsState.sampleCount        = VK_SAMPLE_COUNT_1_BIT;
    m_context->setRasterizerState(rsState);
    
    DxvkMultisampleState msState;
    msState.sampleMask            = 0xffffffff;
    msState.enableAlphaToCoverage = VK_FALSE;
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
    dsState.enableStencilTest = VK_FALSE;
    dsState.depthCompareOp    = VK_COMPARE_OP_ALWAYS;
    dsState.stencilOpFront    = stencilOp;
    dsState.stencilOpBack     = stencilOp;
    m_context->setDepthStencilState(dsState);
    
    DxvkLogicOpState loState;
    loState.enableLogicOp = VK_FALSE;
    loState.logicOp       = VK_LOGIC_OP_NO_OP;
    m_context->setLogicOpState(loState);

    DxvkBlendMode blendMode;
    blendMode.enableBlending  = VK_FALSE;
    blendMode.colorSrcFactor  = VK_BLEND_FACTOR_ONE;
    blendMode.colorDstFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendMode.colorBlendOp    = VK_BLEND_OP_ADD;
    blendMode.alphaSrcFactor  = VK_BLEND_FACTOR_ONE;
    blendMode.alphaDstFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendMode.alphaBlendOp    = VK_BLEND_OP_ADD;
    blendMode.writeMask       = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                              | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_context->setBlendMode(0, blendMode);

    VkViewport viewport;
    viewport.x        = float(dstRect.offset.x);
    viewport.y        = float(dstRect.offset.y);
    viewport.width    = float(dstRect.extent.width);
    viewport.height   = float(dstRect.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    m_context->setViewports(1, &viewport, &dstRect);

    Rc<DxvkShader> fs;
    Rc<DxvkImageView> resource = backBuffer;

    if (dstRect.extent == srcRect.extent) {
      // Copy or resolve back buffer directly into the swap chain image
      fs = backBuffer->imageInfo().sampleCount == VK_SAMPLE_COUNT_1_BIT ? m_fsCopy : m_fsResolve;
    } else {
      fs = m_fsBlit;

      if (backBuffer->imageInfo().sampleCount != VK_SAMPLE_COUNT_1_BIT) {
        // Create a temporary resolve image since we cannot perform
        // a resolve and blit at the same time, then blit from that.
        if (m_resolveImage == nullptr
         || m_resolveImage->info().extent != backBuffer->imageInfo().extent
         || m_resolveImage->info().format != backBuffer->imageInfo().format)
          createResolveImage(backBuffer->imageInfo());

        if (!repeat) {
          VkImageResolve resolve;
          resolve.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
          resolve.srcOffset      = { 0, 0, 0 };
          resolve.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
          resolve.dstOffset      = { 0u, 0u, 0u };
          resolve.extent         = m_resolveView->imageInfo().extent;

          m_context->resolveImage(
            m_resolveImage, backBuffer->image(),
            resolve, VK_FORMAT_UNDEFINED);
        }

        resource = m_resolveView;
      }
    }

    if (resource == backBuffer)
      destroyResolveImage();

    // Render back buffer to swap chain image
    DxvkRenderTargets renderTargets;
    renderTargets.color[0].view   = view;
    renderTargets.color[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    m_context->bindRenderTargets(renderTargets);

    if (dstRect.extent == info.imageExtent)
      m_context->discardImageView(view, VK_IMAGE_ASPECT_COLOR_BIT);
    else
      m_context->clearRenderTarget(view, VK_IMAGE_ASPECT_COLOR_BIT, VkClearValue());

    m_context->bindResourceSampler(BindingIds::Image, m_samplerPresent);
    m_context->bindResourceSampler(BindingIds::Gamma, m_samplerGamma);

    m_context->bindResourceView(BindingIds::Image, resource, nullptr);
    m_context->bindResourceView(BindingIds::Gamma, m_gammaView, nullptr);

    m_context->bindShader(VK_SHADER_STAGE_VERTEX_BIT, m_vs);
    m_context->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, fs);

    PresenterArgs args;
    args.srcOffset = srcRect.offset;

    if (dstRect.extent == srcRect.extent)
      args.dstOffset = dstRect.offset;
    else
      args.srcExtent = srcRect.extent;

    m_context->pushConstants(0, sizeof(args), &args);

    m_context->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, resource->imageInfo().sampleCount);
    m_context->draw(3, 1, 0, 0);
    m_context->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0);

    // Draw HUD, but use full image area
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = float(info.imageExtent.width);
    viewport.height   = float(info.imageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = info.imageExtent;
    
    m_context->setViewports(1, &viewport, &scissor);

    if (m_hud != nullptr)
      m_hud->render(m_context, info.format, info.imageExtent);

    if (signal != nullptr)
      m_context->signal(signal, frameId);

    m_commands = m_context->endRecording();
    m_presenterSync = sync;

    m_status.result = VK_NOT_READY;
    return true;
  }


  void DxvkPresenter::submitPresentCommands(uint32_t repeat) {
    m_device->submitCommandList(std::move(m_commands),
      m_presenterSync.acquire, m_presenterSync.present);

    if (!repeat && m_hud != nullptr)
      m_hud->update();

    m_device->presentImage(m_presenter,
      m_presenterSync.present, &m_status);
  }


  void DxvkPresenter::changeParameters(const vk::PresenterDesc& desc) {
    m_presenterDesc = desc;
    this->recreateSwapChain();
  }


  void DxvkPresenter::setGammaRamp(
          uint32_t            cpCount,
    const DxvkGammaCp*        cpData) {
    if (cpCount) {
      // Reuse existing image if possible
      if (m_gammaImage == nullptr || m_gammaImage->info().extent.width != cpCount) {
        DxvkImageCreateInfo imgInfo;
        imgInfo.type        = VK_IMAGE_TYPE_1D;
        imgInfo.format      = VK_FORMAT_R16G16B16A16_UNORM;
        imgInfo.flags       = 0;
        imgInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.extent      = { cpCount, 1, 1 };
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
        
        m_gammaImage = m_device->createImage(
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
        
        m_gammaView = m_device->createImageView(m_gammaImage, viewInfo);
      }

      m_gammaRamp.resize(cpCount);
      for (uint32_t i = 0; i < cpCount; i++)
        m_gammaRamp[i] = cpData[i];
      m_gammaDirty = true;
    } else {
      m_gammaRamp.clear();
      m_gammaImage = nullptr;
      m_gammaView  = nullptr;
      m_gammaDirty = false;
    }
  }


  void DxvkPresenter::synchronizePresent() {
    VkResult status = m_device->waitForSubmission(&m_status);

    if (status != VK_SUCCESS)
      this->recreateSwapChain();
  }


  void DxvkPresenter::createPresenter(HWND window) {
    DxvkDeviceQueue graphicsQueue = m_device->queues().graphics;

    vk::PresenterDevice presenterDevice;
    presenterDevice.queueFamily   = graphicsQueue.queueFamily;
    presenterDevice.queue         = graphicsQueue.queueHandle;
    presenterDevice.adapter       = m_device->adapter()->handle();
    presenterDevice.features.fullScreenExclusive = m_device->extensions().extFullScreenExclusive;

    m_presenter = new vk::Presenter(window,
      m_device->adapter()->vki(), m_device->vkd(),
      presenterDevice, m_presenterDesc);
  }


  void DxvkPresenter::createRenderTargetViews() {
    vk::PresenterInfo info = m_presenter->info();

    m_renderTargetViews.clear();
    m_renderTargetViews.resize(info.imageCount);

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
      
      auto image = new DxvkImage(m_device->vkd(), imageInfo, imageHandle);
      auto view = new DxvkImageView(m_device->vkd(), image, viewInfo);
      m_renderTargetViews[i] = view;
    }
  }

  void DxvkPresenter::createSampler() {
    DxvkSamplerCreateInfo samplerInfo;
    samplerInfo.magFilter       = VK_FILTER_LINEAR;
    samplerInfo.minFilter       = VK_FILTER_LINEAR;
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
    samplerInfo.usePixelCoord   = VK_TRUE;
    m_samplerPresent = m_device->createSampler(samplerInfo);

    samplerInfo.addressModeU    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.usePixelCoord   = VK_FALSE;
    m_samplerGamma = m_device->createSampler(samplerInfo);
  }

  void DxvkPresenter::createShaders() {
    const SpirvCodeBuffer vsCode(dxvk_present_vert);
    const SpirvCodeBuffer fsCodeBlit(dxvk_present_frag_blit);
    const SpirvCodeBuffer fsCodeCopy(dxvk_present_frag);
    const SpirvCodeBuffer fsCodeResolve(dxvk_present_frag_ms);
    const SpirvCodeBuffer fsCodeResolveAmd(dxvk_present_frag_ms_amd);
    
    const std::array<DxvkResourceSlot, 2> fsResourceSlots = {{
      { BindingIds::Image, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D },
      { BindingIds::Gamma, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_1D },
    }};

    m_vs = m_device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      0, nullptr, { 0u, 1u },
      vsCode);
    
    m_fsBlit = m_device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      fsResourceSlots.size(),
      fsResourceSlots.data(),
      { 1u, 1u, 0u, sizeof(PresenterArgs) },
      fsCodeBlit);
    
    m_fsCopy = m_device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      fsResourceSlots.size(),
      fsResourceSlots.data(),
      { 0u, 1u, 0u, sizeof(PresenterArgs) },
      fsCodeCopy);
    
    m_fsResolve = m_device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      fsResourceSlots.size(),
      fsResourceSlots.data(),
      { 0u, 1u, 0u, sizeof(PresenterArgs) },
      m_device->extensions().amdShaderFragmentMask
        ? fsCodeResolveAmd : fsCodeResolve);
  }

  void DxvkPresenter::createResolveImage(const DxvkImageCreateInfo& info) {
    DxvkImageCreateInfo newInfo;
    newInfo.type = VK_IMAGE_TYPE_2D;
    newInfo.format = info.format;
    newInfo.flags = 0;
    newInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    newInfo.extent = info.extent;
    newInfo.numLayers = 1;
    newInfo.mipLevels = 1;
    newInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                   | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                   | VK_IMAGE_USAGE_SAMPLED_BIT;
    newInfo.stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                   | VK_PIPELINE_STAGE_TRANSFER_BIT
                   | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    newInfo.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                   | VK_ACCESS_TRANSFER_WRITE_BIT
                   | VK_ACCESS_SHADER_READ_BIT;
    newInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    newInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    m_resolveImage = m_device->createImage(newInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = info.format;
    viewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    viewInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel  = 0;
    viewInfo.numLevels = 1;
    viewInfo.minLayer  = 0;
    viewInfo.numLayers = 1;
    m_resolveView = m_device->createImageView(m_resolveImage, viewInfo);
  }

  void DxvkPresenter::destroyResolveImage() {
    m_resolveImage = nullptr;
    m_resolveView = nullptr;
  }

  void DxvkPresenter::recreateSwapChain() {
    m_device->waitForSubmission(&m_status);
    m_device->waitForIdle();

    m_status.result = VK_SUCCESS;

    if (m_presenter->recreateSwapChain(m_presenterDesc) != VK_SUCCESS)
      throw DxvkError("D3D11SwapChain: Failed to recreate swap chain");

    this->createRenderTargetViews();    
  }

}