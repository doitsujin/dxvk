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

  HudTextureMemory::HudTextureMemory(D3D9DeviceEx* device)
          : m_device          (device)
          , m_allocatedString ("")
          , m_mappedString    ("") {}


  void HudTextureMemory::update(dxvk::high_resolution_clock::time_point time) {
    D3D9MemoryAllocator* allocator = m_device->GetAllocator();

    m_maxAllocated = std::max(m_maxAllocated, allocator->AllocatedMemory());
    m_maxUsed = std::max(m_maxUsed, allocator->UsedMemory());
    m_maxMapped = std::max(m_maxMapped, allocator->MappedMemory());

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time - m_lastUpdate);

    if (elapsed.count() < UpdateInterval)
      return;

    m_allocatedString = str::format(m_maxAllocated >> 20, " MB (Used: ", m_maxUsed >> 20, " MB)");
    m_mappedString = str::format(m_maxMapped >> 20, " MB");
    m_maxAllocated = 0;
    m_maxUsed = 0;
    m_maxMapped = 0;
    m_lastUpdate = time;
  }


  HudPos HudTextureMemory::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
                      { position.x, position.y },
                      { 0.0f, 1.0f, 0.75f, 1.0f },
                      "Mappable:");

    renderer.drawText(16.0f,
                      { position.x + 120.0f, position.y },
                      { 1.0f, 1.0f, 1.0f, 1.0f },
                      m_allocatedString);

    position.y += 24.0f;

    renderer.drawText(16.0f,
                      { position.x, position.y },
                      { 0.0f, 1.0f, 0.75f, 1.0f },
                      "Mapped:");

    renderer.drawText(16.0f,
                      { position.x + 120.0f, position.y },
                      { 1.0f, 1.0f, 1.0f, 1.0f },
                      m_mappedString);

    position.y += 8.0f;

    return position;
  }

  HudFixedFunctionShaders::HudFixedFunctionShaders(D3D9DeviceEx* device)
          : m_device          (device)
          , m_ffShaderCount ("") {}


  void HudFixedFunctionShaders::update(dxvk::high_resolution_clock::time_point time) {
    m_ffShaderCount = str::format(
      "VS: ", m_device->GetFixedFunctionVSCount(),
      " FS: ", m_device->GetFixedFunctionFSCount(),
      " SWVP: ", m_device->GetSWVPShaderCount()
    );
  }


  HudPos HudFixedFunctionShaders::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.0f, 1.0f, 0.75f, 1.0f },
      "FF Shaders:");

    renderer.drawText(16.0f,
      { position.x + 155.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_ffShaderCount);

    position.y += 8.0f;
    return position;
  }

  HudSWVPState::HudSWVPState(D3D9DeviceEx* device)
          : m_device          (device)
          , m_isSWVPText ("") {}


  void HudSWVPState::update(dxvk::high_resolution_clock::time_point time) {
    if (m_device->IsSWVP()) {
      if (m_device->CanOnlySWVP()) {
        m_isSWVPText = "SWVP";
      } else {
        m_isSWVPText = "SWVP (Mixed)";
      }
    } else {
      if (m_device->CanSWVP()) {
        m_isSWVPText = "HWVP (Mixed)";
      } else {
        m_isSWVPText = "HWVP";
      }
    }
  }


  HudPos HudSWVPState::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.0f, 1.0f, 0.75f, 1.0f },
      "Vertex Processing:");

    renderer.drawText(16.0f,
      { position.x + 240.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_isSWVPText);

    position.y += 8.0f;
    return position;
  }

}
