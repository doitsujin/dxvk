#include "dxvk_hud_renderer.h"

#include <hud_text_comp.h>
#include <hud_text_frag.h>
#include <hud_text_vert.h>

namespace dxvk::hud {
  
  struct HudGlyphGpuData {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t originX;
    int16_t originY;
    int16_t advance;
    int16_t padding[1];
  };


  struct HudFontGpuData {
    float size;
    uint32_t padding[3];
    HudGlyphGpuData glyphs[256];
  };


  struct ComputePushConstants {
    uint32_t drawCount;
  };


  static const std::array<VkSpecializationMapEntry, 2> HudSpecConstantMap = {{
    { 0, offsetof(HudSpecConstants, dstSpace),  sizeof(VkColorSpaceKHR) },
    { 1, offsetof(HudSpecConstants, dstIsSrgb), sizeof(VkBool32) },
  }};



  HudRenderer::HudRenderer(const Rc<DxvkDevice>& device)
  : m_device               (device),
    m_textGfxSetLayout     (createGfxSetLayout()),
    m_textPipelineLayout   (createPipelineLayout()) {
    createComputePipeline();

    // createShaderModule(m_textCs, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(hud_text_comp), hud_text_comp);
    createShaderModule(m_textVs, VK_SHADER_STAGE_VERTEX_BIT, sizeof(hud_text_vert), hud_text_vert);
    createShaderModule(m_textFs, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(hud_text_frag), hud_text_frag);
  }
  
  
  HudRenderer::~HudRenderer() {
    auto vk = m_device->vkd();

    for (const auto& p : m_textPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second, nullptr);

    // vk->vkDestroyShaderModule(vk->device(), m_textCs.stageInfo.module, nullptr);
    vk->vkDestroyShaderModule(vk->device(), m_textVs.stageInfo.module, nullptr);
    vk->vkDestroyShaderModule(vk->device(), m_textFs.stageInfo.module, nullptr);
    
    vk->vkDestroyPipeline(vk->device(), m_computePipeline, nullptr);
    vk->vkDestroyPipelineLayout(vk->device(), m_computePipelineLayout, nullptr);
    vk->vkDestroyDescriptorSetLayout(vk->device(), m_textComputeSetLayout, nullptr);

    vk->vkDestroyPipelineLayout(vk->device(), m_textPipelineLayout, nullptr);
    vk->vkDestroyDescriptorSetLayout(vk->device(), m_textGfxSetLayout, nullptr);
  }
  
  
  void HudRenderer::beginFrame(
    const DxvkContextObjects& ctx,
    const Rc<DxvkImageView>&  dstView,
    const HudOptions&         options) {
    if (unlikely(m_device->isDebugEnabled())) {
      ctx.cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
        vk::makeLabel(0xf0c0dc, "HUD"));
    }

    if (!m_fontTextureView) {
      createFontResources();
      uploadFontResources(ctx);
    }

    VkExtent3D extent = dstView->mipLevelExtent(0u);

    m_pushConstants.surfaceSize = { extent.width, extent.height };
    m_pushConstants.opacity = options.opacity;
    m_pushConstants.scale = options.scale;

    VkViewport viewport = { };
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = float(extent.width);
    viewport.height = float(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = { };
    scissor.offset = { 0u, 0u };
    scissor.extent = { extent.width, extent.height };

    ctx.cmd->cmdSetViewport(1, &viewport);
    ctx.cmd->cmdSetScissor(1, &scissor);
  }
  
  
  void HudRenderer::endFrame(
    const DxvkContextObjects& ctx) {
    if (unlikely(m_device->isDebugEnabled()))
      ctx.cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
  }


  void HudRenderer::drawText(
          uint32_t            size,
          HudPos              pos,
          uint32_t            color,
    const std::string&        text) {
    if (text.empty())
      return;

    auto& draw = m_textDraws.emplace_back();
    draw.textOffset = m_textData.size();
    draw.textLength = text.size();
    draw.fontSize = size;
    draw.posX = pos.x;
    draw.posY = pos.y;
    draw.color = color;

    m_textData.resize(draw.textOffset + draw.textLength);
    std::memcpy(&m_textData[draw.textOffset], text.data(), draw.textLength);
  }


