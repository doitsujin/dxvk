#include "dxvk_swapchain_blitter.h"

#include <dxvk_present_frag.h>
#include <dxvk_present_frag_blit.h>
#include <dxvk_present_frag_ms.h>
#include <dxvk_present_frag_ms_amd.h>
#include <dxvk_present_vert.h>

namespace dxvk {
  
  DxvkSwapchainBlitter::DxvkSwapchainBlitter(const Rc<DxvkDevice>& device)
  : m_device(device) {
    this->createSampler();
    this->createShaders();
  }


  DxvkSwapchainBlitter::~DxvkSwapchainBlitter() {
    
  }


  void DxvkSwapchainBlitter::presentImage(
          DxvkContext*        ctx,
    const Rc<DxvkImageView>&  dstView,
          VkRect2D            dstRect,
    const Rc<DxvkImageView>&  srcView,
          VkRect2D            srcRect) {
    if (m_gammaDirty)
      this->updateGammaTexture(ctx);

    // Fix up default present areas if necessary
    if (!dstRect.extent.width || !dstRect.extent.height) {
      dstRect.offset = { 0, 0 };
      dstRect.extent = {
        dstView->imageInfo().extent.width,
        dstView->imageInfo().extent.height };
    }

    if (!srcRect.extent.width || !srcRect.extent.height) {
      srcRect.offset = { 0, 0 };
      srcRect.extent = {
        srcView->imageInfo().extent.width,
        srcView->imageInfo().extent.height };
    }

    bool sameSize = dstRect.extent == srcRect.extent;
    bool usedResolveImage = false;

    if (srcView->imageInfo().sampleCount == VK_SAMPLE_COUNT_1_BIT) {
      this->draw(ctx, sameSize ? m_fsCopy : m_fsBlit,
        dstView, dstRect, srcView, srcRect);
    } else if (sameSize) {
      this->draw(ctx, m_fsResolve,
        dstView, dstRect, srcView, srcRect);
    } else {
        if (m_resolveImage == nullptr
         || m_resolveImage->info().extent != srcView->imageInfo().extent
         || m_resolveImage->info().format != srcView->imageInfo().format)
          this->createResolveImage(srcView->imageInfo());

      this->resolve(ctx, m_resolveView, srcView);
      this->draw(ctx, m_fsBlit, dstView, dstRect, m_resolveView, srcRect);

      usedResolveImage = true;
    }

    if (!usedResolveImage)
      this->destroyResolveImage();
  }


  void DxvkSwapchainBlitter::setGammaRamp(
          uint32_t            cpCount,
    const DxvkGammaCp*        cpData) {
    m_gammaRamp.resize(cpCount);

    for (uint32_t i = 0; i < cpCount; i++)
      m_gammaRamp[i] = cpData[i];

    m_gammaDirty = true;
  }


