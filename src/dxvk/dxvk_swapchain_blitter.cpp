#include "dxvk_swapchain_blitter.h"

#include <dxvk_cursor_frag.h>
#include <dxvk_cursor_vert.h>
#include <dxvk_present_frag.h>
#include <dxvk_present_frag_blit.h>
#include <dxvk_present_frag_ms.h>
#include <dxvk_present_frag_ms_amd.h>
#include <dxvk_present_frag_ms_blit.h>
#include <dxvk_present_vert.h>

namespace dxvk {
  
  DxvkSwapchainBlitter::DxvkSwapchainBlitter(
    const Rc<DxvkDevice>& device,
    const Rc<hud::Hud>&   hud)
  : m_device(device), m_hud(hud),
    m_setLayout(createSetLayout()),
    m_pipelineLayout(createPipelineLayout()),
    m_cursorSetLayout(createCursorSetLayout()),
    m_cursorPipelineLayout(createCursorPipelineLayout()) {
    this->createSampler();
    this->createShaders();
  }


  DxvkSwapchainBlitter::~DxvkSwapchainBlitter() {
    auto vk = m_device->vkd();

    for (const auto& p : m_pipelines)
      vk->vkDestroyPipeline(vk->device(), p.second, nullptr);

    for (const auto& p : m_cursorPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second, nullptr);

    vk->vkDestroyShaderModule(vk->device(), m_shaderVsBlit.stageInfo.module, nullptr);
    vk->vkDestroyShaderModule(vk->device(), m_shaderFsBlit.stageInfo.module, nullptr);
    vk->vkDestroyShaderModule(vk->device(), m_shaderFsCopy.stageInfo.module, nullptr);
    vk->vkDestroyShaderModule(vk->device(), m_shaderFsMsBlit.stageInfo.module, nullptr);
    vk->vkDestroyShaderModule(vk->device(), m_shaderFsMsResolve.stageInfo.module, nullptr);

    vk->vkDestroyShaderModule(vk->device(), m_shaderVsCursor.stageInfo.module, nullptr);
    vk->vkDestroyShaderModule(vk->device(), m_shaderFsCursor.stageInfo.module, nullptr);

    vk->vkDestroyPipelineLayout(vk->device(), m_pipelineLayout, nullptr);
    vk->vkDestroyDescriptorSetLayout(vk->device(), m_setLayout, nullptr);

    vk->vkDestroyPipelineLayout(vk->device(), m_cursorPipelineLayout, nullptr);
    vk->vkDestroyDescriptorSetLayout(vk->device(), m_cursorSetLayout, nullptr);
  }


