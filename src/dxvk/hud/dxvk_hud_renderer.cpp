#include "dxvk_hud_renderer.h"

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
  };


  struct HudFontGpuData {
    float size;
    float advance;
    uint32_t padding[2];
    HudGlyphGpuData glyphs[256];
  };



  static const std::array<VkSpecializationMapEntry, 2> HudSpecConstantMap = {{
    { 0, offsetof(HudSpecConstants, dstSpace),  sizeof(VkColorSpaceKHR) },
    { 1, offsetof(HudSpecConstants, dstIsSrgb), sizeof(VkBool32) },
  }};



  HudRenderer::HudRenderer(const Rc<DxvkDevice>& device)
  : m_device              (device),
    m_textPipelineLayout  (createPipelineLayout()) {

  }
  
  
  HudRenderer::~HudRenderer() {
    auto vk = m_device->vkd();

    for (const auto& p : m_textPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second, nullptr);
  }
  
  
  void HudRenderer::beginFrame(
    const Rc<DxvkCommandList>&ctx,
    const Rc<DxvkImageView>&  dstView,
    const HudOptions&         options) {
    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture))) {
      ctx->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer,
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
    m_pushConstants.sampler = m_fontSampler->getDescriptor().samplerIndex;

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

    ctx->cmdSetViewport(1, &viewport);
    ctx->cmdSetScissor(1, &scissor);
  }
  
  
  void HudRenderer::endFrame(
    const Rc<DxvkCommandList>&ctx) {
    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      ctx->cmdEndDebugUtilsLabel(DxvkCmdBuffer::ExecBuffer);
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
    const Rc<DxvkCommandList>&ctx,
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
    size_t drawInfoSize = align(m_textDraws.size() * sizeof(HudTextDrawInfo), 256u);
    size_t drawArgsSize = align(m_textDraws.size() * sizeof(VkDrawIndirectCommand), 256u);

    // Align buffer size to something large so we don't recreate it all the time
    size_t bufferSize = align(textSizeAligned + drawInfoSize + drawArgsSize, 2048u);

    if (!m_textBuffer || m_textBuffer->info().size < bufferSize) {
      DxvkBufferCreateInfo textBufferInfo = { };
      textBufferInfo.size = bufferSize;
      textBufferInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                           | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                           | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
      textBufferInfo.stages = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
                            | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
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
      ctx->track(std::move(storage));
    }

    // Upload aligned text data in such a way that we write full cache lines
    std::memcpy(m_textBuffer->mapPtr(0), m_textData.data(), textSizeAligned);

    // Upload draw parameters and pad aligned region with zeroes
    size_t drawInfoCopySize = m_textDraws.size() * sizeof(HudTextDrawInfo);
    std::memcpy(m_textBuffer->mapPtr(textSizeAligned), m_textDraws.data(), drawInfoCopySize);
    std::memset(m_textBuffer->mapPtr(textSizeAligned + drawInfoCopySize), 0, drawInfoSize - drawInfoCopySize);

    // Emit indirect draw parameters
    size_t drawArgWriteSize = m_textDraws.size() * sizeof(VkDrawIndirectCommand);
    size_t drawArgOffset = textSizeAligned + drawInfoSize;

    auto drawArgs = reinterpret_cast<VkDrawIndirectCommand*>(m_textBuffer->mapPtr(drawArgOffset));

    for (size_t i = 0; i < m_textDraws.size(); i++) {
      drawArgs[i].vertexCount = 6u * m_textDraws[i].textLength;
      drawArgs[i].instanceCount = 1u;
      drawArgs[i].firstVertex = 0u;
      drawArgs[i].firstInstance = 0u;
    }

    std::memset(m_textBuffer->mapPtr(drawArgOffset + drawArgWriteSize), 0, drawArgsSize - drawArgWriteSize);

    // Draw the actual text
    DxvkResourceBufferInfo textBufferInfo = m_textBuffer->getSliceInfo(textSizeAligned, drawInfoSize);
    DxvkResourceBufferInfo drawBufferInfo = m_textBuffer->getSliceInfo(drawArgOffset, drawArgWriteSize);

    drawTextIndirect(ctx, getPipelineKey(dstView),
      drawBufferInfo, textBufferInfo,
      m_textBufferView, m_textDraws.size());

    // Ensure all used resources are kept alive
    ctx->track(m_textBuffer, DxvkAccess::Read);
    ctx->track(m_fontBuffer, DxvkAccess::Read);
    ctx->track(m_fontTexture, DxvkAccess::Read);
    ctx->track(m_fontSampler);

    // Reset internal text buffers
    m_textDraws.clear();
    m_textData.clear();
  }


  void HudRenderer::drawTextIndirect(
    const Rc<DxvkCommandList>&ctx,
    const HudPipelineKey&     key,
    const DxvkResourceBufferInfo& drawArgs,
    const DxvkResourceBufferInfo& drawInfos,
    const Rc<DxvkBufferView>& textView,
          uint32_t            drawCount) {
    // Bind the correct pipeline for the swap chain
    VkPipeline pipeline = getPipeline(key);

    ctx->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind resources
    std::array<DxvkDescriptorWrite, 4u> descriptors = { };
    descriptors[0u].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptors[0u].buffer = m_fontBuffer->getSliceInfo();

    descriptors[1u].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptors[1u].buffer = drawInfos;

    descriptors[2u].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    descriptors[2u].descriptor = textView->getDescriptor(false);

    descriptors[3u].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptors[3u].descriptor = m_fontTextureView->getDescriptor();

    ctx->bindResources(DxvkCmdBuffer::ExecBuffer,
      m_textPipelineLayout, descriptors.size(), descriptors.data(),
      sizeof(m_pushConstants), &m_pushConstants);

    // Emit the actual draw call
    ctx->cmdDrawIndirect(drawArgs.buffer, drawArgs.offset,
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
    const Rc<DxvkCommandList>&ctx) {
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
    glyphData.advance = float(g_hudFont.advance);

    for (size_t i = 0; i < g_hudFont.charCount; i++) {
      auto& src = g_hudFont.glyphs[i];
      auto& dst = glyphData.glyphs[src.codePoint];

      dst.x = src.x;
      dst.y = src.y;
      dst.w = src.w;
      dst.h = src.h;
      dst.originX = src.originX;
      dst.originY = src.originY;
    }

    std::memcpy(uploadBuffer->mapPtr(0), &glyphData, bufferDataSize);
    std::memcpy(uploadBuffer->mapPtr(bufferDataSize), g_hudFont.texture, textureDataSize);

    auto uploadSlice = uploadBuffer->getSliceInfo();
    auto fontSlice = m_fontBuffer->getSliceInfo();

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

    ctx->cmdPipelineBarrier(DxvkCmdBuffer::InitBuffer, &depInfo);

    VkBufferCopy2 bufferRegion = { VK_STRUCTURE_TYPE_BUFFER_COPY_2 };
    bufferRegion.srcOffset = uploadSlice.offset;
    bufferRegion.dstOffset = fontSlice.offset;
    bufferRegion.size = bufferDataSize;

    VkCopyBufferInfo2 bufferCopy = { VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2 };
    bufferCopy.srcBuffer = uploadSlice.buffer;
    bufferCopy.dstBuffer = fontSlice.buffer;
    bufferCopy.regionCount = 1;
    bufferCopy.pRegions = &bufferRegion;

    ctx->cmdCopyBuffer(DxvkCmdBuffer::InitBuffer, &bufferCopy);

    VkBufferImageCopy2 imageRegion = { VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2 };
    imageRegion.bufferOffset = uploadSlice.offset + bufferDataSize;
    imageRegion.imageExtent = m_fontTexture->info().extent;
    imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageRegion.imageSubresource.layerCount = 1u;

    VkCopyBufferToImageInfo2 imageCopy = { VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2 };
    imageCopy.srcBuffer = uploadSlice.buffer;
    imageCopy.dstImage = m_fontTexture->handle();
    imageCopy.dstImageLayout = m_fontTexture->pickLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    imageCopy.regionCount = 1;
    imageCopy.pRegions = &imageRegion;

    ctx->cmdCopyBufferToImage(DxvkCmdBuffer::InitBuffer, &imageCopy);

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

    ctx->cmdPipelineBarrier(DxvkCmdBuffer::InitBuffer, &depInfo);

    m_fontTexture->trackLayout(m_fontTexture->getAvailableSubresources(), m_fontTexture->info().layout);

    ctx->track(uploadBuffer, DxvkAccess::Read);
    ctx->track(m_fontBuffer, DxvkAccess::Write);
    ctx->track(m_fontTexture, DxvkAccess::Write);
  }


  const DxvkPipelineLayout* HudRenderer::createPipelineLayout() {
    static const std::array<DxvkDescriptorSetLayoutBinding, 4> bindings = {{
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          1, VK_SHADER_STAGE_VERTEX_BIT   },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          1, VK_SHADER_STAGE_VERTEX_BIT   },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,    1, VK_SHADER_STAGE_VERTEX_BIT   },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,           1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    return m_device->createBuiltInPipelineLayout(DxvkPipelineLayoutFlag::UsesSamplerHeap,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      sizeof(HudPushConstants), bindings.size(), bindings.data());
  }


  VkPipeline HudRenderer::createPipeline(
    const HudPipelineKey&     key) {
    auto vk = m_device->vkd();

    HudSpecConstants specConstants = getSpecConstants(key);
    VkSpecializationInfo specInfo = getSpecInfo(&specConstants);

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

    util::DxvkBuiltInGraphicsState state;
    state.vs = util::DxvkBuiltInShaderStage(hud_text_vert, nullptr);
    state.fs = util::DxvkBuiltInShaderStage(hud_text_frag, &specInfo);
    state.colorFormat = key.format;
    state.cbAttachment = &cbAttachment;

    return m_device->createBuiltInGraphicsPipeline(m_textPipelineLayout, state);
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