  void DxvkSwapchainBlitter::draw(
          DxvkContext*        ctx,
    const Rc<DxvkShader>&     fs,
    const Rc<DxvkImageView>&  dstView,
          VkRect2D            dstRect,
    const Rc<DxvkImageView>&  srcView,
          VkRect2D            srcRect) {
    DxvkInputAssemblyState iaState;
    iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    iaState.primitiveRestart  = VK_FALSE;
    iaState.patchVertexCount  = 0;
    ctx->setInputAssemblyState(iaState);
    ctx->setInputLayout(0, nullptr, 0, nullptr);
    
    DxvkRasterizerState rsState;
    rsState.polygonMode        = VK_POLYGON_MODE_FILL;
    rsState.cullMode           = VK_CULL_MODE_BACK_BIT;
    rsState.frontFace          = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.depthClipEnable    = VK_FALSE;
    rsState.depthBiasEnable    = VK_FALSE;
    rsState.conservativeMode   = VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
    rsState.sampleCount        = VK_SAMPLE_COUNT_1_BIT;
    ctx->setRasterizerState(rsState);
    
    DxvkMultisampleState msState;
    msState.sampleMask            = 0xffffffff;
    msState.enableAlphaToCoverage = VK_FALSE;
    ctx->setMultisampleState(msState);
    
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
    ctx->setDepthStencilState(dsState);
    
    DxvkLogicOpState loState;
    loState.enableLogicOp = VK_FALSE;
    loState.logicOp       = VK_LOGIC_OP_NO_OP;
    ctx->setLogicOpState(loState);

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
    ctx->setBlendMode(0, blendMode);

    VkViewport viewport;
    viewport.x        = float(dstRect.offset.x);
    viewport.y        = float(dstRect.offset.y);
    viewport.width    = float(dstRect.extent.width);
    viewport.height   = float(dstRect.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    ctx->setViewports(1, &viewport, &dstRect);

    DxvkRenderTargets renderTargets;
    renderTargets.color[0].view   = dstView;
    renderTargets.color[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    ctx->bindRenderTargets(renderTargets);

    VkExtent2D dstExtent = {
      dstView->imageInfo().extent.width,
      dstView->imageInfo().extent.height };

    if (dstRect.extent == dstExtent)
      ctx->discardImageView(dstView, VK_IMAGE_ASPECT_COLOR_BIT);
    else
      ctx->clearRenderTarget(dstView, VK_IMAGE_ASPECT_COLOR_BIT, VkClearValue());

    ctx->bindResourceSampler(BindingIds::Image, m_samplerPresent);
    ctx->bindResourceSampler(BindingIds::Gamma, m_samplerGamma);

    ctx->bindResourceView(BindingIds::Image, srcView, nullptr);
    ctx->bindResourceView(BindingIds::Gamma, m_gammaView, nullptr);

    ctx->bindShader(VK_SHADER_STAGE_VERTEX_BIT, m_vs);
    ctx->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, fs);

    PresenterArgs args;
    args.srcOffset = srcRect.offset;

    if (dstRect.extent == srcRect.extent)
      args.dstOffset = dstRect.offset;
    else
      args.srcExtent = srcRect.extent;

    ctx->pushConstants(0, sizeof(args), &args);

    ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, srcView->imageInfo().sampleCount);
    ctx->draw(3, 1, 0, 0);
    ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0);
  }

  void DxvkSwapchainBlitter::resolve(
          DxvkContext*        ctx,
    const Rc<DxvkImageView>&  dstView,
    const Rc<DxvkImageView>&  srcView) {
    VkImageResolve resolve;
    resolve.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    resolve.srcOffset      = { 0, 0, 0 };
    resolve.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    resolve.dstOffset      = { 0, 0, 0 };
    resolve.extent         = dstView->imageInfo().extent;
    ctx->resolveImage(dstView->image(), srcView->image(), resolve, VK_FORMAT_UNDEFINED);
  }


  void DxvkSwapchainBlitter::updateGammaTexture(DxvkContext* ctx) {
    uint32_t n = uint32_t(m_gammaRamp.size());

    if (n) {
      // Reuse existing image if possible
      if (m_gammaImage == nullptr || m_gammaImage->info().extent.width != n) {
        DxvkImageCreateInfo imgInfo;
        imgInfo.type        = VK_IMAGE_TYPE_1D;
        imgInfo.format      = VK_FORMAT_R16G16B16A16_UNORM;
        imgInfo.flags       = 0;
        imgInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.extent      = { n, 1, 1 };
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

      ctx->updateImage(m_gammaImage,
        VkImageSubresourceLayers { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        VkOffset3D { 0, 0, 0 },
        VkExtent3D { n, 1, 1 },
        m_gammaRamp.data(),
        sizeof(DxvkGammaCp) * n,
        sizeof(DxvkGammaCp) * n);
    } else {
      m_gammaImage = nullptr;
      m_gammaView  = nullptr;
    }

    m_gammaDirty = false;
  }


  void DxvkSwapchainBlitter::createSampler() {
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

  void DxvkSwapchainBlitter::createShaders() {
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

  void DxvkSwapchainBlitter::createResolveImage(const DxvkImageCreateInfo& info) {
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


  void DxvkSwapchainBlitter::destroyResolveImage() {
    m_resolveImage = nullptr;
    m_resolveView = nullptr;
  }

}