  void HudRenderer::flushDraws(
    const DxvkContextObjects& ctx,
    const Rc<DxvkImageView>&  dstView,
    const HudOptions&         options) {
    if (m_textDraws.empty())
      return;

    // Align text size so that we're guaranteed to be able to put draw
    // parameters where we want, and can also upload data without
    // running into perf issues due to incomplete cache lines.
    size_t textSizeAligned = align(m_textData.size(), 256u);
    m_textData.resize(textSizeAligned);

    // We'll use indirect draws and then just use aligned subsections
    // of the data buffer to write our draw parameters
    size_t charInfoSize = align(m_textData.size() * sizeof(HudCharInfo), 256u);
    size_t drawInfoSize = align(m_textDraws.size() * sizeof(HudTextDrawInfo), 256u);
    size_t drawArgsSize = align(m_textDraws.size() * sizeof(VkDrawIndirectCommand), 256u);
    
    // m_textBuffer will be our buffer where we put everything that the shaders will need:
    // text data | character x-coordinates | draw infos | draw args

    // Align buffer size to something large so we don't recreate it all the time
    size_t bufferSize = align(textSizeAligned + charInfoSize + drawInfoSize + drawArgsSize, 2048u);

    if (!m_textBuffer || m_textBuffer->info().size < bufferSize) {
      DxvkBufferCreateInfo textBufferInfo = { };
      textBufferInfo.size = bufferSize;
      textBufferInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                           | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                           | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
                           | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      textBufferInfo.stages = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
                            | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                            | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      textBufferInfo.access = VK_ACCESS_SHADER_READ_BIT
                            | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
      textBufferInfo.debugName = "HUD text buffer";

      m_textBuffer = m_device->createBuffer(textBufferInfo,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      DxvkBufferViewKey textViewInfo = { };
      textViewInfo.format = VK_FORMAT_R8_UINT;
      textViewInfo.offset = 0u;
      textViewInfo.size = bufferSize;
      textViewInfo.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

      m_textBufferView = m_textBuffer->createView(textViewInfo);
    } else {
      // Discard and invalidate buffer so we can safely update it
      auto storage = m_textBuffer->assignStorage(Rc<DxvkResourceAllocation>(m_textBuffer->allocateStorage()));
      ctx.cmd->track(std::move(storage));
    }

    // Upload aligned text data in such a way that we write full cache lines
    std::memcpy(m_textBuffer->mapPtr(0), m_textData.data(), textSizeAligned);
    
    // Upload zeroes to character info to initialize
    std::memset(m_textBuffer->mapPtr(textSizeAligned), 65, charInfoSize);

    // Upload draw parameters and pad aligned region with zeroes
    size_t drawInfoCopySize = m_textDraws.size() * sizeof(HudTextDrawInfo);
    std::memcpy(m_textBuffer->mapPtr(textSizeAligned + charInfoSize), m_textDraws.data(), drawInfoCopySize);
    std::memset(m_textBuffer->mapPtr(textSizeAligned + charInfoSize + drawInfoCopySize), 0, drawInfoSize - drawInfoCopySize);

    // Emit indirect draw parameters
    size_t drawArgWriteSize = m_textDraws.size() * sizeof(VkDrawIndirectCommand);
    size_t drawArgOffset = textSizeAligned + charInfoSize + drawInfoSize;

    auto drawArgs = reinterpret_cast<VkDrawIndirectCommand*>(m_textBuffer->mapPtr(drawArgOffset));

    for (size_t i = 0; i < m_textDraws.size(); i++) {
      drawArgs[i].vertexCount = 12u * m_textDraws[i].textLength;
      drawArgs[i].instanceCount = 1u;
      drawArgs[i].firstVertex = 0u;
      drawArgs[i].firstInstance = 0u;
    }

    std::memset(m_textBuffer->mapPtr(drawArgOffset + drawArgWriteSize), 0, drawArgsSize - drawArgWriteSize);

    // Draw the actual text
    VkDescriptorBufferInfo charInfosDescriptor = m_textBuffer->getDescriptor(textSizeAligned, charInfoSize).buffer;
    VkDescriptorBufferInfo drawInfosDescriptor = m_textBuffer->getDescriptor(textSizeAligned + charInfoSize, drawInfoSize).buffer;
    VkDescriptorBufferInfo drawArgsDescriptor = m_textBuffer->getDescriptor(drawArgOffset, drawArgWriteSize).buffer;

    drawTextIndirect(ctx, getPipelineKey(dstView), charInfosDescriptor,
      drawArgsDescriptor, drawInfosDescriptor,
      m_textBufferView->handle(), m_textDraws.size());

    // Ensure all used resources are kept alive
    ctx.cmd->track(m_textBuffer, DxvkAccess::Read);
    ctx.cmd->track(m_fontBuffer, DxvkAccess::Read);
    ctx.cmd->track(m_fontTexture, DxvkAccess::Read);
    ctx.cmd->track(m_fontSampler);

    // Reset internal text buffers
    m_textDraws.clear();
    m_textData.clear();
  }


  void HudRenderer::drawTextIndirect(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const VkDescriptorBufferInfo& charInfos,
    const VkDescriptorBufferInfo& drawArgs,
    const VkDescriptorBufferInfo& drawInfos,
          VkBufferView        text,
          uint32_t            drawCount) {
    // Prepare font buffer, needed by both compute and gfx.

    VkDescriptorBufferInfo fontBufferDescriptor = m_fontBuffer->getDescriptor(0, m_fontBuffer->info().size).buffer;

    VkDescriptorImageInfo fontTextureDescriptor = { };
    fontTextureDescriptor.sampler = m_fontSampler->handle();
    fontTextureDescriptor.imageView = m_fontTextureView->handle();
    fontTextureDescriptor.imageLayout = m_fontTexture->info().layout;
    
    // -- Compute part
    
    VkDescriptorSet computeSet = ctx.descriptorPool->alloc(m_textComputeSetLayout);
    
    std::array<VkWriteDescriptorSet, 4> computeDescriptorWrites = {{
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        computeSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &fontBufferDescriptor },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        computeSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &charInfos },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        computeSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &drawInfos },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        computeSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, nullptr, nullptr, &text },
    }};

    ctx.cmd->updateDescriptorSets(
      computeDescriptorWrites.size(),
      computeDescriptorWrites.data());
    
    ctx.cmd->cmdBindPipeline(DxvkCmdBuffer::InitBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);

    ctx.cmd->cmdBindDescriptorSet(DxvkCmdBuffer::InitBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout,
      computeSet, 0, nullptr);

    ComputePushConstants pushConstants = { };
    pushConstants.drawCount = drawCount;

    ctx.cmd->cmdPushConstants(DxvkCmdBuffer::InitBuffer,
      m_computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
      0, sizeof(pushConstants), &pushConstants);

    static const int textComputeWorkgroupSize = 256;
    ctx.cmd->cmdDispatch(DxvkCmdBuffer::InitBuffer,
      (drawCount + textComputeWorkgroupSize - 1) / textComputeWorkgroupSize, 1, 1);
    
    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    
    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.memoryBarrierCount = 1u;
    depInfo.pMemoryBarriers = &barrier;
    
    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::InitBuffer, &depInfo);
    
    // -- Graphics part
    
    // Bind the correct pipeline for the swap chain
    VkPipeline pipeline = getPipeline(key);

    ctx.cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind resources
    VkDescriptorSet gfxSet = ctx.descriptorPool->alloc(m_textGfxSetLayout);

    std::array<VkWriteDescriptorSet, 5> gfxDescriptorWrites = {{
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        gfxSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &fontBufferDescriptor },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        gfxSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &charInfos },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        gfxSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &drawInfos },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        gfxSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, nullptr, nullptr, &text },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        gfxSet, 4, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &fontTextureDescriptor },
    }};

    ctx.cmd->updateDescriptorSets(
      gfxDescriptorWrites.size(),
      gfxDescriptorWrites.data());

    ctx.cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, m_textPipelineLayout,
      gfxSet, 0, nullptr);

    ctx.cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer, m_textPipelineLayout,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      0, sizeof(m_pushConstants), &m_pushConstants);

    // Emit the actual draw call
    ctx.cmd->cmdDrawIndirect(drawArgs.buffer, drawArgs.offset,
      drawCount, sizeof(VkDrawIndirectCommand));
  }


  HudPipelineKey HudRenderer::getPipelineKey(
    const Rc<DxvkImageView>&  dstView) const {
    HudPipelineKey key;
    key.format = dstView->info().format;
    key.colorSpace = dstView->image()->info().colorSpace;
    return key;
  }


  HudSpecConstants HudRenderer::getSpecConstants(
    const HudPipelineKey&     key) const {
    HudSpecConstants result = { };
    result.dstSpace = key.colorSpace;
    result.dstIsSrgb = lookupFormatInfo(key.format)->flags.test(DxvkFormatFlag::ColorSpaceSrgb);
    return result;
  }


  HudPushConstants HudRenderer::getPushConstants() const {
    return m_pushConstants;
  }


  VkSpecializationInfo HudRenderer::getSpecInfo(
    const HudSpecConstants*   constants) const {
    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = HudSpecConstantMap.size();
    specInfo.pMapEntries = HudSpecConstantMap.data();
    specInfo.dataSize = sizeof(*constants);
    specInfo.pData = constants;
    return specInfo;
  }


  void HudRenderer::createShaderModule(
          HudShaderModule&    shader,
          VkShaderStageFlagBits stage,
          size_t              size,
    const uint32_t*           code) const {
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


  void HudRenderer::createFontResources() {
    DxvkBufferCreateInfo fontBufferInfo;
    fontBufferInfo.size = sizeof(HudFontGpuData);
    fontBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                         | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    fontBufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                          | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    fontBufferInfo.access = VK_ACCESS_TRANSFER_WRITE_BIT
                          | VK_ACCESS_TRANSFER_READ_BIT
                          | VK_ACCESS_SHADER_READ_BIT;
    fontBufferInfo.debugName = "HUD font metadata";

    m_fontBuffer = m_device->createBuffer(fontBufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    DxvkImageCreateInfo fontTextureInfo;
    fontTextureInfo.type = VK_IMAGE_TYPE_2D;
    fontTextureInfo.format = VK_FORMAT_R8_UNORM;
    fontTextureInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    fontTextureInfo.extent = { g_hudFont.width, g_hudFont.height, 1u };
    fontTextureInfo.numLayers = 1u;
    fontTextureInfo.mipLevels = 1u;
    fontTextureInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                          | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                          | VK_IMAGE_USAGE_SAMPLED_BIT;
    fontTextureInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                           | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    fontTextureInfo.access = VK_ACCESS_TRANSFER_WRITE_BIT
                           | VK_ACCESS_TRANSFER_READ_BIT
                           | VK_ACCESS_SHADER_READ_BIT;
    fontTextureInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    fontTextureInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    fontTextureInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    fontTextureInfo.debugName = "HUD font texture";

    m_fontTexture = m_device->createImage(fontTextureInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    DxvkImageViewKey fontTextureViewInfo;
    fontTextureViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    fontTextureViewInfo.format = VK_FORMAT_R8_UNORM;
    fontTextureViewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    fontTextureViewInfo.aspects = VK_IMAGE_ASPECT_COLOR_BIT;
    fontTextureViewInfo.mipIndex = 0u;
    fontTextureViewInfo.mipCount = 1u;
    fontTextureViewInfo.layerIndex = 0u;
    fontTextureViewInfo.layerCount = 1u;

    m_fontTextureView = m_fontTexture->createView(fontTextureViewInfo);

    DxvkSamplerKey samplerInfo;
    samplerInfo.setFilter(VK_FILTER_LINEAR,
      VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST);
    samplerInfo.setAddressModes(
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    samplerInfo.setUsePixelCoordinates(true);

    m_fontSampler = m_device->createSampler(samplerInfo);
  }


  void HudRenderer::uploadFontResources(
    const DxvkContextObjects& ctx) {
    size_t bufferDataSize = sizeof(HudFontGpuData);
    size_t textureDataSize = g_hudFont.width * g_hudFont.height;

    DxvkBufferCreateInfo bufferInfo;
    bufferInfo.size = bufferDataSize + textureDataSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    bufferInfo.access = VK_ACCESS_TRANSFER_READ_BIT;

    auto uploadBuffer = m_device->createBuffer(bufferInfo,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    HudFontGpuData glyphData = { };
    glyphData.size = float(g_hudFont.size);

    for (size_t i = 0; i < g_hudFont.charCount; i++) {
      auto& src = g_hudFont.glyphs[i];
      auto& dst = glyphData.glyphs[src.codePoint];

      dst.x = src.x;
      dst.y = src.y;
      dst.w = src.w;
      dst.h = src.h;
      dst.originX = src.originX;
      dst.originY = src.originY;
      dst.advance = src.advance;
    }

    std::memcpy(uploadBuffer->mapPtr(0), &glyphData, bufferDataSize);
    std::memcpy(uploadBuffer->mapPtr(bufferDataSize), g_hudFont.texture, textureDataSize);

    auto uploadSlice = uploadBuffer->getSliceHandle();
    auto fontSlice = m_fontBuffer->getSliceHandle();

    VkImageMemoryBarrier2 imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = m_fontTexture->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = m_fontTexture->handle();
    imageBarrier.subresourceRange = m_fontTexture->getAvailableSubresources();

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.imageMemoryBarrierCount = 1u;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::InitBuffer, &depInfo);
    m_fontTexture->trackInitialization(imageBarrier.subresourceRange);

    VkBufferCopy2 bufferRegion = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
    bufferRegion.srcOffset = uploadSlice.offset;
    bufferRegion.dstOffset = fontSlice.offset;
    bufferRegion.size = bufferDataSize;

    VkCopyBufferInfo2 bufferCopy = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    bufferCopy.srcBuffer = uploadSlice.handle;
    bufferCopy.dstBuffer = fontSlice.handle;
    bufferCopy.regionCount = 1;
    bufferCopy.pRegions = &bufferRegion;

    ctx.cmd->cmdCopyBuffer(DxvkCmdBuffer::InitBuffer, &bufferCopy);

    VkBufferImageCopy2 imageRegion = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
    imageRegion.bufferOffset = uploadSlice.offset + bufferDataSize;
    imageRegion.imageExtent = m_fontTexture->info().extent;
    imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageRegion.imageSubresource.layerCount = 1u;

    VkCopyBufferToImageInfo2 imageCopy = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
    imageCopy.srcBuffer = uploadSlice.handle;
    imageCopy.dstImage = m_fontTexture->handle();
    imageCopy.dstImageLayout = m_fontTexture->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    imageCopy.regionCount = 1;
    imageCopy.pRegions = &imageRegion;

    ctx.cmd->cmdCopyBufferToImage(DxvkCmdBuffer::InitBuffer, &imageCopy);

    VkMemoryBarrier2 memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    memoryBarrier.dstAccessMask = m_fontBuffer->info().access;
    memoryBarrier.dstStageMask = m_fontBuffer->info().stages;

    imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    imageBarrier.dstAccessMask = m_fontTexture->info().access;
    imageBarrier.dstStageMask = m_fontTexture->info().stages;
    imageBarrier.oldLayout = m_fontTexture->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    imageBarrier.newLayout = m_fontTexture->info().layout;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = m_fontTexture->handle();
    imageBarrier.subresourceRange = m_fontTexture->getAvailableSubresources();

    depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.memoryBarrierCount = 1u;
    depInfo.pMemoryBarriers = &memoryBarrier;
    depInfo.imageMemoryBarrierCount = 1u;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::InitBuffer, &depInfo);

    ctx.cmd->track(uploadBuffer, DxvkAccess::Read);
    ctx.cmd->track(m_fontBuffer, DxvkAccess::Write);
    ctx.cmd->track(m_fontTexture, DxvkAccess::Write);
  }


  void HudRenderer::createComputePipeline() {
    auto vk = m_device->vkd();

    static const std::array<VkDescriptorSetLayoutBinding, 4> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_COMPUTE_BIT },
      { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_COMPUTE_BIT },
      { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_COMPUTE_BIT },
      { 3, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1, VK_SHADER_STAGE_COMPUTE_BIT },
    }};
    
    VkDescriptorSetLayoutCreateInfo setLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setLayoutInfo.bindingCount = bindings.size();
    setLayoutInfo.pBindings = bindings.data();

    VkResult vr = vk->vkCreateDescriptorSetLayout(vk->device(),
      &setLayoutInfo, nullptr, &m_textComputeSetLayout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create HUD text compute set layout: ", vr));
    
    VkPushConstantRange pushConstantRange = { };
    pushConstantRange.offset = 0u;
    pushConstantRange.size = sizeof(ComputePushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = 1u;
    pipelineLayoutInfo.pSetLayouts = &m_textComputeSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    vr = vk->vkCreatePipelineLayout(vk->device(),
      &pipelineLayoutInfo, nullptr, &m_computePipelineLayout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create HUD compute pipeline layout: ", vr));
    
    HudShaderModule shader = { };
    createShaderModule(shader, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(hud_text_comp), hud_text_comp);

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.stage = shader.stageInfo;
    info.layout = m_computePipelineLayout;
    info.basePipelineIndex = -1;

    vr = vk->vkCreateComputePipelines(vk->device(),
      VK_NULL_HANDLE, 1, &info, nullptr, &m_computePipeline);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create HUD compute pipeline: ", vr));

    vk->vkDestroyShaderModule(vk->device(), shader.stageInfo.module, nullptr);
  }


  VkDescriptorSetLayout HudRenderer::createGfxSetLayout() {
    auto vk = m_device->vkd();

    static const std::array<VkDescriptorSetLayoutBinding, 5> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_VERTEX_BIT   },
      { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_VERTEX_BIT   },
      { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_VERTEX_BIT   },
      { 3, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1, VK_SHADER_STAGE_VERTEX_BIT   },
      { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = bindings.size();
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateDescriptorSetLayout(vk->device(), &info, nullptr, &layout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create HUD descriptor set layout: ", vr));

    return layout;
  }


  VkPipelineLayout HudRenderer::createPipelineLayout() {
    auto vk = m_device->vkd();

    VkPushConstantRange pushConstantRange = { };
    pushConstantRange.offset = 0u;
    pushConstantRange.size = sizeof(HudPushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount = 1;
    info.pSetLayouts = &m_textGfxSetLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushConstantRange;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreatePipelineLayout(vk->device(), &info, nullptr, &layout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create HUD descriptor set layout: ", vr));

    return layout;
  }


  VkPipeline HudRenderer::createPipeline(
    const HudPipelineKey&     key) {
    auto vk = m_device->vkd();

    HudSpecConstants specConstants = getSpecConstants(key);
    VkSpecializationInfo specInfo = getSpecInfo(&specConstants);

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = { };
    stages[0] = m_textVs.stageInfo;
    stages[1] = m_textFs.stageInfo;
    stages[1].pSpecializationInfo = &specInfo;

    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rtInfo.colorAttachmentCount = 1;
    rtInfo.pColorAttachmentFormats = &key.format;

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

    VkPipelineColorBlendStateCreateInfo cbOpaqueState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbOpaqueState.attachmentCount = 1;
    cbOpaqueState.pAttachments = &cbAttachment;

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
    blitInfo.pColorBlendState = &cbOpaqueState;
    blitInfo.pDynamicState = &dynState;
    blitInfo.layout = m_textPipelineLayout;
    blitInfo.basePipelineIndex = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE,
      1, &blitInfo, nullptr, &pipeline);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create swap chain blit pipeline: ", vr));

    return pipeline;
  }


  VkPipeline HudRenderer::getPipeline(
    const HudPipelineKey&     key) {
    auto entry = m_textPipelines.find(key);

    if (entry != m_textPipelines.end())
      return entry->second;

    VkPipeline pipeline = createPipeline(key);
    m_textPipelines.insert({ key, pipeline });
    return pipeline;
  }

}
