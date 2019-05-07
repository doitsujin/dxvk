#include "dxvk_hud_renderer.h"

#include <hud_line_frag.h>
#include <hud_line_vert.h>

#include <hud_text_frag.h>
#include <hud_text_vert.h>

namespace dxvk::hud {
  
  HudRenderer::HudRenderer(const Rc<DxvkDevice>& device)
  : m_mode          (Mode::RenderNone),
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
  
  
  void HudRenderer::beginFrame(const Rc<DxvkContext>& context, VkExtent2D surfaceSize) {
    auto vertexSlice = m_vertexBuffer->allocSlice();
    context->invalidateBuffer(m_vertexBuffer, vertexSlice);
    
    context->bindResourceSampler(1, m_fontSampler);
    context->bindResourceView   (1, m_fontView, nullptr);
    
    m_mode        = Mode::RenderNone;
    m_surfaceSize = surfaceSize;
  }
  
  
  void HudRenderer::drawText(
    const Rc<DxvkContext>&  context,
          float             size,
          HudPos            pos,
          HudColor          color,
    const std::string&      text) {
    beginTextRendering(context);

    uint32_t vertexCount = 6 * text.size();

    auto vertexSlice = allocVertexBuffer(context,
      vertexCount * sizeof(HudTextVertex));
    
    context->bindVertexBuffer(0, vertexSlice, sizeof(HudTextVertex));
    context->pushConstants(0, sizeof(color), &color);
    context->draw(vertexCount, 1, 0, 0);

    auto vertexData = reinterpret_cast<HudTextVertex*>(
      vertexSlice.getSliceHandle().mapPtr);
    
    const float sizeFactor = size / static_cast<float>(g_hudFont.size);
    
    for (size_t i = 0; i < text.size(); i++) {
      const HudGlyph& glyph = g_hudFont.glyphs[
        m_charMap[static_cast<uint8_t>(text[i])]];
      
      const HudPos size  = {
        sizeFactor * static_cast<float>(glyph.w),
        sizeFactor * static_cast<float>(glyph.h) };
      
      const HudPos origin = {
        pos.x + sizeFactor * static_cast<float>(glyph.originX),
        pos.y - sizeFactor * static_cast<float>(glyph.originY) };
      
      const HudPos posTl = { origin.x,          origin.y          };
      const HudPos posBr = { origin.x + size.x, origin.y + size.y };
      
      const HudTexCoord texTl = {
        static_cast<uint32_t>(glyph.x),
        static_cast<uint32_t>(glyph.y), };
        
      const HudTexCoord texBr = {
        static_cast<uint32_t>(glyph.x + glyph.w),
        static_cast<uint32_t>(glyph.y + glyph.h) };
      
      vertexData[6 * i + 0].position = { posTl.x, posTl.y };
      vertexData[6 * i + 0].texcoord = { texTl.u, texTl.v };
      
      vertexData[6 * i + 1].position = { posBr.x, posTl.y };
      vertexData[6 * i + 1].texcoord = { texBr.u, texTl.v };
      
      vertexData[6 * i + 2].position = { posTl.x, posBr.y };
      vertexData[6 * i + 2].texcoord = { texTl.u, texBr.v };
      
      vertexData[6 * i + 3].position = { posBr.x, posBr.y };
      vertexData[6 * i + 3].texcoord = { texBr.u, texBr.v };
      
      vertexData[6 * i + 4].position = { posTl.x, posBr.y };
      vertexData[6 * i + 4].texcoord = { texTl.u, texBr.v };
      
      vertexData[6 * i + 5].position = { posBr.x, posTl.y };
      vertexData[6 * i + 5].texcoord = { texBr.u, texTl.v };
      
      pos.x += sizeFactor * static_cast<float>(g_hudFont.advance);
    }
  }
  
  
  void HudRenderer::drawLines(
    const Rc<DxvkContext>&  context,
          size_t            vertexCount,
    const HudLineVertex*    vertexData) {
    beginLineRendering(context);

    auto vertexSlice = allocVertexBuffer(context,
      vertexCount * sizeof(HudLineVertex));
    
    context->bindVertexBuffer(0, vertexSlice, sizeof(HudLineVertex));
    context->draw(vertexCount, 1, 0, 0);
    
    auto dstVertexData = reinterpret_cast<HudLineVertex*>(
      vertexSlice.getSliceHandle().mapPtr);
    
    for (size_t i = 0; i < vertexCount; i++)
      dstVertexData[i] = vertexData[i];
  }
  
  
  DxvkBufferSlice HudRenderer::allocVertexBuffer(
    const Rc<DxvkContext>&  context,
          VkDeviceSize      dataSize) {
    dataSize = align(dataSize, 64);

    if (m_vertexOffset + dataSize > m_vertexBuffer->info().size) {
      context->invalidateBuffer(m_vertexBuffer, m_vertexBuffer->allocSlice());
      m_vertexOffset = 0;
    }

    DxvkBufferSlice slice(m_vertexBuffer, m_vertexOffset, dataSize);
    m_vertexOffset += dataSize;
    return slice;
  }
  