  void DxvkSwapchainBlitter::present(
    const DxvkContextObjects& ctx,
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

    VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = dstView->image()->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = dstView->image()->handle();
    barrier.subresourceRange = dstView->imageSubresources();

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    VkExtent3D dstExtent = dstView->mipLevelExtent(0u);

    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageView = dstView->handle();
    attachmentInfo.imageLayout = dstView->image()->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
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

    ctx.cmd->cmdBeginRendering(&renderInfo);

    performDraw(ctx, dstView, dstRect,
      srcView, srcRect, composite);

    if (!composite) {
      if (m_hud)
        m_hud->render(ctx, dstView);

      if (m_cursorView)
        renderCursor(ctx, dstView);
    }

    ctx.cmd->cmdEndRendering();

    barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.oldLayout = dstView->image()->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    barrier.newLayout = dstView->image()->info().layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = dstView->image()->handle();
    barrier.subresourceRange = dstView->imageSubresources();

    depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);
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
    const DxvkContextObjects& ctx,
    const Rc<DxvkImageView>&  dstView,
          VkRect2D            dstRect,
    const Rc<DxvkImageView>&  srcView,
          VkRect2D            srcRect,
          VkBool32            composite) {
    VkColorSpaceKHR dstColorSpace = dstView->image()->info().colorSpace;
    VkColorSpaceKHR srcColorSpace = srcView->image()->info().colorSpace;

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture))) {
      ctx.cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
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

    ctx.cmd->cmdSetViewport(1, &viewport);

    VkRect2D scissor = { };
    scissor.offset = coordA;
    scissor.extent.width = uint32_t(coordB.x - coordA.x);
    scissor.extent.height = uint32_t(coordB.y - coordA.y);

    ctx.cmd->cmdSetScissor(1, &scissor);

    DxvkSwapchainPipelineKey key;
    key.srcSpace = srcColorSpace;
    key.srcSamples = srcView->image()->info().sampleCount;
    key.srcIsSrgb = srcView->formatInfo()->flags.test(DxvkFormatFlag::ColorSpaceSrgb);
    key.dstSpace = dstColorSpace;
    key.dstFormat = dstView->info().format;
    key.needsGamma = m_gammaView != nullptr;
    key.needsBlit = dstRect.extent != srcRect.extent;
    key.compositeHud = composite && m_hudView;
    key.compositeCursor = composite && m_cursorView;

    VkPipeline pipeline = getPipeline(key);

    ctx.cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDescriptorSet set = ctx.descriptorPool->alloc(m_setLayout);

    VkDescriptorImageInfo imageDescriptor = { };
    imageDescriptor.sampler = m_samplerPresent->handle();
    imageDescriptor.imageView = srcView->handle();
    imageDescriptor.imageLayout = srcView->image()->info().layout;

    VkDescriptorImageInfo gammaDescriptor = { };
    gammaDescriptor.sampler = m_samplerGamma->handle();

    if (m_gammaView) {
      gammaDescriptor.imageView = m_gammaView->handle();
      gammaDescriptor.imageLayout = m_gammaView->image()->info().layout;
    }

    VkDescriptorImageInfo hudDescriptor = { };

    if (m_hudView) {
      hudDescriptor.imageView = m_hudView->handle();
      hudDescriptor.imageLayout = m_hudImage->info().layout;
    }

    VkDescriptorImageInfo cursorDescriptor = { };
    cursorDescriptor.sampler = m_samplerCursorNearest->handle();

    if (m_cursorView) {
      VkExtent3D extent = m_cursorImage->info().extent;

      if (m_cursorRect.extent.width != extent.width
       || m_cursorRect.extent.height != extent.height)
        cursorDescriptor.sampler = m_samplerCursorLinear->handle();

      cursorDescriptor.imageLayout = m_cursorImage->info().layout;
      cursorDescriptor.imageView = m_cursorView->handle();
    }

    std::array<VkWriteDescriptorSet, 4> descriptorWrites = {{
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageDescriptor },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &gammaDescriptor },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 2, 0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &hudDescriptor },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &cursorDescriptor },
    }};

    ctx.cmd->updateDescriptorSets(
      descriptorWrites.size(), descriptorWrites.data());

    ctx.cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
      set, 0, nullptr);

    PushConstants args = { };
    args.srcOffset = srcRect.offset;
    args.srcExtent = srcRect.extent;
    args.dstOffset = dstRect.offset;
    args.cursorOffset = m_cursorRect.offset;
    args.cursorExtent = m_cursorRect.extent;

    ctx.cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
      m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
      0, sizeof(args), &args);

    ctx.cmd->cmdDraw(3, 1, 0, 0);

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      ctx.cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    // Make sure to keep used resources alive
    ctx.cmd->track(srcView->image(), DxvkAccess::Read);
    ctx.cmd->track(dstView->image(), DxvkAccess::Write);

    if (m_gammaImage)
      ctx.cmd->track(m_gammaImage, DxvkAccess::Read);

    if (m_hudImage)
      ctx.cmd->track(m_hudImage, DxvkAccess::Read);

    if (m_cursorImage)
      ctx.cmd->track(m_cursorImage, DxvkAccess::Read);

    ctx.cmd->track(m_samplerGamma);
    ctx.cmd->track(m_samplerPresent);
    ctx.cmd->track(m_samplerCursorLinear);
    ctx.cmd->track(m_samplerCursorNearest);
  }


  void DxvkSwapchainBlitter::renderHudImage(
    const DxvkContextObjects&         ctx,
          VkExtent3D                  extent) {
    if (m_hud->empty())
      return;

    if (!m_hudImage || m_hudImage->info().extent != extent)
      createHudImage(extent);

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture))) {
      ctx.cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xdcc0f0, "HUD render"));
    }

    // Reset image
    VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = m_hudImage->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_hudImage->handle();
    barrier.subresourceRange = m_hudView->imageSubresources();

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);
    m_hudImage->trackInitialization(barrier.subresourceRange);

    // Render actual HUD image
    VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachmentInfo.imageView = m_hudView->handle();
    attachmentInfo.imageLayout = m_hudImage->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderInfo.renderArea.offset = { 0u, 0u };
    renderInfo.renderArea.extent = { extent.width, extent.height };
    renderInfo.layerCount = 1u;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &attachmentInfo;

    ctx.cmd->cmdBeginRendering(&renderInfo);

    m_hud->render(ctx, m_hudView);

    ctx.cmd->cmdEndRendering();

    // Make image shader-readable
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.oldLayout = m_hudImage->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    barrier.newLayout = m_hudImage->info().layout;

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      ctx.cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    ctx.cmd->track(m_hudImage, DxvkAccess::Write);
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
    viewInfo.usage          = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT;
    viewInfo.format         = imageInfo.format;
    viewInfo.aspects        = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.mipIndex       = 0u;
    viewInfo.mipCount       = 1u;
    viewInfo.layerIndex     = 0u;
    viewInfo.layerCount     = 1u;

    m_hudView = m_hudImage->createView(viewInfo);
  }


  void DxvkSwapchainBlitter::destroyHudImage() {
    m_hudImage = nullptr;
    m_hudView = nullptr;
  }


  void DxvkSwapchainBlitter::renderCursor(
    const DxvkContextObjects&         ctx,
    const Rc<DxvkImageView>&          dstView) {
    if (!m_cursorRect.extent.width || !m_cursorRect.extent.height)
      return;

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture))) {
      ctx.cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
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

    ctx.cmd->cmdSetViewport(1, &viewport);

    VkRect2D scissor = { };
    scissor.extent.width = dstExtent.width;
    scissor.extent.height = dstExtent.height;

    ctx.cmd->cmdSetScissor(1, &scissor);

    DxvkCursorPipelineKey key = { };
    key.dstFormat = dstView->info().format;
    key.dstSpace = dstView->image()->info().colorSpace;

    VkPipeline pipeline = getCursorPipeline(key);

    ctx.cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDescriptorSet set = ctx.descriptorPool->alloc(m_cursorSetLayout);

    VkExtent3D cursorExtent = m_cursorImage->info().extent;

    bool filterLinear = m_cursorRect.extent.width != cursorExtent.width
                     || m_cursorRect.extent.height != cursorExtent.height;

    VkDescriptorImageInfo imageDescriptor = { };
    imageDescriptor.sampler = filterLinear
      ? m_samplerCursorLinear->handle()
      : m_samplerCursorNearest->handle();
    imageDescriptor.imageView = m_cursorView->handle();
    imageDescriptor.imageLayout = m_cursorImage->info().layout;

    std::array<VkWriteDescriptorSet, 1> descriptorWrites = {{
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageDescriptor },
    }};

    ctx.cmd->updateDescriptorSets(
      descriptorWrites.size(), descriptorWrites.data());

    ctx.cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, m_cursorPipelineLayout,
      set, 0, nullptr);

    CursorPushConstants args = { };
    args.dstExtent = { dstExtent.width, dstExtent.height };
    args.cursorOffset = m_cursorRect.offset;
    args.cursorExtent = m_cursorRect.extent;

    ctx.cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer,
      m_cursorPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
      0, sizeof(args), &args);

    ctx.cmd->cmdDraw(4, 1, 0, 0);

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      ctx.cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);

    ctx.cmd->track(m_cursorImage, DxvkAccess::Write);
  }


  void DxvkSwapchainBlitter::uploadGammaImage(
    const DxvkContextObjects&         ctx) {
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
    const DxvkContextObjects&         ctx) {
    uploadTexture(ctx, m_cursorImage, m_cursorBuffer);
    m_cursorBuffer = nullptr;
  }


  void DxvkSwapchainBlitter::uploadTexture(
    const DxvkContextObjects&         ctx,
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

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);
    image->trackInitialization(barrier.subresourceRange);

    DxvkBufferSliceHandle bufferSlice = buffer->getSliceHandle();

    VkBufferImageCopy2 copyRegion = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
    copyRegion.bufferOffset = bufferSlice.offset;
    copyRegion.imageExtent = image->info().extent;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1u;

    VkCopyBufferToImageInfo2 copy = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
    copy.srcBuffer = bufferSlice.handle;
    copy.dstImage = image->handle();
    copy.dstImageLayout = image->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy.regionCount = 1;
    copy.pRegions = &copyRegion;

    ctx.cmd->cmdCopyBufferToImage(DxvkCmdBuffer::ExecBuffer, &copy);

    barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = image->info().stages;
    barrier.dstAccessMask = image->info().access;
    barrier.oldLayout = barrier.newLayout;
    barrier.newLayout = image->info().layout;

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    ctx.cmd->track(buffer, DxvkAccess::Read);
    ctx.cmd->track(image, DxvkAccess::Write);
  }


  void DxvkSwapchainBlitter::createSampler() {
    DxvkSamplerKey samplerInfo = { };
    samplerInfo.setFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
      VK_SAMPLER_MIPMAP_MODE_NEAREST);
    samplerInfo.setAddressModes(
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
    samplerInfo.setUsePixelCoordinates(true);
 
    m_samplerPresent = m_device->createSampler(samplerInfo);

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


  void DxvkSwapchainBlitter::createShaders() {
    createShaderModule(m_shaderVsBlit, VK_SHADER_STAGE_VERTEX_BIT,
      sizeof(dxvk_present_vert), dxvk_present_vert);
    createShaderModule(m_shaderFsBlit, VK_SHADER_STAGE_FRAGMENT_BIT,
      sizeof(dxvk_present_frag_blit), dxvk_present_frag_blit);
    createShaderModule(m_shaderFsCopy, VK_SHADER_STAGE_FRAGMENT_BIT,
      sizeof(dxvk_present_frag), dxvk_present_frag);
    createShaderModule(m_shaderFsMsBlit, VK_SHADER_STAGE_FRAGMENT_BIT,
      sizeof(dxvk_present_frag_ms_blit), dxvk_present_frag_ms_blit);

    if (m_device->features().amdShaderFragmentMask) {
      createShaderModule(m_shaderFsMsResolve, VK_SHADER_STAGE_FRAGMENT_BIT,
        sizeof(dxvk_present_frag_ms_amd), dxvk_present_frag_ms_amd);
    } else {
      createShaderModule(m_shaderFsMsResolve, VK_SHADER_STAGE_FRAGMENT_BIT,
        sizeof(dxvk_present_frag_ms), dxvk_present_frag_ms);
    }

    createShaderModule(m_shaderVsCursor, VK_SHADER_STAGE_VERTEX_BIT,
      sizeof(dxvk_cursor_vert), dxvk_cursor_vert);
    createShaderModule(m_shaderFsCursor, VK_SHADER_STAGE_FRAGMENT_BIT,
      sizeof(dxvk_cursor_frag), dxvk_cursor_frag);
  }


  void DxvkSwapchainBlitter::createShaderModule(
          ShaderModule&               shader,
          VkShaderStageFlagBits       stage,
          size_t                      size,
    const uint32_t*                   code) {
    shader.moduleInfo.codeSize = size;
    shader.moduleInfo.pCode = code;

    shader.stageInfo.stage = stage;
    shader.stageInfo.pName = "main";

    if (m_device->features().khrMaintenance5.maintenance5
     || m_device->features().extGraphicsPipelineLibrary.graphicsPipelineLibrary) {
      shader.stageInfo.pNext = &shader.moduleInfo;
      return;
    }

    auto vk = m_device->vkd();

    VkResult vr = vk->vkCreateShaderModule(vk->device(),
      &shader.moduleInfo, nullptr, &shader.stageInfo.module);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create swap chain blit shader module: ", vr));
  }


  VkDescriptorSetLayout DxvkSwapchainBlitter::createSetLayout() {
    auto vk = m_device->vkd();

    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = bindings.size();
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateDescriptorSetLayout(vk->device(), &info, nullptr, &layout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create swap chain blit descriptor set layout: ", vr));

    return layout;
  }


  VkDescriptorSetLayout DxvkSwapchainBlitter::createCursorSetLayout() {
    auto vk = m_device->vkd();

    std::array<VkDescriptorSetLayoutBinding, 1> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = bindings.size();
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateDescriptorSetLayout(vk->device(), &info, nullptr, &layout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create swap chain cursor descriptor set layout: ", vr));

    return layout;
  }


  VkPipelineLayout DxvkSwapchainBlitter::createPipelineLayout() {
    auto vk = m_device->vkd();

    VkPushConstantRange pushConst = { };
    pushConst.size = sizeof(PushConstants);
    pushConst.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount = 1;
    info.pSetLayouts = &m_setLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushConst;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreatePipelineLayout(vk->device(), &info, nullptr, &layout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create swap chain blit pipeline layout: ", vr));

    return layout;
  }


  VkPipelineLayout DxvkSwapchainBlitter::createCursorPipelineLayout() {
    auto vk = m_device->vkd();

    VkPushConstantRange pushConst = { };
    pushConst.size = sizeof(CursorPushConstants);
    pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount = 1;
    info.pSetLayouts = &m_cursorSetLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushConst;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreatePipelineLayout(vk->device(), &info, nullptr, &layout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create swap chain cursor pipeline layout: ", vr));

    return layout;
  }


  VkPipeline DxvkSwapchainBlitter::createPipeline(
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

    std::array<VkPipelineShaderStageCreateInfo, 2> blitStages = { };
    blitStages[0] = m_shaderVsBlit.stageInfo;

    if (key.srcSamples == VK_SAMPLE_COUNT_1_BIT)
      blitStages[1] = key.needsBlit ? m_shaderFsBlit.stageInfo : m_shaderFsCopy.stageInfo;
    else
      blitStages[1] = key.needsBlit ? m_shaderFsMsBlit.stageInfo : m_shaderFsMsResolve.stageInfo;

    blitStages[1].pSpecializationInfo = &specInfo;

    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rtInfo.colorAttachmentCount = 1;
    rtInfo.pColorAttachmentFormats = &key.dstFormat;

    VkPipelineVertexInputStateCreateInfo viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

    VkPipelineRasterizationStateCreateInfo rsState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsState.cullMode = VK_CULL_MODE_NONE;
    rsState.polygonMode = VK_POLYGON_MODE_FILL;
    rsState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.lineWidth = 1.0f;

    constexpr uint32_t sampleMask = 0x1;

    VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msState.pSampleMask = &sampleMask;

    VkPipelineColorBlendAttachmentState cbAttachment = { };
    cbAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbState.attachmentCount = 1;
    cbState.pAttachments = &cbAttachment;

    static const std::array<VkDynamicState, 2> dynStates = {
      VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    };

    VkPipelineDynamicStateCreateInfo dynState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount = dynStates.size();
    dynState.pDynamicStates = dynStates.data();

    VkGraphicsPipelineCreateInfo blitInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rtInfo };
    blitInfo.stageCount = blitStages.size();
    blitInfo.pStages = blitStages.data();
    blitInfo.pVertexInputState = &viState;
    blitInfo.pInputAssemblyState = &iaState;
    blitInfo.pViewportState = &vpState;
    blitInfo.pRasterizationState = &rsState;
    blitInfo.pMultisampleState = &msState;
    blitInfo.pColorBlendState = &cbState;
    blitInfo.pDynamicState = &dynState;
    blitInfo.layout = m_pipelineLayout;
    blitInfo.basePipelineIndex = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE,
      1, &blitInfo, nullptr, &pipeline);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create swap chain blit pipeline: ", vr));

    return pipeline;
  }


  VkPipeline DxvkSwapchainBlitter::getPipeline(
    const DxvkSwapchainPipelineKey&   key) {
    auto entry = m_pipelines.find(key);

    if (entry != m_pipelines.end())
      return entry->second;

    VkPipeline pipeline = createPipeline(key);
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

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = { };
    stages[0] = m_shaderVsCursor.stageInfo;
    stages[1] = m_shaderFsCursor.stageInfo;
    stages[1].pSpecializationInfo = &specInfo;

    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rtInfo.colorAttachmentCount = 1;
    rtInfo.pColorAttachmentFormats = &key.dstFormat;

    VkPipelineVertexInputStateCreateInfo viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

    VkPipelineRasterizationStateCreateInfo rsState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsState.cullMode = VK_CULL_MODE_NONE;
    rsState.polygonMode = VK_POLYGON_MODE_FILL;
    rsState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.lineWidth = 1.0f;

    constexpr uint32_t sampleMask = 0x1;

    VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msState.pSampleMask = &sampleMask;

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

    VkPipelineColorBlendStateCreateInfo cbState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbState.attachmentCount = 1;
    cbState.pAttachments = &cbAttachment;

    static const std::array<VkDynamicState, 2> dynStates = {
      VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    };

    VkPipelineDynamicStateCreateInfo dynState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount = dynStates.size();
    dynState.pDynamicStates = dynStates.data();

    VkGraphicsPipelineCreateInfo blitInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rtInfo };
    blitInfo.stageCount = stages.size();
    blitInfo.pStages = stages.data();
    blitInfo.pVertexInputState = &viState;
    blitInfo.pInputAssemblyState = &iaState;
    blitInfo.pViewportState = &vpState;
    blitInfo.pRasterizationState = &rsState;
    blitInfo.pMultisampleState = &msState;
    blitInfo.pColorBlendState = &cbState;
    blitInfo.pDynamicState = &dynState;
    blitInfo.layout = m_cursorPipelineLayout;
    blitInfo.basePipelineIndex = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE,
      1, &blitInfo, nullptr, &pipeline);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create swap chain blit pipeline: ", vr));

    return pipeline;
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
