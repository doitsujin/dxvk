#include "dxvk_hud_renderer.h"

#include <hud_line_frag.h>
#include <hud_line_vert.h>

#include <hud_text_frag.h>
#include <hud_text_vert.h>

namespace dxvk::hud {
  
  HudRenderer::HudRenderer(const Rc<DxvkDevice>& device)
  : m_mode          (Mode::RenderNone),
    m_scale         (1.0f),
    m_surfaceSize   { 0, 0 },
    m_device        (device),
    m_textShaders   (createTextShaders()),
    m_lineShaders   (createLineShaders()),
    m_dataBuffer    (createDataBuffer()),
    m_dataView      (createDataView()),
    m_dataOffset    (0ull),
    m_fontBuffer    (createFontBuffer()),
    m_fontImage     (createFontImage()),
    m_fontView      (createFontView()),
    m_fontSampler   (createFontSampler()),
    m_vertexBuffer  (createVertexBuffer()) {

  }
  
  
  HudRenderer::~HudRenderer() {
    
  }
  
  
  void HudRenderer::beginFrame(const Rc<DxvkContext>& context, VkExtent2D surfaceSize, float scale) {
    if (!m_initialized)
      this->initFontTexture(context);

    m_mode        = Mode::RenderNone;
    m_scale       = scale;
    m_surfaceSize = surfaceSize;
    m_context     = context;

    allocVertexBufferSlice();
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
  
  
  void HudRenderer::drawLines(
          size_t            vertexCount,
    const HudLineVertex*    vertexData) {
    beginLineRendering();

    const float xscale = m_scale / std::max(float(m_surfaceSize.width),  1.0f);
    const float yscale = m_scale / std::max(float(m_surfaceSize.height), 1.0f);

    if (m_currLineVertex + vertexCount > MaxLineVertexCount)
      allocVertexBufferSlice();

    m_context->draw(vertexCount, 1, m_currLineVertex, 0);
    
    for (size_t i = 0; i < vertexCount; i++) {
      uint32_t idx = m_currLineVertex + i;

      m_vertexData->lineVertices[idx].position = {
        xscale * vertexData[i].position.x,
        yscale * vertexData[i].position.y };
      m_vertexData->lineVertices[idx].color = vertexData[i].color;
    }

    m_currLineVertex += vertexCount;
  }
  
  
  void HudRenderer::allocVertexBufferSlice() {
    auto vertexSlice = m_vertexBuffer->allocSlice();
    m_context->invalidateBuffer(m_vertexBuffer, vertexSlice);
    
    m_currLineVertex    = 0;

    m_vertexData = reinterpret_cast<VertexBufferData*>(vertexSlice.mapPtr);
  }


  void HudRenderer::beginTextRendering() {
    if (m_mode != Mode::RenderText) {
      m_mode = Mode::RenderText;

      m_context->bindShader(VK_SHADER_STAGE_VERTEX_BIT,   m_textShaders.vert);
      m_context->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_textShaders.frag);
      
      m_context->bindResourceBuffer (0, DxvkBufferSlice(m_fontBuffer));
      m_context->bindResourceView   (1, nullptr, m_dataView);
      m_context->bindResourceSampler(2, m_fontSampler);
      m_context->bindResourceView   (2, m_fontView, nullptr);
      
      static const DxvkInputAssemblyState iaState = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE, 0 };
      
      m_context->setInputAssemblyState(iaState);
      m_context->setInputLayout(0, nullptr, 0, nullptr);
    }
  }

  
  void HudRenderer::beginLineRendering() {
    if (m_mode != Mode::RenderLines) {
      m_mode = Mode::RenderLines;

      m_context->bindVertexBuffer(0, DxvkBufferSlice(m_vertexBuffer, offsetof(VertexBufferData, lineVertices), sizeof(HudLineVertex) * MaxLineVertexCount), sizeof(HudLineVertex));

      m_context->bindShader(VK_SHADER_STAGE_VERTEX_BIT,   m_lineShaders.vert);
      m_context->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_lineShaders.frag);
      
      static const DxvkInputAssemblyState iaState = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        VK_FALSE, 0 };

      static const std::array<DxvkVertexAttribute, 2> ilAttributes = {{
        { 0, 0, VK_FORMAT_R32G32_SFLOAT,  offsetof(HudLineVertex, position) },
        { 1, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(HudLineVertex, color)    },
      }};
      
      static const std::array<DxvkVertexBinding, 1> ilBindings = {{
        { 0, 0, VK_VERTEX_INPUT_RATE_VERTEX },
      }};
      
      m_context->setInputAssemblyState(iaState);
      m_context->setInputLayout(
        ilAttributes.size(),
        ilAttributes.data(),
        ilBindings.size(),
        ilBindings.data());
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

    const SpirvCodeBuffer vsCode(hud_text_vert);
    const SpirvCodeBuffer fsCode(hud_text_frag);
    
    const std::array<DxvkResourceSlot, 3> vsResources = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       VK_IMAGE_VIEW_TYPE_MAX_ENUM },
      { 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_IMAGE_VIEW_TYPE_MAX_ENUM },
    }};
    
    const std::array<DxvkResourceSlot, 1> fsResources = {{
      { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D },
    }};
    
    result.vert = m_device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      vsResources.size(),
      vsResources.data(),
      { 0x0, 0x3, 0, sizeof(HudTextPushConstants) },
      vsCode);
    
    result.frag = m_device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      fsResources.size(),
      fsResources.data(),
      { 0x3, 0x1 },
      fsCode);
    
    return result;
  }
  
  
  HudRenderer::ShaderPair HudRenderer::createLineShaders() {
    ShaderPair result;

    const SpirvCodeBuffer vsCode(hud_line_vert);
    const SpirvCodeBuffer fsCode(hud_line_frag);
    
    result.vert = m_device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      0, nullptr, { 0x3, 0x1 }, vsCode);
    
    result.frag = m_device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      0, nullptr, { 0x1, 0x1 }, fsCode);
    
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
    info.borderColor    = VkClearColorValue();
    info.usePixelCoord  = VK_TRUE;
    
    return m_device->createSampler(info);
  }
  
  
  Rc<DxvkBuffer> HudRenderer::createVertexBuffer() {
    DxvkBufferCreateInfo info;
    info.size           = sizeof(VertexBufferData);
    info.usage          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.stages         = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    info.access         = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    
    return m_device->createBuffer(info,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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
