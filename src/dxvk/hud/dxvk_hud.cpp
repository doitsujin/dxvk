#include "dxvk_hud.h"

#include <cstring>

namespace dxvk::hud {
  
  Hud::Hud(const Rc<DxvkDevice>& device)
  : m_device        (device),
    m_context       (m_device->createContext()),
    m_textRenderer  (m_device, m_context),
    m_uniformBuffer (createUniformBuffer()),
    m_hudDeviceInfo (device) {
    this->setupConstantState();
  }
  
  
  Hud::~Hud() {
    
  }
  
  
  void Hud::render(VkExtent2D size) {
    bool recreateFbo = m_surfaceSize != size;
    
    if (recreateFbo) {
      m_surfaceSize = size;
      this->setupFramebuffer(size);
    }
      
    m_hudFps.update();
    
    this->beginRenderPass(recreateFbo);
    this->updateUniformBuffer();
    this->renderText();
    this->endRenderPass();
  }
  
  
  Rc<Hud> Hud::createHud(const Rc<DxvkDevice>& device) {
    const std::string hudConfig = env::getEnvVar(L"DXVK_HUD");
    
    if (hudConfig.size() == 0 || hudConfig == "0")
      return nullptr;
    
    // TODO implement configuration options for the HUD
    return new Hud(device);
  }
  
  
  Rc<DxvkBuffer> Hud::createUniformBuffer() {
    DxvkBufferCreateInfo info;
    info.size           = sizeof(HudUniformData);
    info.usage          = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    info.stages         = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                        | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    info.access         = VK_ACCESS_UNIFORM_READ_BIT;
    
    return m_device->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }
  
  
  void Hud::renderText() {
    m_textRenderer.beginFrame(m_context);
    
    HudPos position = { 8.0f, 24.0f };
    position = m_hudDeviceInfo.renderText(
      m_context, m_textRenderer, position);
    position = m_hudFps.renderText(
      m_context, m_textRenderer, position);
  }
  
  
  void Hud::updateUniformBuffer() {
    HudUniformData uniformData;
    uniformData.surfaceSize = m_surfaceSize;
    
    auto slice = m_uniformBuffer->allocPhysicalSlice();
    m_context->invalidateBuffer(m_uniformBuffer, slice);
    std::memcpy(slice.mapPtr(0), &uniformData, sizeof(uniformData));
  }
  
  
  void Hud::beginRenderPass(bool initFbo) {
    m_context->beginRecording(
      m_device->createCommandList());
    
    if (initFbo) {
      m_context->initImage(m_renderTarget,
        VkImageSubresourceRange {
          VK_IMAGE_ASPECT_COLOR_BIT,
          0, 1, 0, 1 });
    }
    
    VkClearAttachment clearInfo;
    clearInfo.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    clearInfo.colorAttachment = 0;
    
    for (uint32_t i = 0; i < 4; i++)
      clearInfo.clearValue.color.float32[i] = 0.0f;
    
    VkClearRect clearRect;
    clearRect.rect.offset = { 0, 0 };
    clearRect.rect.extent = m_surfaceSize;
    clearRect.baseArrayLayer = 0;
    clearRect.layerCount     = 1;
    
    m_context->bindFramebuffer(m_renderTargetFbo);
    m_context->clearRenderTarget(clearInfo, clearRect);
    
    VkViewport viewport;
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(m_surfaceSize.width);
    viewport.height   = static_cast<float>(m_surfaceSize.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = m_surfaceSize;
    
    m_context->setViewports(1, &viewport, &scissor);
    m_context->bindResourceBuffer(0, DxvkBufferSlice(m_uniformBuffer));
  }
  
  
  void Hud::endRenderPass() {
    m_device->submitCommandList(
      m_context->endRecording(),
      nullptr, nullptr);
  }
  
  
  void Hud::setupFramebuffer(VkExtent2D size) {
    DxvkImageCreateInfo imageInfo;
    imageInfo.type          = VK_IMAGE_TYPE_2D;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.flags         = 0;
    imageInfo.sampleCount   = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.extent        = { size.width, size.height, 1 };
    imageInfo.numLayers     = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.stages        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    imageInfo.access        = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                            | VK_ACCESS_SHADER_READ_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.layout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    m_renderTarget = m_device->createImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type           = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format         = imageInfo.format;
    viewInfo.aspect         = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel       = 0;
    viewInfo.numLevels      = 1;
    viewInfo.minLayer       = 0;
    viewInfo.numLayers      = 1;
    
    m_renderTargetView = m_device->createImageView(m_renderTarget, viewInfo);
    
    DxvkRenderTargets framebufferInfo;
    framebufferInfo.setColorTarget(0, m_renderTargetView,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    m_renderTargetFbo = m_device->createFramebuffer(framebufferInfo);
  }
  
  
  void Hud::setupConstantState() {
    DxvkRasterizerState rsState;
    rsState.enableDepthClamp  = VK_FALSE;
    rsState.enableDiscard     = VK_FALSE;
    rsState.polygonMode       = VK_POLYGON_MODE_FILL;
    rsState.cullMode          = VK_CULL_MODE_BACK_BIT;
    rsState.frontFace         = VK_FRONT_FACE_CLOCKWISE;
    rsState.depthBiasEnable   = VK_FALSE;
    rsState.depthBiasConstant = 0.0f;
    rsState.depthBiasClamp    = 0.0f;
    rsState.depthBiasSlope    = 0.0f;
    m_context->setRasterizerState(rsState);
    
    DxvkMultisampleState msState;
    msState.sampleMask            = 0xFFFFFFFF;
    msState.enableAlphaToCoverage = VK_FALSE;
    msState.enableAlphaToOne      = VK_FALSE;
    msState.enableSampleShading   = VK_FALSE;
    msState.minSampleShading      = 1.0f;
    m_context->setMultisampleState(msState);
    
    VkStencilOpState stencilOp;
    stencilOp.failOp          = VK_STENCIL_OP_KEEP;
    stencilOp.passOp          = VK_STENCIL_OP_KEEP;
    stencilOp.depthFailOp     = VK_STENCIL_OP_KEEP;
    stencilOp.compareOp       = VK_COMPARE_OP_NEVER;
    stencilOp.compareMask     = 0xFFFFFFFF;
    stencilOp.writeMask       = 0xFFFFFFFF;
    stencilOp.reference       = 0;
    
    DxvkDepthStencilState dsState;
    dsState.enableDepthTest   = VK_FALSE;
    dsState.enableDepthWrite  = VK_FALSE;
    dsState.enableDepthBounds = VK_FALSE;
    dsState.enableStencilTest = VK_FALSE;
    dsState.depthCompareOp    = VK_COMPARE_OP_NEVER;
    dsState.stencilOpFront    = stencilOp;
    dsState.stencilOpBack     = stencilOp;
    m_context->setDepthStencilState(dsState);
    
    DxvkLogicOpState loState;
    loState.enableLogicOp     = VK_FALSE;
    loState.logicOp           = VK_LOGIC_OP_NO_OP;
    m_context->setLogicOpState(loState);
    
    DxvkBlendMode blendMode;
    blendMode.enableBlending  = VK_TRUE;
    blendMode.colorSrcFactor  = VK_BLEND_FACTOR_ONE;
    blendMode.colorDstFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendMode.colorBlendOp    = VK_BLEND_OP_ADD;
    blendMode.alphaSrcFactor  = VK_BLEND_FACTOR_ONE;
    blendMode.alphaDstFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendMode.alphaBlendOp    = VK_BLEND_OP_ADD;
    blendMode.writeMask       = VK_COLOR_COMPONENT_R_BIT
                              | VK_COLOR_COMPONENT_G_BIT
                              | VK_COLOR_COMPONENT_B_BIT
                              | VK_COLOR_COMPONENT_A_BIT;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
      m_context->setBlendMode(i, blendMode);
  }
  
}