#include "dxvk_hud_text.h"

#include <hud_text_frag.h>
#include <hud_text_vert.h>

namespace dxvk::hud {
  
  HudTextRenderer::HudTextRenderer(
    const Rc<DxvkDevice>&   device,
    const Rc<DxvkContext>&  context)
  : m_vertShader    (createVertexShader(device)),
    m_fragShader    (createFragmentShader(device)),
    m_fontImage     (createFontImage(device)),
    m_fontView      (createFontView(device)),
    m_fontSampler   (createFontSampler(device)),
    m_vertexBuffer  (createVertexBuffer(device)) {
    this->initFontTexture(device, context);
    this->initCharMap();
  }
  
  
  HudTextRenderer::~HudTextRenderer() {
    
  }
  
  
  void HudTextRenderer::beginFrame(const Rc<DxvkContext>& context) {
    context->bindShader(VK_SHADER_STAGE_VERTEX_BIT,   m_vertShader);
    context->bindShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_fragShader);
    
    DxvkInputAssemblyState iaState;
    iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    iaState.primitiveRestart  = VK_FALSE;
    iaState.patchVertexCount  = 0;
    context->setInputAssemblyState(iaState);
    
    const std::array<DxvkVertexAttribute, 3> ilAttributes = {{
      { 0, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(HudTextVertex, position) },
      { 1, 0, VK_FORMAT_R32G32_UINT,         offsetof(HudTextVertex, texcoord) },
      { 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(HudTextVertex, color)    },
    }};
    
    const std::array<DxvkVertexBinding, 1> ilBindings = {{
      { 0, VK_VERTEX_INPUT_RATE_VERTEX },
    }};
    
    context->setInputLayout(
      ilAttributes.size(),
      ilAttributes.data(),
      ilBindings.size(),
      ilBindings.data());
    
    context->bindVertexBuffer(0,
      DxvkBufferSlice(m_vertexBuffer),
      sizeof(HudTextVertex));
    
    context->bindResourceSampler(1, m_fontSampler);
    context->bindResourceImage  (2, m_fontView);
    
    m_vertexIndex = 0;
  }
  
  
  void HudTextRenderer::drawText(
    const Rc<DxvkContext>&  context,
          float             size,
          HudPos            pos,
          HudColor          color,
    const std::string&      text) {
    const size_t vertexIndex = m_vertexIndex;
    
    auto vertexSlice = m_vertexBuffer->allocPhysicalSlice();
    context->invalidateBuffer(m_vertexBuffer, vertexSlice);
    
    HudTextVertex* vertexData = reinterpret_cast<HudTextVertex*>(
      vertexSlice.mapPtr(vertexIndex * sizeof(HudTextVertex)));
    
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
      vertexData[6 * i + 0].color    = color;
      
      vertexData[6 * i + 1].position = { posBr.x, posTl.y };
      vertexData[6 * i + 1].texcoord = { texBr.u, texTl.v };
      vertexData[6 * i + 1].color    = color;
      
      vertexData[6 * i + 2].position = { posTl.x, posBr.y };
      vertexData[6 * i + 2].texcoord = { texTl.u, texBr.v };
      vertexData[6 * i + 2].color    = color;
      
      vertexData[6 * i + 3].position = { posBr.x, posBr.y };
      vertexData[6 * i + 3].texcoord = { texBr.u, texBr.v };
      vertexData[6 * i + 3].color    = color;
      
      vertexData[6 * i + 4].position = { posTl.x, posBr.y };
      vertexData[6 * i + 4].texcoord = { texTl.u, texBr.v };
      vertexData[6 * i + 4].color    = color;
      
      vertexData[6 * i + 5].position = { posBr.x, posTl.y };
      vertexData[6 * i + 5].texcoord = { texBr.u, texTl.v };
      vertexData[6 * i + 5].color    = color;
      
      pos.x += sizeFactor * static_cast<float>(g_hudFont.advance);
    }
    
    const uint32_t vertexCount = 6 * text.size();
    context->draw(vertexCount, 1, vertexIndex, 0);
    m_vertexIndex += vertexCount;
  }
  
  
  Rc<DxvkShader> HudTextRenderer::createVertexShader(const Rc<DxvkDevice>& device) {
    const SpirvCodeBuffer codeBuffer(hud_text_vert);
    
    // One shader resource: Global HUD uniform buffer
    const std::array<DxvkResourceSlot, 1> resourceSlots = {{
      { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_IMAGE_VIEW_TYPE_MAX_ENUM },
    }};
    
    // 3 input registers, 2 output registers, tightly packed
    const DxvkInterfaceSlots interfaceSlots = { 0x7, 0x3 };
    
    return new DxvkShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      resourceSlots.size(),
      resourceSlots.data(),
      interfaceSlots,
      codeBuffer);
  }
  
  
  Rc<DxvkShader> HudTextRenderer::createFragmentShader(const Rc<DxvkDevice>& device) {
    const SpirvCodeBuffer codeBuffer(hud_text_frag);
    
    // One shader resource: Global HUD uniform buffer
    const std::array<DxvkResourceSlot, 2> resourceSlots = {{
      { 1, VK_DESCRIPTOR_TYPE_SAMPLER,       VK_IMAGE_VIEW_TYPE_MAX_ENUM },
      { 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_IMAGE_VIEW_TYPE_2D       },
    }};
    
    // 2 input registers, 1 output register
    const DxvkInterfaceSlots interfaceSlots = { 0x3, 0x1 };
    
    return new DxvkShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      resourceSlots.size(),
      resourceSlots.data(),
      interfaceSlots,
      codeBuffer);
  }
  
  
  Rc<DxvkImage> HudTextRenderer::createFontImage(const Rc<DxvkDevice>& device) {
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
  
  
  Rc<DxvkImageView> HudTextRenderer::createFontView(const Rc<DxvkDevice>& device) {
    DxvkImageViewCreateInfo info;
    info.type           = VK_IMAGE_VIEW_TYPE_2D;
    info.format         = m_fontImage->info().format;
    info.aspect         = VK_IMAGE_ASPECT_COLOR_BIT;
    info.minLevel       = 0;
    info.numLevels      = 1;
    info.minLayer       = 0;
    info.numLayers      = 1;
    
    return device->createImageView(m_fontImage, info);
  }
  
  
  Rc<DxvkSampler> HudTextRenderer::createFontSampler(const Rc<DxvkDevice>& device) {
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
    info.borderColor    = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    info.usePixelCoord  = VK_TRUE;
    
    return device->createSampler(info);
  }
  
  
  Rc<DxvkBuffer> HudTextRenderer::createVertexBuffer(const Rc<DxvkDevice>& device) {
    DxvkBufferCreateInfo info;
    info.size           = MaxVertexCount * sizeof(HudTextVertex);
    info.usage          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.stages         = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    info.access         = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    
    return device->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }
  
  
  void HudTextRenderer::initFontTexture(
    const Rc<DxvkDevice>&  device,
    const Rc<DxvkContext>& context) {
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
      nullptr, nullptr);
  }
  
  
  void HudTextRenderer::initCharMap() {
    std::fill(m_charMap.begin(), m_charMap.end(), 0);
    
    for (uint32_t i = 0; i < g_hudFont.charCount; i++)
      m_charMap.at(g_hudFont.glyphs[i].codePoint) = i;
  }
  
}