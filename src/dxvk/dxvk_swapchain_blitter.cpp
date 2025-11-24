#include "dxvk_swapchain_blitter.h"

#include <dxvk_cursor_frag.h>
#include <dxvk_cursor_vert.h>
#include <dxvk_present_frag.h>
#include <dxvk_present_frag_blit.h>
#include <dxvk_present_frag_ms.h>
#include <dxvk_present_frag_ms_blit.h>
#include <dxvk_present_vert.h>

namespace dxvk {
  
  DxvkSwapchainBlitter::DxvkSwapchainBlitter(
    const Rc<DxvkDevice>& device,
    const Rc<hud::Hud>&   hud)
  : m_device(device), m_hud(hud),
    m_blitLayout(createBlitPipelineLayout()),
    m_cursorLayout(createCursorPipelineLayout()) {
    this->createSampler();
  }


  DxvkSwapchainBlitter::~DxvkSwapchainBlitter() {
    auto vk = m_device->vkd();

    for (const auto& p : m_pipelines)
      vk->vkDestroyPipeline(vk->device(), p.second, nullptr);

    for (const auto& p : m_cursorPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second, nullptr);
  }


  void DxvkSwapchainBlitter::present(
    const Rc<DxvkCommandList>&ctx,
    const Rc<DxvkImageView>&  dstView,
          VkRect2D            dstRect,
    const Rc<DxvkImageView>&  srcView,
          VkRect2D            srcRect) {
    std::unique_lock lock(m_mutex);

    // Update HUD, if we have one
    if (m_hud)
      m_hud->update();

    // Fix up default present areas if necessary
    if (!dstRect.extent.width || !dstRect.extent.height) {
      dstRect.offset = { 0, 0 };
      dstRect.extent = {
        dstView->image()->info().extent.width,
        dstView->image()->info().extent.height };
    }

    if (!srcRect.extent.width || !srcRect.extent.height) {
      srcRect.offset = { 0, 0 };
      srcRect.extent = {
        srcView->image()->info().extent.width,
        srcView->image()->info().extent.height };
    }

    if (m_gammaBuffer)
      uploadGammaImage(ctx);

    if (m_cursorBuffer)
      uploadCursorImage(ctx);

    // If we can't do proper blending, render the HUD into a separate image
    bool composite = needsComposition(dstView);

    if (m_hud && composite)
      renderHudImage(ctx, dstView->mipLevelExtent(0));
    else
      destroyHudImage();

    VkImageLayout renderLayout = dstView->getLayout();

    VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = renderLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = dstView->image()->handle();
    barrier.subresourceRange = dstView->imageSubresources();

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    ctx->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    VkExtent3D dstExtent = dstView->mipLevelExtent(0u);

    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageView = dstView->handle();
    attachmentInfo.imageLayout = renderLayout;
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    if (srcRect.extent != dstRect.extent)
      attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

    VkRenderingInfo renderInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderInfo.renderArea.offset = { 0u, 0u };
    renderInfo.renderArea.extent = { dstExtent.width, dstExtent.height };
    renderInfo.layerCount = 1u;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &attachmentInfo;

    ctx->cmdBeginRendering(&renderInfo);

    performDraw(ctx, dstView, dstRect,
      srcView, srcRect, composite);

    if (!composite) {
      if (m_hud)
        m_hud->render(ctx, dstView);

      if (m_cursorView)
        renderCursor(ctx, dstView);
    }

    ctx->cmdEndRendering();

    barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.oldLayout = renderLayout;
    barrier.newLayout = dstView->image()->info().layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = dstView->image()->handle();
    barrier.subresourceRange = dstView->imageSubresources();

    depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    ctx->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);
  }


  void DxvkSwapchainBlitter::setGammaRamp(
          uint32_t            cpCount,
    const DxvkGammaCp*        cpData) {
    std::unique_lock lock(m_mutex);

    if (cpCount) {
      // Create temporary upload buffer for the curve
      DxvkBufferCreateInfo bufferInfo = { };
      bufferInfo.size = cpCount * sizeof(*cpData);
      bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      bufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
      bufferInfo.access = VK_ACCESS_TRANSFER_READ_BIT;

      m_gammaBuffer = m_device->createBuffer(bufferInfo,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      m_gammaCpCount = cpCount;

      std::memcpy(m_gammaBuffer->mapPtr(0), cpData, cpCount * sizeof(*cpData));
    } else {
      // Destroy gamma image altogether
      m_gammaBuffer = nullptr;
      m_gammaImage = nullptr;
      m_gammaView = nullptr;
      m_gammaCpCount = 0;
    }
  }


  void DxvkSwapchainBlitter::setCursorTexture(
          VkExtent2D          extent,
          VkFormat            format,
    const void*               data) {
    std::unique_lock lock(m_mutex);

    if (extent.width && extent.height && format && data) {
      auto formatInfo = lookupFormatInfo(format);

      DxvkBufferCreateInfo bufferInfo = { };
      bufferInfo.size = extent.width * extent.height * formatInfo->elementSize;
      bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      bufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
      bufferInfo.access = VK_ACCESS_TRANSFER_READ_BIT;

      m_cursorBuffer = m_device->createBuffer(bufferInfo,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      std::memcpy(m_cursorBuffer->mapPtr(0), data, bufferInfo.size);

      DxvkImageCreateInfo imageInfo = { };
      imageInfo.type = VK_IMAGE_TYPE_2D;
      imageInfo.format = format;
      imageInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.extent = { extent.width, extent.height, 1u };
      imageInfo.numLayers = 1u;
      imageInfo.mipLevels = 1u;
      imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                      | VK_IMAGE_USAGE_SAMPLED_BIT;
      imageInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                       | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      imageInfo.access = VK_ACCESS_TRANSFER_WRITE_BIT
                       | VK_ACCESS_TRANSFER_READ_BIT
                       | VK_ACCESS_SHADER_READ_BIT;
      imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      imageInfo.debugName = "Swapchain cursor";

      m_cursorImage = m_device->createImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      DxvkImageViewKey viewInfo = { };
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
      viewInfo.format = format;
      viewInfo.aspects = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.mipIndex = 0u;
      viewInfo.mipCount = 1u;
      viewInfo.layerIndex = 0u;
      viewInfo.layerCount = 1u;

      m_cursorView = m_cursorImage->createView(viewInfo);
    } else {
      // Destroy cursor image
      m_cursorBuffer = nullptr;
      m_cursorImage = nullptr;
      m_cursorView = nullptr;
    }
  }


  void DxvkSwapchainBlitter::setCursorPos(
          VkRect2D            rect) {
    std::unique_lock lock(m_mutex);
    m_cursorRect = rect;
  }


  void DxvkSwapchainBlitter::performDraw(
    const Rc<DxvkCommandList>&ctx,
    const Rc<DxvkImageView>&  dstView,
          VkRect2D            dstRect,
    const Rc<DxvkImageView>&  srcView,
          VkRect2D            srcRect,
          VkBool32            composite) {
    VkColorSpaceKHR dstColorSpace = dstView->image()->info().colorSpace;
    VkColorSpaceKHR srcColorSpace = srcView->image()->info().colorSpace;

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture))) {
      ctx->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xdcc0f0, "Swapchain blit"));
    }

    VkExtent3D dstExtent = dstView->mipLevelExtent(0);

    VkOffset2D coordA = dstRect.offset;
    VkOffset2D coordB = {
      coordA.x + int32_t(dstRect.extent.width),
      coordA.y + int32_t(dstRect.extent.height)
    };

    coordA.x = std::max(coordA.x, 0);
    coordA.y = std::max(coordA.y, 0);
    coordB.x = std::min(coordB.x, int32_t(dstExtent.width));
    coordB.y = std::min(coordB.y, int32_t(dstExtent.height));

    if (coordA.x >= coordB.x || coordA.y >= coordB.y)
      return;

    VkViewport viewport = { };
    viewport.x = float(dstRect.offset.x);
    viewport.y = float(dstRect.offset.y);
    viewport.width = float(dstRect.extent.width);
    viewport.height = float(dstRect.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 0.0f;

    ctx->cmdSetViewport(1, &viewport);

    VkRect2D scissor = { };
    scissor.offset = coordA;
    scissor.extent.width = uint32_t(coordB.x - coordA.x);
    scissor.extent.height = uint32_t(coordB.y - coordA.y);

    ctx->cmdSetScissor(1, &scissor);

    DxvkSwapchainPipelineKey key;
    key.srcSpace = srcColorSpace;
    key.srcSamples = srcView->image()->info().sampleCount;
    key.srcIsSrgb = srcView->formatInfo()->flags.test(DxvkFormatFlag::ColorSpaceSrgb);
    key.dstSpace = dstColorSpace;
    key.dstFormat = dstView->info().format;
    key.needsGamma = m_gammaView != nullptr;
    key.needsBlit = dstRect.extent != srcRect.extent;
    key.compositeHud = composite && m_hudSrv;
    key.compositeCursor = composite && m_cursorView;

    VkPipeline pipeline = getBlitPipeline(key);

    // Set up resource bindings
    std::array<DxvkDescriptorWrite, 4> descriptors = { };

    auto& imageDescriptor = descriptors[0u];
    imageDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imageDescriptor.descriptor = srcView->getDescriptor();

    auto& gammaDescriptor = descriptors[1u];
    gammaDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    if (m_gammaView)
      gammaDescriptor.descriptor = m_gammaView->getDescriptor();

    auto& hudDescriptor = descriptors[2u];
    hudDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    if (m_hudSrv)
      hudDescriptor.descriptor = m_hudSrv->getDescriptor();

    auto& cursorDescriptor = descriptors[3u];
    cursorDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    uint32_t cursorSampler = m_samplerCursorNearest->getDescriptor().samplerIndex;

    if (m_cursorView) {
      VkExtent3D extent = m_cursorImage->info().extent;

      if (m_cursorRect.extent.width != extent.width
       || m_cursorRect.extent.height != extent.height)
        cursorSampler = m_samplerCursorLinear->getDescriptor().samplerIndex;

      cursorDescriptor.descriptor = m_cursorView->getDescriptor();
    }

    PushConstants args = { };
    args.srcOffset = srcRect.offset;
    args.srcExtent = srcRect.extent;
    args.dstOffset = dstRect.offset;
    args.cursorOffset = m_cursorRect.offset;
    args.cursorExtent = m_cursorRect.extent;
    args.samplerGamma = m_samplerGamma->getDescriptor().samplerIndex;
    args.samplerCursor = cursorSampler;

    ctx->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    ctx->bindResources(DxvkCmdBuffer::ExecBuffer,
      m_blitLayout, descriptors.size(), descriptors.data(),
      sizeof(args), &args);

    ctx->cmdDraw(3, 1, 0, 0);

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      ctx->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    // Make sure to keep used resources alive
    ctx->track(srcView->image(), DxvkAccess::Read);
    ctx->track(dstView->image(), DxvkAccess::Write);

    if (m_gammaImage)
      ctx->track(m_gammaImage, DxvkAccess::Read);

    if (m_hudImage)
      ctx->track(m_hudImage, DxvkAccess::Read);

    if (m_cursorImage)
      ctx->track(m_cursorImage, DxvkAccess::Read);

    ctx->track(m_samplerGamma);
    ctx->track(m_samplerCursorLinear);
    ctx->track(m_samplerCursorNearest);
  }


  void DxvkSwapchainBlitter::renderHudImage(
    const Rc<DxvkCommandList>&        ctx,
          VkExtent3D                  extent) {
    if (m_hud->empty())
      return;

    if (!m_hudImage || m_hudImage->info().extent != extent)
      createHudImage(extent);

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture))) {
      ctx->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xdcc0f0, "HUD render"));
    }

    // Reset image
    VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = m_hudRtv->getLayout();
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_hudImage->handle();
    barrier.subresourceRange = m_hudRtv->imageSubresources();

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    ctx->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    // Render actual HUD image
    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageView = m_hudRtv->handle();
    attachmentInfo.imageLayout = m_hudRtv->getLayout();
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderInfo.renderArea.offset = { 0u, 0u };
    renderInfo.renderArea.extent = { extent.width, extent.height };
    renderInfo.layerCount = 1u;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &attachmentInfo;

    ctx->cmdBeginRendering(&renderInfo);

    m_hud->render(ctx, m_hudRtv);

    ctx->cmdEndRendering();

    // Make image shader-readable
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.oldLayout = m_hudRtv->getLayout();
    barrier.newLayout = m_hudImage->info().layout;

    ctx->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    m_hudImage->trackLayout(m_hudRtv->imageSubresources(), m_hudImage->info().layout);

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      ctx->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    ctx->track(m_hudImage, DxvkAccess::Write);
  }


  void DxvkSwapchainBlitter::createHudImage(
          VkExtent3D                  extent) {
    DxvkImageCreateInfo imageInfo = { };
    imageInfo.type          = VK_IMAGE_TYPE_2D;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.sampleCount   = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.extent        = extent;
    imageInfo.mipLevels     = 1;
    imageInfo.numLayers     = 1;
    imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.stages        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    imageInfo.access        = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                            | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                            | VK_ACCESS_SHADER_READ_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.layout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.colorSpace    = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    imageInfo.debugName     = "HUD composition";

    m_hudImage = m_device->createImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    DxvkImageViewKey viewInfo = { };
    viewInfo.viewType       = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format         = imageInfo.format;
    viewInfo.aspects        = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.mipIndex       = 0u;
    viewInfo.mipCount       = 1u;
    viewInfo.layerIndex     = 0u;
    viewInfo.layerCount     = 1u;

    viewInfo.usage          = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    m_hudRtv = m_hudImage->createView(viewInfo);

    viewInfo.usage          = VK_IMAGE_USAGE_SAMPLED_BIT;
    m_hudSrv = m_hudImage->createView(viewInfo);
  }


  void DxvkSwapchainBlitter::destroyHudImage() {
    m_hudImage = nullptr;
    m_hudRtv = nullptr;
    m_hudSrv = nullptr;
  }


  void DxvkSwapchainBlitter::renderCursor(
    const Rc<DxvkCommandList>&        ctx,
    const Rc<DxvkImageView>&          dstView) {
    if (!m_cursorRect.extent.width || !m_cursorRect.extent.height)
      return;

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture))) {
      ctx->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xdcc0f0, "Software cursor"));
    }

    VkExtent3D dstExtent = dstView->mipLevelExtent(0u);

    VkViewport viewport = { };
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = float(dstExtent.width);
    viewport.height = float(dstExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 0.0f;

    ctx->cmdSetViewport(1, &viewport);

    VkRect2D scissor = { };
    scissor.extent.width = dstExtent.width;
    scissor.extent.height = dstExtent.height;

    ctx->cmdSetScissor(1, &scissor);

    DxvkCursorPipelineKey key = { };
    key.dstFormat = dstView->info().format;
    key.dstSpace = dstView->image()->info().colorSpace;

    VkPipeline pipeline = getCursorPipeline(key);

    VkExtent3D cursorExtent = m_cursorImage->info().extent;

    DxvkDescriptorWrite imageDescriptor = { };
    imageDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    imageDescriptor.descriptor = m_cursorView->getDescriptor();

    CursorPushConstants args = { };
    args.dstExtent = { dstExtent.width, dstExtent.height };
    args.cursorOffset = m_cursorRect.offset;
    args.cursorExtent = m_cursorRect.extent;
    args.sampler = m_samplerCursorNearest->getDescriptor().samplerIndex;

    if (m_cursorRect.extent.width != cursorExtent.width
     || m_cursorRect.extent.height != cursorExtent.height)
      args.sampler = m_samplerCursorLinear->getDescriptor().samplerIndex;

    ctx->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    ctx->bindResources(DxvkCmdBuffer::ExecBuffer,
      m_cursorLayout, 1u, &imageDescriptor, sizeof(args), &args);

    ctx->cmdDraw(4, 1, 0, 0);

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      ctx->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    ctx->track(m_cursorImage, DxvkAccess::Write);
  }


  void DxvkSwapchainBlitter::uploadGammaImage(
    const Rc<DxvkCommandList>&        ctx) {
    if (!m_gammaImage || m_gammaImage->info().extent.width != m_gammaCpCount) {
      DxvkImageCreateInfo imageInfo = { };
      imageInfo.type = VK_IMAGE_TYPE_1D;
      imageInfo.format = VK_FORMAT_R16G16B16A16_UNORM;
      imageInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.extent = { m_gammaCpCount, 1u, 1u };
      imageInfo.numLayers = 1u;
      imageInfo.mipLevels = 1u;
      imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      imageInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      imageInfo.access = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
      imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
      imageInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imageInfo.debugName = "Swapchain gamma ramp";

      m_gammaImage = m_device->createImage(imageInfo,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      DxvkImageViewKey viewInfo = { };
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
      viewInfo.format = imageInfo.format;
      viewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
      viewInfo.aspects = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.mipIndex = 0u;
      viewInfo.mipCount = 1u;
      viewInfo.layerIndex = 0u;
      viewInfo.layerCount = 1u;

      m_gammaView = m_gammaImage->createView(viewInfo);
    }

    uploadTexture(ctx, m_gammaImage, m_gammaBuffer);
    m_gammaBuffer = nullptr;
  }


  void DxvkSwapchainBlitter::uploadCursorImage(
    const Rc<DxvkCommandList>&        ctx) {
    uploadTexture(ctx, m_cursorImage, m_cursorBuffer);
    m_cursorBuffer = nullptr;
  }


  void DxvkSwapchainBlitter::uploadTexture(
    const Rc<DxvkCommandList>&        ctx,
    const Rc<DxvkImage>&              image,
    const Rc<DxvkBuffer>&             buffer) {
    VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = image->info().stages;
    barrier.srcAccessMask = image->info().access;
    barrier.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image->handle();
    barrier.subresourceRange = image->getAvailableSubresources();

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    ctx->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    DxvkResourceBufferInfo bufferSlice = buffer->getSliceInfo();

    VkBufferImageCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
    copyRegion.bufferOffset = bufferSlice.offset;
    copyRegion.imageExtent = image->info().extent;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1u;

    VkCopyBufferToImageInfo2 copy = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
    copy.srcBuffer = bufferSlice.buffer;
    copy.dstImage = image->handle();
    copy.dstImageLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy.regionCount = 1;
    copy.pRegions = &copyRegion;

    ctx->cmdCopyBufferToImage(DxvkCmdBuffer::ExecBuffer, &copy);

    barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = image->info().stages;
    barrier.dstAccessMask = image->info().access;
    barrier.oldLayout = barrier.newLayout;
    barrier.newLayout = image->info().layout;

    ctx->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    image->trackLayout(image->getAvailableSubresources(), image->info().layout);

    ctx->track(buffer, DxvkAccess::Read);
    ctx->track(image, DxvkAccess::Write);
  }


  void DxvkSwapchainBlitter::createSampler() {
    DxvkSamplerKey samplerInfo = { };
    samplerInfo.setFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
      VK_SAMPLER_MIPMAP_MODE_NEAREST);
    samplerInfo.setAddressModes(
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    samplerInfo.setUsePixelCoordinates(false);
 
    m_samplerGamma = m_device->createSampler(samplerInfo);

    samplerInfo.setAddressModes(
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

    m_samplerCursorLinear = m_device->createSampler(samplerInfo);

    samplerInfo.setFilter(VK_FILTER_NEAREST, VK_FILTER_NEAREST,
      VK_SAMPLER_MIPMAP_MODE_NEAREST);

    m_samplerCursorNearest = m_device->createSampler(samplerInfo);
  }


  const DxvkPipelineLayout* DxvkSwapchainBlitter::createBlitPipelineLayout() {
    static const std::array<DxvkDescriptorSetLayoutBinding, 4> bindings = {{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    return m_device->createBuiltInPipelineLayout(DxvkPipelineLayoutFlag::UsesSamplerHeap,
      VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushConstants), bindings.size(), bindings.data());
  }


  const DxvkPipelineLayout* DxvkSwapchainBlitter::createCursorPipelineLayout() {
    static const std::array<DxvkDescriptorSetLayoutBinding, 1> bindings = {{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    return m_device->createBuiltInPipelineLayout(DxvkPipelineLayoutFlag::UsesSamplerHeap,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      sizeof(CursorPushConstants), bindings.size(), bindings.data());
  }


  VkPipeline DxvkSwapchainBlitter::createBlitPipeline(
    const DxvkSwapchainPipelineKey&   key) {
    auto vk = m_device->vkd();

    static const std::array<VkSpecializationMapEntry, 8> specMap = {{
      { 0, offsetof(SpecConstants, sampleCount),    sizeof(VkSampleCountFlagBits) },
      { 1, offsetof(SpecConstants, gammaBound),     sizeof(VkBool32) },
      { 2, offsetof(SpecConstants, srcSpace),       sizeof(VkColorSpaceKHR) },
      { 3, offsetof(SpecConstants, srcIsSrgb),      sizeof(VkBool32) },
      { 4, offsetof(SpecConstants, dstSpace),       sizeof(VkColorSpaceKHR) },
      { 5, offsetof(SpecConstants, dstIsSrgb),      sizeof(VkBool32) },
      { 6, offsetof(SpecConstants, compositeHud),   sizeof(VkBool32) },
      { 7, offsetof(SpecConstants, compositeCursor),sizeof(VkBool32) },
    }};

    SpecConstants specConstants = { };
    specConstants.sampleCount = key.srcSamples;
    specConstants.gammaBound = key.needsGamma && key.srcSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    specConstants.srcSpace = key.srcSpace;
    specConstants.srcIsSrgb = key.srcIsSrgb;
    specConstants.dstSpace = key.dstSpace;
    specConstants.dstIsSrgb = lookupFormatInfo(key.dstFormat)->flags.test(DxvkFormatFlag::ColorSpaceSrgb);
    specConstants.compositeCursor = key.compositeCursor;
    specConstants.compositeHud = key.compositeHud;

    // Avoid redundant color space conversions if color spaces
    // and images properties match and we don't do a resolve
    if (key.srcSpace == key.dstSpace && key.srcSamples == VK_SAMPLE_COUNT_1_BIT
     && !key.compositeCursor && !key.compositeHud) {
      specConstants.srcSpace = VK_COLOR_SPACE_PASS_THROUGH_EXT;
      specConstants.dstSpace = VK_COLOR_SPACE_PASS_THROUGH_EXT;
    }

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = specMap.size();
    specInfo.pMapEntries = specMap.data();
    specInfo.dataSize = sizeof(specConstants);
    specInfo.pData = &specConstants;

    util::DxvkBuiltInGraphicsState state = { };
    state.vs = util::DxvkBuiltInShaderStage(dxvk_present_vert, nullptr);

    if (key.srcSamples == VK_SAMPLE_COUNT_1_BIT) {
      state.fs = key.needsBlit
        ? util::DxvkBuiltInShaderStage(dxvk_present_frag_blit, &specInfo)
        : util::DxvkBuiltInShaderStage(dxvk_present_frag, &specInfo);
    } else {
      state.fs = key.needsBlit
        ? util::DxvkBuiltInShaderStage(dxvk_present_frag_ms_blit, &specInfo)
        : util::DxvkBuiltInShaderStage(dxvk_present_frag_ms, &specInfo);
    }

    state.colorFormat = key.dstFormat;
    return m_device->createBuiltInGraphicsPipeline(m_blitLayout, state);
  }


  VkPipeline DxvkSwapchainBlitter::getBlitPipeline(
    const DxvkSwapchainPipelineKey&   key) {
    auto entry = m_pipelines.find(key);

    if (entry != m_pipelines.end())
      return entry->second;

    VkPipeline pipeline = createBlitPipeline(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }


  VkPipeline DxvkSwapchainBlitter::createCursorPipeline(
    const DxvkCursorPipelineKey&      key) {
    auto vk = m_device->vkd();

    static const std::array<VkSpecializationMapEntry, 2> specMap = {{
      { 0, offsetof(CursorSpecConstants, dstSpace),  sizeof(VkColorSpaceKHR) },
      { 1, offsetof(CursorSpecConstants, dstIsSrgb), sizeof(VkBool32) },
    }};

    CursorSpecConstants specConstants = { };
    specConstants.dstSpace = key.dstSpace;
    specConstants.dstIsSrgb = lookupFormatInfo(key.dstFormat)->flags.test(DxvkFormatFlag::ColorSpaceSrgb);

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = specMap.size();
    specInfo.pMapEntries = specMap.data();
    specInfo.dataSize = sizeof(specConstants);
    specInfo.pData = &specConstants;

    VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineColorBlendAttachmentState cbAttachment = { };
    cbAttachment.blendEnable = VK_TRUE;
    cbAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    cbAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    cbAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    util::DxvkBuiltInGraphicsState state = { };
    state.vs = util::DxvkBuiltInShaderStage(dxvk_cursor_vert, nullptr);
    state.fs = util::DxvkBuiltInShaderStage(dxvk_cursor_frag, &specInfo);
    state.colorFormat = key.dstFormat;
    state.iaState = &iaState;
    state.cbAttachment = &cbAttachment;

    return m_device->createBuiltInGraphicsPipeline(m_cursorLayout, state);
  }


  VkPipeline DxvkSwapchainBlitter::getCursorPipeline(
    const DxvkCursorPipelineKey&      key) {
    auto entry = m_cursorPipelines.find(key);

    if (entry != m_cursorPipelines.end())
      return entry->second;

    VkPipeline pipeline = createCursorPipeline(key);
    m_cursorPipelines.insert({ key, pipeline });
    return pipeline;
  }


  bool DxvkSwapchainBlitter::needsComposition(
    const Rc<DxvkImageView>&          dstView) {
    VkColorSpaceKHR colorSpace = dstView->image()->info().colorSpace;

    switch (colorSpace) {
      case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        return !dstView->formatInfo()->flags.test(DxvkFormatFlag::ColorSpaceSrgb);

      case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        return false;

      default:
        return true;
    }
  }

}
