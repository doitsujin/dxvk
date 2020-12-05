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
    m_textShaders   (createTextShaders(device)),
    m_lineShaders   (createLineShaders(device)),
    m_fontImage     (createFontImage(device)),
    m_fontView      (createFontView(device)),
    m_fontSampler   (createFontSampler(device)),
    m_vertexBuffer  (createVertexBuffer(device)) {
    this->initFontTexture(device);
    this->initCharMap();
  }
  
  
  HudRenderer::~HudRenderer() {
    
  }
  
  
  void HudRenderer::beginFrame(const Rc<DxvkContext>& context, VkExtent2D surfaceSize, float scale) {
    context->bindResourceSampler(0, m_fontSampler);
    context->bindResourceView   (0, m_fontView, nullptr);
    
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
    beginTextRendering();

    const float xscale = m_scale / std::max(float(m_surfaceSize.width),  1.0f);
    const float yscale = m_scale / std::max(float(m_surfaceSize.height), 1.0f);

    uint32_t vertexCount = 6 * text.size();

    if (m_currTextVertex   + vertexCount > MaxTextVertexCount
     || m_currTextInstance + 1           > MaxTextInstanceCount)
      allocVertexBufferSlice();

    m_context->draw(vertexCount, 1, m_currTextVertex, m_currTextInstance);

    const float sizeFactor = size / float(g_hudFont.size);
    
    for (size_t i = 0; i < text.size(); i++) {
      const HudGlyph& glyph = g_hudFont.glyphs[
        m_charMap[uint8_t(text[i])]];
      
      HudPos size  = {
        sizeFactor * float(glyph.w),
        sizeFactor * float(glyph.h) };
      
      HudPos origin = {
        pos.x - sizeFactor * float(glyph.originX),
        pos.y - sizeFactor * float(glyph.originY) };
      
      HudPos posTl = { xscale * (origin.x),          yscale * (origin.y)          };
      HudPos posBr = { xscale * (origin.x + size.x), yscale * (origin.y + size.y) };
      
      HudTexCoord texTl = { uint32_t(glyph.x),           uint32_t(glyph.y)           };
      HudTexCoord texBr = { uint32_t(glyph.x + glyph.w), uint32_t(glyph.y + glyph.h) };

      uint32_t idx = 6 * i + m_currTextVertex;
      
      m_vertexData->textVertices[idx + 0].position = { posTl.x, posTl.y };
      m_vertexData->textVertices[idx + 0].texcoord = { texTl.u, texTl.v };
      
      m_vertexData->textVertices[idx + 1].position = { posBr.x, posTl.y };
      m_vertexData->textVertices[idx + 1].texcoord = { texBr.u, texTl.v };
      
      m_vertexData->textVertices[idx + 2].position = { posTl.x, posBr.y };
      m_vertexData->textVertices[idx + 2].texcoord = { texTl.u, texBr.v };
      
      m_vertexData->textVertices[idx + 3].position = { posBr.x, posBr.y };
      m_vertexData->textVertices[idx + 3].texcoord = { texBr.u, texBr.v };
      
      m_vertexData->textVertices[idx + 4].position = { posTl.x, posBr.y };
      m_vertexData->textVertices[idx + 4].texcoord = { texTl.u, texBr.v };
      
      m_vertexData->textVertices[idx + 5].position = { posBr.x, posTl.y };
      m_vertexData->textVertices[idx + 5].texcoord = { texBr.u, texTl.v };
      
      pos.x += sizeFactor * static_cast<float>(g_hudFont.advance);
    }

    m_vertexData->textColors[m_currTextInstance] = color;

    m_currTextVertex   += vertexCount;
    m_currTextInstance += 1;
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
    
    m_currTextVertex    = 0;
    m_currTextInstance  = 0;
    m_currLineVertex    = 0;

    m_vertexData = reinterpret_cast<VertexBufferData*>(vertexSlice.mapPtr);
  }


  void HudRenderer::beginTextRendering() {
    if (m_mode != Mode::RenderText) {
      m_mode = Mode::RenderText;

      m_context->bindVertexBuffer(0, DxvkBufferSlice(m_vertexBuffer, offsetof(VertexBufferData, textVertices), sizeof(HudTextVertex) * MaxTextVertexCount), sizeof(HudTextVertex));
      m_context->bindVertexBuffer(1, DxvkBufferSlice(m_vertexBuffer, offsetof(VertexBufferData, textColors), sizeof(HudColor) * MaxTextInstanceCount), sizeof(HudColor));

      m_context->bindShader(VK_SHADER_STAGE_VERTEX_BIT,   m_textShaders.vert);
      m_context->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_textShaders.frag);
      
      static const DxvkInputAssemblyState iaState = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE, 0 };

      static const std::array<DxvkVertexAttribute, 3> ilAttributes = {{
        { 0, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(HudTextVertex, position) },
        { 1, 0, VK_FORMAT_R32G32_UINT,         offsetof(HudTextVertex, texcoord) },
        { 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
      }};
      
      static const std::array<DxvkVertexBinding, 2> ilBindings = {{
        { 0, 0, VK_VERTEX_INPUT_RATE_VERTEX   },
        { 1, 1, VK_VERTEX_INPUT_RATE_INSTANCE },
      }};
      
      m_context->setInputAssemblyState(iaState);
      m_context->setInputLayout(
        ilAttributes.size(),
        ilAttributes.data(),
        ilBindings.size(),
        ilBindings.data());
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
  

  HudRenderer::ShaderPair HudRenderer::createTextShaders(const Rc<DxvkDevice>& device) {
    ShaderPair result;

    const SpirvCodeBuffer vsCode(hud_text_vert);
    const SpirvCodeBuffer fsCode(hud_text_frag);
    
    // Two shader resources: Font texture and sampler
    const std::array<DxvkResourceSlot, 1> fsResources = {{
      { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D },
    }};
    
    result.vert = device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      0, nullptr, { 0x7, 0x3 }, vsCode);
    
    result.frag = device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      fsResources.size(),
      fsResources.data(),
      { 0x3, 0x1 },
      fsCode);
    
    return result;
  }
  
  
  HudRenderer::ShaderPair HudRenderer::createLineShaders(const Rc<DxvkDevice>& device) {
    ShaderPair result;

    const SpirvCodeBuffer vsCode(hud_line_vert);
    const SpirvCodeBuffer fsCode(hud_line_frag);
    
    result.vert = device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      0, nullptr, { 0x3, 0x1 }, vsCode);
    
    result.frag = device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      0, nullptr, { 0x1, 0x1 }, fsCode);
    
    return result;
  }
  
  
  Rc<DxvkImage> HudRenderer::createFontImage(const Rc<DxvkDevice>& device) {
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
    
    return device->createImage(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }
  
  
  Rc<DxvkImageView> HudRenderer::createFontView(const Rc<DxvkDevice>& device) {
    DxvkImageViewCreateInfo info;
    info.type           = VK_IMAGE_VIEW_TYPE_2D;
    info.format         = m_fontImage->info().format;
    info.usage          = VK_IMAGE_USAGE_SAMPLED_BIT;
    info.aspect         = VK_IMAGE_ASPECT_COLOR_BIT;
    info.minLevel       = 0;
    info.numLevels      = 1;
    info.minLayer       = 0;
    info.numLayers      = 1;
    
    return device->createImageView(m_fontImage, info);
  }
  
  
  Rc<DxvkSampler> HudRenderer::createFontSampler(const Rc<DxvkDevice>& device) {
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
    
    return device->createSampler(info);
  }
  
  
  Rc<DxvkBuffer> HudRenderer::createVertexBuffer(const Rc<DxvkDevice>& device) {
    DxvkBufferCreateInfo info;
    info.size           = sizeof(VertexBufferData);
    info.usage          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.stages         = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    info.access         = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    
    return device->createBuffer(info,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }
  
  
  void HudRenderer::initFontTexture(
    const Rc<DxvkDevice>&  device) {
    Rc<DxvkContext> context = device->createContext();
    
    context->beginRecording(
      device->createCommandList());
    
    context->uploadImage(m_fontImage,
      VkImageSubresourceLayers {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 0, 1 },
      g_hudFont.texture,
      g_hudFont.width,
      g_hudFont.width * g_hudFont.height);
    
    device->submitCommandList(
      context->endRecording(),
      VK_NULL_HANDLE,
      VK_NULL_HANDLE);
    
    context->trimStagingBuffers();
  }
  
  
  void HudRenderer::initCharMap() {
    std::fill(m_charMap.begin(), m_charMap.end(), 0);
    
    for (uint32_t i = 0; i < g_hudFont.charCount; i++)
      m_charMap.at(g_hudFont.glyphs[i].codePoint) = i;
  }
  
}
