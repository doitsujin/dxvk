#include "d3d9_hud.h"

namespace dxvk::hud {

  HudSamplerCount::HudSamplerCount(D3D9DeviceEx* device)
    : m_device       (device)
    , m_samplerCount ("0"){

  }


  void HudSamplerCount::update(dxvk::high_resolution_clock::time_point time) {
    DxvkSamplerStats stats = m_device->GetDXVKDevice()->getSamplerStats();
    m_samplerCount = str::format(stats.totalCount);
  }


  HudPos HudSamplerCount::render(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xffc0ff00u, "Samplers:");
    renderer.drawText(16, { position.x + 120, position.y }, 0xffffffffu, m_samplerCount);

    position.y += 8;
    return position;
  }

  HudTextureMemory::HudTextureMemory(D3D9DeviceEx* device)
  : m_device          (device)
  , m_allocatedString ("")
  , m_mappedString    ("") { }


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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xffc0ff00u, "Mappable:");
    renderer.drawText(16, { position.x + 120, position.y }, 0xffffffffu, m_allocatedString);

    position.y += 20;
    renderer.drawText(16, position, 0xffc0ff00u, "Mapped:");
    renderer.drawText(16, { position.x + 120, position.y }, 0xffffffffu, m_mappedString);

    position.y += 8;
    return position;
  }

  HudFixedFunctionShaders::HudFixedFunctionShaders(D3D9DeviceEx* device)
  : m_device        (device)
  , m_ffShaderCount ("") {}


  void HudFixedFunctionShaders::update(dxvk::high_resolution_clock::time_point time) {
    m_ffShaderCount = str::format(
      "VS: ", m_device->GetFixedFunctionVSCount(),
      " FS: ", m_device->GetFixedFunctionFSCount(),
      " SWVP: ", m_device->GetSWVPShaderCount()
    );
  }


  HudPos HudFixedFunctionShaders::render(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xffc0ff00u, "FF Shaders:");
    renderer.drawText(16, { position.x + 155, position.y }, 0xffffffffu, m_ffShaderCount);

    position.y += 8;
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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xffc0ff00u, "Vertex Processing:");
    renderer.drawText(16, { position.x + 240, position.y }, 0xffffffffu, m_isSWVPText);

    position.y += 8;
    return position;
  }

}
