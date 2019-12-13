#include <cstring>

#include "dxvk_hud.h"

namespace dxvk::hud {
  
  Hud::Hud(
    const Rc<DxvkDevice>& device)
  : m_device        (device),
    m_uniformBuffer (createUniformBuffer()),
    m_renderer      (device) {
    // Set up constant state
    m_rsState.polygonMode       = VK_POLYGON_MODE_FILL;
    m_rsState.cullMode          = VK_CULL_MODE_BACK_BIT;
    m_rsState.frontFace         = VK_FRONT_FACE_CLOCKWISE;
    m_rsState.depthClipEnable   = VK_FALSE;
    m_rsState.depthBiasEnable   = VK_FALSE;
    m_rsState.sampleCount       = VK_SAMPLE_COUNT_1_BIT;

    m_blendMode.enableBlending  = VK_TRUE;
    m_blendMode.colorSrcFactor  = VK_BLEND_FACTOR_ONE;
    m_blendMode.colorDstFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_blendMode.colorBlendOp    = VK_BLEND_OP_ADD;
    m_blendMode.alphaSrcFactor  = VK_BLEND_FACTOR_ONE;
    m_blendMode.alphaDstFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_blendMode.alphaBlendOp    = VK_BLEND_OP_ADD;
    m_blendMode.writeMask       = VK_COLOR_COMPONENT_R_BIT
                                | VK_COLOR_COMPONENT_G_BIT
                                | VK_COLOR_COMPONENT_B_BIT
                                | VK_COLOR_COMPONENT_A_BIT;

    addItem<HudVersionItem>("version");
    addItem<HudClientApiItem>("api", m_device);
    addItem<HudDeviceInfoItem>("devinfo", m_device);
    addItem<HudFpsItem>("fps");
    addItem<HudFrameTimeItem>("frametimes");
    addItem<HudSubmissionStatsItem>("submissions", device);
    addItem<HudDrawCallStatsItem>("drawcalls", device);
    addItem<HudPipelineStatsItem>("pipelines", device);
    addItem<HudMemoryStatsItem>("memory", device);
    addItem<HudGpuLoadItem>("gpuload", device);
    addItem<HudCompilerActivityItem>("compiler", device);
  }
  
  
  Hud::~Hud() {
    
  }
  
  
  void Hud::update() {
    m_hudItems.update();
  }
  
  
  void Hud::render(
    const Rc<DxvkContext>&  ctx,
          VkSurfaceFormatKHR surfaceFormat,
          VkExtent2D        surfaceSize) {
    m_uniformData.surfaceSize = surfaceSize;
    
    this->updateUniformBuffer(ctx, m_uniformData);

    this->setupRendererState(ctx, surfaceFormat);
    this->renderHudElements(ctx);
    this->resetRendererState(ctx);
  }
  
  
  Rc<Hud> Hud::createHud(const Rc<DxvkDevice>& device) {
    return new Hud(device);
  }


  void Hud::setupRendererState(
    const Rc<DxvkContext>&  ctx,
          VkSurfaceFormatKHR surfaceFormat) {
    bool isSrgb = imageFormatInfo(surfaceFormat.format)->flags.test(DxvkFormatFlag::ColorSpaceSrgb);

    ctx->setRasterizerState(m_rsState);
    ctx->setBlendMode(0, m_blendMode);

    ctx->bindResourceBuffer(0,
      DxvkBufferSlice(m_uniformBuffer));

    ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, isSrgb);
    m_renderer.beginFrame(ctx, m_uniformData.surfaceSize);
  }


  void Hud::resetRendererState(const Rc<DxvkContext>& ctx) {
    ctx->setSpecConstant(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0);
  }


  void Hud::renderHudElements(const Rc<DxvkContext>& ctx) {
    m_hudItems.render(m_renderer);
  }
  
  
  void Hud::updateUniformBuffer(const Rc<DxvkContext>& ctx, const HudUniformData& data) {
    auto slice = m_uniformBuffer->allocSlice();
    std::memcpy(slice.mapPtr, &data, sizeof(data));

    ctx->invalidateBuffer(m_uniformBuffer, slice);
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
  
}