  void HudRenderer::beginTextRendering(
    const Rc<DxvkContext>&  context) {
    if (m_mode != Mode::RenderText) {
      m_mode = Mode::RenderText;

      context->bindShader(VK_SHADER_STAGE_VERTEX_BIT,   m_textShaders.vert);
      context->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_textShaders.frag);
      
      static const DxvkInputAssemblyState iaState = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE, 0 };

      static const std::array<DxvkVertexAttribute, 2> ilAttributes = {{
        { 0, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(HudTextVertex, position) },
        { 1, 0, VK_FORMAT_R32G32_UINT,         offsetof(HudTextVertex, texcoord) },
      }};
      
      static const std::array<DxvkVertexBinding, 1> ilBindings = {{
        { 0, VK_VERTEX_INPUT_RATE_VERTEX },
      }};
      
      context->setInputAssemblyState(iaState);
      context->setInputLayout(
        ilAttributes.size(),
        ilAttributes.data(),
        ilBindings.size(),
        ilBindings.data());
    }
  }

  
  void HudRenderer::beginLineRendering(
    const Rc<DxvkContext>&  context) {
    if (m_mode != Mode::RenderLines) {
      m_mode = Mode::RenderLines;

      context->bindShader(VK_SHADER_STAGE_VERTEX_BIT,   m_lineShaders.vert);
      context->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_lineShaders.frag);
      
      static const DxvkInputAssemblyState iaState = {
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        VK_FALSE, 0 };

      static const std::array<DxvkVertexAttribute, 2> ilAttributes = {{
        { 0, 0, VK_FORMAT_R32G32_SFLOAT,  offsetof(HudLineVertex, position) },
        { 1, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(HudLineVertex, color)    },
      }};
      
      static const std::array<DxvkVertexBinding, 1> ilBindings = {{
        { 0, VK_VERTEX_INPUT_RATE_VERTEX },
      }};
      
      context->setInputAssemblyState(iaState);
      context->setInputLayout(
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
    
    // One shader resource: Global HUD uniform buffer
    const std::array<DxvkResourceSlot, 1> vsResources = {{
      { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_IMAGE_VIEW_TYPE_MAX_ENUM },
    }};

    // Two shader resources: Font texture and sampler
    const std::array<DxvkResourceSlot, 1> fsResources = {{
      { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_IMAGE_VIEW_TYPE_2D },
    }};
    
    result.vert = device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      vsResources.size(),
      vsResources.data(),
      { 0x3, 0x1 },
      vsCode);
    
    result.frag = device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      fsResources.size(),
      fsResources.data(),
      { 0x1, 0x1, 0, sizeof(HudColor) },
      fsCode);
    
    return result;
  }
  
  
  HudRenderer::ShaderPair HudRenderer::createLineShaders(const Rc<DxvkDevice>& device) {
    ShaderPair result;

    const SpirvCodeBuffer vsCode(hud_line_vert);
    const SpirvCodeBuffer fsCode(hud_line_frag);
    
    // One shader resource: Global HUD uniform buffer
    const std::array<DxvkResourceSlot, 1> vsResources = {{
      { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_IMAGE_VIEW_TYPE_MAX_ENUM },
    }};

    result.vert = device->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      vsResources.size(),
      vsResources.data(),
      { 0x3, 0x1 },
      vsCode);
    
    result.frag = device->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      0, nullptr, { 0x1, 0x1 },
      fsCode);
    
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
    info.size           = 1 << 16;
    info.usage          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.stages         = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    info.access         = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    
    return device->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }
  
  
  void HudRenderer::initFontTexture(
    const Rc<DxvkDevice>&  device) {
    Rc<DxvkContext> context = device->createContext();
    
    context->beginRecording(
      device->createCommandList());
    
    context->initImage(m_fontImage,
      VkImageSubresourceRange {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 1, 0, 1 });
    
    context->updateImage(m_fontImage,
      VkImageSubresourceLayers {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 0, 1 },
      VkOffset3D { 0, 0, 0 },
      VkExtent3D { g_hudFont.width, g_hudFont.height, 1 },
      g_hudFont.texture,
      g_hudFont.width,
      g_hudFont.width * g_hudFont.height);
    
    device->submitCommandList(
      context->endRecording(),
      VK_NULL_HANDLE,
      VK_NULL_HANDLE);
  }
  
  
  void HudRenderer::initCharMap() {
    std::fill(m_charMap.begin(), m_charMap.end(), 0);
    
    for (uint32_t i = 0; i < g_hudFont.charCount; i++)
      m_charMap.at(g_hudFont.glyphs[i].codePoint) = i;
  }
  
}