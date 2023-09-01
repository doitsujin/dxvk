#include "dxvk_hud_renderer.h"

#include <hud_graph_frag.h>
#include <hud_graph_vert.h>

#include <hud_text_frag.h>
#include <hud_text_vert.h>

namespace dxvk::hud {
  
  HudRenderer::HudRenderer(const Rc<DxvkDevice>& device)
  : m_mode          (Mode::RenderNone),
    m_scale         (1.0f),
    m_surfaceSize   { 0, 0 },
    m_device        (device),
    m_textShaders   (createTextShaders()),
    m_graphShaders  (createGraphShaders()),
    m_dataBuffer    (createDataBuffer()),
    m_dataView      (createDataView()),
    m_dataOffset    (0ull),
    m_fontBuffer    (createFontBuffer()),
    m_fontBufferView(createFontBufferView()),
    m_fontImage     (createFontImage()),
    m_fontView      (createFontView()),
    m_fontSampler   (createFontSampler()) {

  }
  
  
  HudRenderer::~HudRenderer() {
    
  }
  
  
  void HudRenderer::beginFrame(
          const Rc<DxvkContext>& context, 
          VkExtent2D surfaceSize, 
          float scale, 
          float opacity) {
    if (!m_initialized)
      this->initFontTexture(context);

    m_mode        = Mode::RenderNone;
    m_scale       = scale;
    m_opacity     = opacity;
    m_surfaceSize = surfaceSize;
    m_context     = context;
  }
  
  
  void HudRenderer::drawText(
          float             size,
          HudPos            pos,
          HudColor          color,
    const std::string&      text) {
    if (text.empty())
      return;

    beginTextRendering();

    // Copy string into string buffer, but extend it to cover a full cache
    // line to avoid potential CPU performance issues with the upload.
    std::string textCopy = text;
    textCopy.resize(align(text.size(), CACHE_LINE_SIZE), ' ');

    VkDeviceSize offset = allocDataBuffer(textCopy.size());
    std::memcpy(m_dataBuffer->mapPtr(offset), textCopy.data(), textCopy.size());

    // Enforce HUD opacity factor on alpha
    if (m_opacity != 1.0f)
      color.a *= m_opacity;

    // Fill in push constants for the next draw
    HudTextPushConstants pushData;
    pushData.color = color;
    pushData.pos = pos;
    pushData.offset = offset;
    pushData.size = size;
    pushData.scale.x = m_scale / std::max(float(m_surfaceSize.width),  1.0f);
    pushData.scale.y = m_scale / std::max(float(m_surfaceSize.height), 1.0f);

    m_context->pushConstants(0, sizeof(pushData), &pushData);

    // Draw with orignal vertex count
    m_context->draw(6 * text.size(), 1, 0, 0);
  }
  
  
  void HudRenderer::drawGraph(
          HudPos            pos,
          HudPos            size,
          size_t            pointCount,
    const HudGraphPoint*    pointData) {
    beginGraphRendering();

    VkDeviceSize dataSize = pointCount * sizeof(*pointData);
    VkDeviceSize offset = allocDataBuffer(dataSize);
    std::memcpy(m_dataBuffer->mapPtr(offset), pointData, dataSize);

    HudGraphPushConstants pushData;
    pushData.offset = offset / sizeof(*pointData);
    pushData.count = pointCount;
    pushData.pos = pos;
    pushData.size = size;
    pushData.scale.x = m_scale / std::max(float(m_surfaceSize.width),  1.0f);
    pushData.scale.y = m_scale / std::max(float(m_surfaceSize.height), 1.0f);
    pushData.opacity = m_opacity;

    m_context->pushConstants(0, sizeof(pushData), &pushData);
    m_context->draw(4, 1, 0, 0);
  }
  
  
  void HudRenderer::beginTextRendering() {
    if (m_mode != Mode::RenderText) {
      m_mode = Mode::RenderText;

      m_context->bindShader<VK_SHADER_STAGE_VERTEX_BIT>(Rc<DxvkShader>(m_textShaders.vert));
      m_context->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(Rc<DxvkShader>(m_textShaders.frag));
      
      m_context->bindResourceBufferView(VK_SHADER_STAGE_VERTEX_BIT, 0, Rc<DxvkBufferView>(m_fontBufferView));
      m_context->bindResourceBufferView(VK_SHADER_STAGE_VERTEX_BIT, 1, Rc<DxvkBufferView>(m_dataView));
      m_context->bindResourceSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 2, Rc<DxvkSampler>(m_fontSampler));
      m_context->bindResourceImageView(VK_SHADER_STAGE_FRAGMENT_BIT, 2, Rc<DxvkImageView>(m_fontView));
      
      static const DxvkInputAssemblyState iaState = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE, 0 };
      
