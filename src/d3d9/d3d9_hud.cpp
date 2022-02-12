#include "d3d9_hud.h"

namespace dxvk::hud {

  HudSamplerCount::HudSamplerCount(D3D9DeviceEx* device)
    : m_device       (device)
    , m_samplerCount ("0"){

  }


  void HudSamplerCount::update(dxvk::high_resolution_clock::time_point time) {
    m_samplerCount = str::format(m_device->GetSamplerCount());
  }


  HudPos HudSamplerCount::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.0f, 1.0f, 0.75f, 1.0f },
      "Samplers:");

    renderer.drawText(16.0f,
      { position.x + 120.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_samplerCount);

    position.y += 8.0f;
    return position;
  }



  HudWaitForResourceCount::HudWaitForResourceCount(D3D9DeviceEx* device)
    : m_device               (device)
    , m_waitForResourceCount ("0"){

  }


  void HudWaitForResourceCount::update(dxvk::high_resolution_clock::time_point time) {
    m_waitForResourceCount = str::format(m_device->GetWaitForResourceCount());
  }


  HudPos HudWaitForResourceCount::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.0f, 1.0f, 0.75f, 1.0f },
      "WaitForResource calls:");

    renderer.drawText(16.0f,
      { position.x + 270.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_waitForResourceCount);

    position.y += 8.0f;
    return position;
  }



  HudCsSyncCount::HudCsSyncCount(D3D9DeviceEx* device)
    : m_device       (device)
    , m_syncCount ("0"){

  }


  void HudCsSyncCount::update(dxvk::high_resolution_clock::time_point time) {
    m_syncCount = str::format(m_device->GetCsSyncCount());
  }


  HudPos HudCsSyncCount::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.0f, 1.0f, 0.75f, 1.0f },
      "CS Thread syncs:");

    renderer.drawText(16.0f,
      { position.x + 200.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_syncCount);

    position.y += 8.0f;
    return position;
  }

}