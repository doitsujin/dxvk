#include <cstring>

#include "dxvk_hud.h"

namespace dxvk::hud {
  
  Hud::Hud(
    const Rc<DxvkDevice>& device)
  : m_device        (device),
    m_renderer      (device),
    m_hudItems      (device) {
    // Retrieve and sanitize options
    m_options.scale = std::clamp(m_hudItems.getOption<float>("scale", 1.0f), 0.25f, 4.0f);
    m_options.opacity = std::clamp(m_hudItems.getOption<float>("opacity", 1.0f), 0.1f, 1.0f);

    addItem<HudVersionItem>("version", -1);
    addItem<HudDeviceInfoItem>("devinfo", -1, m_device);
    addItem<HudFpsItem>("fps", -1);
    addItem<HudFrameTimeItem>("frametimes", -1, device, &m_renderer);
    addItem<HudSubmissionStatsItem>("submissions", -1, device);
    addItem<HudDrawCallStatsItem>("drawcalls", -1, device);
    addItem<HudPipelineStatsItem>("pipelines", -1, device);
    addItem<HudDescriptorStatsItem>("descriptors", -1, device);
    addItem<HudMemoryStatsItem>("memory", -1, device);
    addItem<HudMemoryDetailsItem>("allocations", -1, device, &m_renderer);
    addItem<HudCsThreadItem>("cs", -1, device);
    addItem<HudGpuLoadItem>("gpuload", -1, device);
    addItem<HudCompilerActivityItem>("compiler", -1, device);
  }


  Hud::~Hud() {
    
  }


  void Hud::update() {
    m_hudItems.update();
  }


  void Hud::render(
    const DxvkContextObjects& ctx,
    const Rc<DxvkImageView>&  dstView) {
    if (empty())
      return;

    auto key = m_renderer.getPipelineKey(dstView);

    m_renderer.beginFrame(ctx, dstView, m_options);
    m_hudItems.render(ctx, key, m_options, m_renderer);
    m_renderer.flushDraws(ctx, dstView, m_options);
    m_renderer.endFrame(ctx);
  }


  Rc<Hud> Hud::createHud(const Rc<DxvkDevice>& device) {
    return new Hud(device);
  }
  
}