      m_context->setInputAssemblyState(iaState);
      m_context->setInputLayout(0, nullptr, 0, nullptr);
    }
  }

  
  void HudRenderer::beginGraphRendering() {
    if (m_mode != Mode::RenderGraph) {
      m_mode = Mode::RenderGraph;

      m_context->bindShader<VK_SHADER_STAGE_VERTEX_BIT>(Rc<DxvkShader>(m_graphShaders.vert));
      m_context->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(Rc<DxvkShader>(m_graphShaders.frag));
      
      m_context->bindResourceBufferView(VK_SHADER_STAGE_FRAGMENT_BIT, 0, Rc<DxvkBufferView>(m_dataView));

      static const DxvkInputAssemblyState iaState = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        VK_FALSE, 0 };

      m_context->setInputAssemblyState(iaState);
      m_context->setInputLayout(0, nullptr, 0, nullptr);
    }
  }


  VkDeviceSize HudRenderer::allocDataBuffer(VkDeviceSize size) {
    if (m_dataOffset + size > m_dataBuffer->info().size) {
      m_context->invalidateBuffer(m_dataBuffer, m_dataBuffer->allocSlice());
      m_dataOffset = 0;
    }
    
    VkDeviceSize offset = m_dataOffset;
    m_dataOffset = align(offset + size, 64);
    return offset;
  }
  

  HudRenderer::ShaderPair HudRenderer::createTextShaders() {
    ShaderPair result;

    SpirvCodeBuffer vsCode(hud_text_vert);
    SpirvCodeBuffer fsCode(hud_text_frag);
    
    const std::array<DxvkBindingInfo, 2> vsBindings = {{
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       0, VK_IMAGE_VIEW_TYPE_MAX_ENUM, VK_SHADER_STAGE_VERTEX_BIT, VK_ACCESS_SHADER_READ_BIT },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_IMAGE_VIEW_TYPE_MAX_ENUM, VK_SHADER_STAGE_VERTEX_BIT, VK_ACCESS_SHADER_READ_BIT },
    }};

    const std::array<DxvkBindingInfo, 1> fsBindings = {{
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, VK_IMAGE_VIEW_TYPE_MAX_ENUM, VK_SHADER_STAGE_FRAGMENT_BIT, VK_ACCESS_SHADER_READ_BIT },
    }};

    DxvkShaderCreateInfo vsInfo;
    vsInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vsInfo.bindingCount = vsBindings.size();
    vsInfo.bindings = vsBindings.data();
    vsInfo.outputMask = 0x3;
    vsInfo.pushConstSize = sizeof(HudTextPushConstants);
    result.vert = new DxvkShader(vsInfo, std::move(vsCode));

    DxvkShaderCreateInfo fsInfo;
    fsInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fsInfo.bindingCount = fsBindings.size();
    fsInfo.bindings = fsBindings.data();
    fsInfo.inputMask = 0x3;
    fsInfo.outputMask = 0x1;
    result.frag = new DxvkShader(fsInfo, std::move(fsCode));

    return result;
  }
  
  
  HudRenderer::ShaderPair HudRenderer::createGraphShaders() {
    ShaderPair result;

    SpirvCodeBuffer vsCode(hud_graph_vert);
    SpirvCodeBuffer fsCode(hud_graph_frag);
    
    const std::array<DxvkBindingInfo, 1> fsBindings = {{
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, VK_IMAGE_VIEW_TYPE_MAX_ENUM, VK_SHADER_STAGE_FRAGMENT_BIT, VK_ACCESS_SHADER_READ_BIT },
    }};

    DxvkShaderCreateInfo vsInfo;
    vsInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vsInfo.outputMask = 0x1;
    vsInfo.pushConstSize = sizeof(HudGraphPushConstants);
    result.vert = new DxvkShader(vsInfo, std::move(vsCode));
    
    DxvkShaderCreateInfo fsInfo;
    fsInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fsInfo.bindingCount = fsBindings.size();
    fsInfo.bindings = fsBindings.data();
    fsInfo.inputMask = 0x1;
    fsInfo.outputMask = 0x1;
    fsInfo.pushConstSize = sizeof(HudGraphPushConstants);
    result.frag = new DxvkShader(fsInfo, std::move(fsCode));
    
    return result;
  }
  
  
  Rc<DxvkBuffer> HudRenderer::createDataBuffer() {
    DxvkBufferCreateInfo info;
    info.size           = DataBufferSize;
    info.usage          = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                        | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    info.stages         = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                        | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    info.access         = VK_ACCESS_SHADER_READ_BIT;
    
    return m_device->createBuffer(info,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }


  Rc<DxvkBufferView> HudRenderer::createDataView() {
    DxvkBufferViewCreateInfo info;
    info.format = VK_FORMAT_R8_UINT;
    info.rangeOffset = 0;
    info.rangeLength = m_dataBuffer->info().size;

    return m_device->createBufferView(m_dataBuffer, info);
  }


  Rc<DxvkBuffer> HudRenderer::createFontBuffer() {
    DxvkBufferCreateInfo info;
    info.size           = sizeof(HudFontGpuData);
    info.usage          = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                        | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.stages         = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                        | VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access         = VK_ACCESS_SHADER_READ_BIT
                        | VK_ACCESS_TRANSFER_WRITE_BIT;
    
    return m_device->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }


  Rc<DxvkBufferView> HudRenderer::createFontBufferView() {
    DxvkBufferViewCreateInfo info;
    info.format         = VK_FORMAT_UNDEFINED;
    info.rangeOffset    = 0;
    info.rangeLength    = m_fontBuffer->info().size;

    return m_device->createBufferView(m_fontBuffer, info);
  }


  Rc<DxvkImage> HudRenderer::createFontImage() {
    DxvkImageCreateInfo info;
    info.type           = VK_IMAGE_TYPE_2D;
    info.format         = VK_FORMAT_R8_UNORM;
    info.flags          = 0;
    info.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
    info.extent         = { g_hudFont.width, g_hudFont.height, 1 };
    info.numLayers      = 1;
    info.mipLevels      = 1;
    info.usage          = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                        | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.stages         = VK_PIPELINE_STAGE_TRANSFER_BIT
                        | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    info.access         = VK_ACCESS_TRANSFER_WRITE_BIT
                        | VK_ACCESS_SHADER_READ_BIT;
    info.tiling         = VK_IMAGE_TILING_OPTIMAL;
    info.layout         = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    return m_device->createImage(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }
  
  
  Rc<DxvkImageView> HudRenderer::createFontView() {
    DxvkImageViewCreateInfo info;
    info.type           = VK_IMAGE_VIEW_TYPE_2D;
    info.format         = m_fontImage->info().format;
    info.usage          = VK_IMAGE_USAGE_SAMPLED_BIT;
    info.aspect         = VK_IMAGE_ASPECT_COLOR_BIT;
    info.minLevel       = 0;
    info.numLevels      = 1;
    info.minLayer       = 0;
    info.numLayers      = 1;
    
    return m_device->createImageView(m_fontImage, info);
  }
  
  
  Rc<DxvkSampler> HudRenderer::createFontSampler() {
    DxvkSamplerCreateInfo info;
    info.magFilter      = VK_FILTER_LINEAR;
    info.minFilter      = VK_FILTER_LINEAR;
    info.mipmapMode     = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.mipmapLodBias  = 0.0f;
    info.mipmapLodMin   = 0.0f;
    info.mipmapLodMax   = 0.0f;
    info.useAnisotropy  = VK_FALSE;
    info.maxAnisotropy  = 1.0f;
    info.addressModeU   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.compareToDepth = VK_FALSE;
    info.compareOp      = VK_COMPARE_OP_NEVER;
    info.reductionMode  = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
    info.borderColor    = VkClearColorValue();
    info.usePixelCoord  = VK_TRUE;
    info.nonSeamless    = VK_FALSE;
    
    return m_device->createSampler(info);
  }
  
  
  void HudRenderer::initFontTexture(
    const Rc<DxvkContext>& context) {
    HudFontGpuData gpuData = { };
    gpuData.size    = float(g_hudFont.size);
    gpuData.advance = float(g_hudFont.advance);

    for (uint32_t i = 0; i < g_hudFont.charCount; i++) {
      auto src = &g_hudFont.glyphs[i];
      auto dst = &gpuData.glyphs[src->codePoint];

      dst->x = src->x;
      dst->y = src->y;
      dst->w = src->w;
      dst->h = src->h;
      dst->originX = src->originX;
      dst->originY = src->originY;
    }

    context->uploadBuffer(m_fontBuffer, &gpuData);

    context->uploadImage(m_fontImage,
      VkImageSubresourceLayers { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
      g_hudFont.texture, g_hudFont.width, g_hudFont.width * g_hudFont.height);
    
    m_initialized = true;
  }
  
}
