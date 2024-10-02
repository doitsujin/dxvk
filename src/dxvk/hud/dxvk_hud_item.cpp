#include "dxvk_hud_item.h"

#include <hud_chunk_frag_background.h>
#include <hud_chunk_frag_visualize.h>
#include <hud_chunk_vert.h>

#include <iomanip>
#include <version.h>

namespace dxvk::hud {

  HudItem::~HudItem() {

  }


  void HudItem::update(dxvk::high_resolution_clock::time_point time) {
    // Do nothing by default. Some items won't need this.
  }


  HudItemSet::HudItemSet(const Rc<DxvkDevice>& device) {
    std::string configStr = env::getEnvVar("DXVK_HUD");

    if (configStr.empty())
      configStr = device->config().hud;

    std::string::size_type pos = 0;
    std::string::size_type end = 0;
    std::string::size_type mid = 0;
    
    while (pos < configStr.size()) {
      end = configStr.find(',', pos);
      mid = configStr.find('=', pos);
      
      if (end == std::string::npos)
        end = configStr.size();
      
      if (mid != std::string::npos && mid < end) {
        m_options.insert({
          configStr.substr(pos,     mid - pos),
          configStr.substr(mid + 1, end - mid - 1) });
      } else {
        m_enabled.insert(configStr.substr(pos, end - pos));
      }

      pos = end + 1;
    }

    if (m_enabled.find("full") != m_enabled.end())
      m_enableFull = true;
    
    if (m_enabled.find("1") != m_enabled.end()) {
      m_enabled.insert("devinfo");
      m_enabled.insert("fps");
    }
  }


  HudItemSet::~HudItemSet() {

  }


  void HudItemSet::update() {
    auto time = dxvk::high_resolution_clock::now();

    for (const auto& item : m_items)
      item->update(time);
  }


  void HudItemSet::render(HudRenderer& renderer) {
    HudPos position = { 8.0f, 8.0f };

    for (const auto& item : m_items)
      position = item->render(renderer, position);
  }


  void HudItemSet::parseOption(const std::string& str, float& value) {
    try {
      value = std::stof(str);
    } catch (const std::invalid_argument&) {
      return;
    }
  }


  HudPos HudVersionItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      "DXVK " DXVK_VERSION);

    position.y += 8.0f;
    return position;
  }


  HudClientApiItem::HudClientApiItem(std::string api)
  : m_api(api) {

  }


  HudClientApiItem::~HudClientApiItem() {

  }


  HudPos HudClientApiItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_api);

    position.y += 8.0f;
    return position;
  }


  HudDeviceInfoItem::HudDeviceInfoItem(const Rc<DxvkDevice>& device) {
    const auto& props = device->properties();

    std::string driverInfo = props.vk12.driverInfo;

    if (driverInfo.empty())
      driverInfo = props.driverVersion.toString();

    m_deviceName = props.core.properties.deviceName;
    m_driverName = str::format("Driver:  ", props.vk12.driverName);
    m_driverVer = str::format("Version: ", driverInfo);
  }


  HudDeviceInfoItem::~HudDeviceInfoItem() {

  }


  HudPos HudDeviceInfoItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_deviceName);
    
    position.y += 24.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_driverName);
    
    position.y += 20.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_driverVer);

    position.y += 8.0f;
    return position;
  }


  HudFpsItem::HudFpsItem() { }
  HudFpsItem::~HudFpsItem() { }


  void HudFpsItem::update(dxvk::high_resolution_clock::time_point time) {
    m_frameCount += 1;

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time - m_lastUpdate);

    if (elapsed.count() >= UpdateInterval) {
      int64_t fps = (10'000'000ll * m_frameCount) / elapsed.count();

      m_frameRate = str::format(fps / 10, ".", fps % 10);
      m_frameCount = 0;
      m_lastUpdate = time;
    }
  }


  HudPos HudFpsItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 0.25f, 0.25f, 1.0f },
      "FPS:");

    renderer.drawText(16.0f,
      { position.x + 60.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_frameRate);

    position.y += 8.0f;
    return position;
  }


  HudFrameTimeItem::HudFrameTimeItem() { }
  HudFrameTimeItem::~HudFrameTimeItem() { }


  void HudFrameTimeItem::update(dxvk::high_resolution_clock::time_point time) {
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time - m_lastUpdate);

    m_dataPoints[m_dataPointId] = float(elapsed.count());
    m_dataPointId = (m_dataPointId + 1) % NumDataPoints;

    m_lastUpdate = time;
  }


  HudPos HudFrameTimeItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    std::array<HudGraphPoint, NumDataPoints> points;

    // 60 FPS = optimal, 10 FPS = worst
    const float targetUs =  16'666.6f;
    const float minUs    =   5'000.0f;
    const float maxUs    = 100'000.0f;
    
    // Ten times the maximum/minimum number
    // of milliseconds for a single frame
    uint32_t minMs = 0xFFFFFFFFu;
    uint32_t maxMs = 0x00000000u;
    
    // Paint the time points
    for (uint32_t i = 0; i < NumDataPoints; i++) {
      float us = m_dataPoints[(m_dataPointId + i) % NumDataPoints];
      
      minMs = std::min(minMs, uint32_t(us / 100.0f));
      maxMs = std::max(maxMs, uint32_t(us / 100.0f));
      
      float r = std::min(std::max(-1.0f + us / targetUs, 0.0f), 1.0f);
      float g = std::min(std::max( 3.0f - us / targetUs, 0.0f), 1.0f);
      float l = std::sqrt(r * r + g * g);
      
      HudNormColor color = {
        uint8_t(255.0f * (r / l)),
        uint8_t(255.0f * (g / l)),
        uint8_t(0), uint8_t(255) };
      
      float hVal = std::log2(std::max((us - minUs) / targetUs + 1.0f, 1.0f))
                 / std::log2((maxUs - minUs) / targetUs);
      
      points[i].value = std::max(hVal, 1.0f / 40.0f);
      points[i].color = color;
    }
    
    renderer.drawGraph(position,
      HudPos { float(NumDataPoints), 40.0f },
      points.size(), points.data());
    
    position.y += 58.0f;

    renderer.drawText(12.0f,
      { position.x, position.y },
      { 1.0f, 0.25f, 0.25f, 1.0f },
      "min:");

    renderer.drawText(12.0f,
      { position.x + 45.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format(minMs / 10, ".", minMs % 10));
    
    renderer.drawText(12.0f,
      { position.x + 150.0f, position.y },
      { 1.0f, 0.25f, 0.25f, 1.0f },
      "max:");
    
    renderer.drawText(12.0f,
      { position.x + 195.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format(maxMs / 10, ".", maxMs % 10));
    
    position.y += 4.0f;
    return position;
  }


  HudSubmissionStatsItem::HudSubmissionStatsItem(const Rc<DxvkDevice>& device)
  : m_device(device) {

  }


  HudSubmissionStatsItem::~HudSubmissionStatsItem() {

  }


  void HudSubmissionStatsItem::update(dxvk::high_resolution_clock::time_point time) {
    DxvkStatCounters counters = m_device->getStatCounters();
    
    uint64_t currSubmitCount = counters.getCtr(DxvkStatCounter::QueueSubmitCount);
    uint64_t currSyncCount = counters.getCtr(DxvkStatCounter::GpuSyncCount);
    uint64_t currSyncTicks = counters.getCtr(DxvkStatCounter::GpuSyncTicks);

    m_maxSubmitCount = std::max(m_maxSubmitCount, currSubmitCount - m_prevSubmitCount);
    m_maxSyncCount = std::max(m_maxSyncCount, currSyncCount - m_prevSyncCount);
    m_maxSyncTicks = std::max(m_maxSyncTicks, currSyncTicks - m_prevSyncTicks);

    m_prevSubmitCount = currSubmitCount;
    m_prevSyncCount = currSyncCount;
    m_prevSyncTicks = currSyncTicks;

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time - m_lastUpdate);

    if (elapsed.count() >= UpdateInterval) {
      m_submitString = str::format(m_maxSubmitCount);

      uint64_t syncTicks = m_maxSyncTicks / 100;

      m_syncString = m_maxSyncCount
        ? str::format(m_maxSyncCount, " (", (syncTicks / 10), ".", (syncTicks % 10), " ms)")
        : str::format(m_maxSyncCount);

      m_maxSubmitCount = 0;
      m_maxSyncCount = 0;
      m_maxSyncTicks = 0;

      m_lastUpdate = time;
    }
  }


  HudPos HudSubmissionStatsItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 0.5f, 0.25f, 1.0f },
      "Queue submissions:");

    renderer.drawText(16.0f,
      { position.x + 228.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_submitString);

    position.y += 20.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 0.5f, 0.25f, 1.0f },
      "Queue syncs:");

    renderer.drawText(16.0f,
      { position.x + 228.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_syncString);

    position.y += 8.0f;
    return position;
  }


  HudDrawCallStatsItem::HudDrawCallStatsItem(const Rc<DxvkDevice>& device)
  : m_device(device) {

  }


  HudDrawCallStatsItem::~HudDrawCallStatsItem() {

  }


  void HudDrawCallStatsItem::update(dxvk::high_resolution_clock::time_point time) {
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(time - m_lastUpdate);

    DxvkStatCounters counters = m_device->getStatCounters();
    auto diffCounters = counters.diff(m_prevCounters);

    if (elapsed.count() >= UpdateInterval) {
      m_gpCount = diffCounters.getCtr(DxvkStatCounter::CmdDrawCalls);
      m_cpCount = diffCounters.getCtr(DxvkStatCounter::CmdDispatchCalls);
      m_rpCount = diffCounters.getCtr(DxvkStatCounter::CmdRenderPassCount);
      m_pbCount = diffCounters.getCtr(DxvkStatCounter::CmdBarrierCount);

      m_lastUpdate = time;
    }

    m_prevCounters = counters;
  }


  HudPos HudDrawCallStatsItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.25f, 0.5f, 1.0f, 1.0f },
      "Draw calls:");
    
    renderer.drawText(16.0f,
      { position.x + 192.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format(m_gpCount));
    
    position.y += 20.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.25f, 0.5f, 1.0f, 1.0f },
      "Dispatch calls:");
    
    renderer.drawText(16.0f,
      { position.x + 192.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format(m_cpCount));
    
    position.y += 20.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.25f, 0.5f, 1.0f, 1.0f },
      "Render passes:");
    
    renderer.drawText(16.0f,
      { position.x + 192.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format(m_rpCount));
    
    position.y += 20.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.25f, 0.5f, 1.0f, 1.0f },
      "Barriers:");
    
    renderer.drawText(16.0f,
      { position.x + 192.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format(m_pbCount));
    
    position.y += 8.0f;
    return position;
  }


  HudPipelineStatsItem::HudPipelineStatsItem(const Rc<DxvkDevice>& device)
  : m_device(device) {

  }


  HudPipelineStatsItem::~HudPipelineStatsItem() {

  }


  void HudPipelineStatsItem::update(dxvk::high_resolution_clock::time_point time) {
    DxvkStatCounters counters = m_device->getStatCounters();

    m_graphicsPipelines = counters.getCtr(DxvkStatCounter::PipeCountGraphics);
    m_graphicsLibraries = counters.getCtr(DxvkStatCounter::PipeCountLibrary);
    m_computePipelines  = counters.getCtr(DxvkStatCounter::PipeCountCompute);
  }


  HudPos HudPipelineStatsItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 0.25f, 1.0f, 1.0f },
      "Graphics pipelines:");
    
    renderer.drawText(16.0f,
      { position.x + 240.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format(m_graphicsPipelines));

    if (m_graphicsLibraries) {
      position.y += 20.0f;
      renderer.drawText(16.0f,
        { position.x, position.y },
        { 1.0f, 0.25f, 1.0f, 1.0f },
        "Graphics shaders:");

      renderer.drawText(16.0f,
        { position.x + 240.0f, position.y },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        str::format(m_graphicsLibraries));
    }

    position.y += 20.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 0.25f, 1.0f, 1.0f },
      "Compute pipelines:");

    renderer.drawText(16.0f,
      { position.x + 240.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format(m_computePipelines));

    position.y += 8.0f;
    return position;
  }


  HudDescriptorStatsItem::HudDescriptorStatsItem(const Rc<DxvkDevice>& device)
  : m_device(device) {

  }


  HudDescriptorStatsItem::~HudDescriptorStatsItem() {

  }


  void HudDescriptorStatsItem::update(dxvk::high_resolution_clock::time_point time) {
    DxvkStatCounters counters = m_device->getStatCounters();

    m_descriptorPoolCount = counters.getCtr(DxvkStatCounter::DescriptorPoolCount);
    m_descriptorSetCount  = counters.getCtr(DxvkStatCounter::DescriptorSetCount);
  }


  HudPos HudDescriptorStatsItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 0.25f, 0.5f, 1.0f },
      "Descriptor pools:");
    
    renderer.drawText(16.0f,
      { position.x + 216.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format(m_descriptorPoolCount));
    
    position.y += 20.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 1.0f, 0.25f, 0.5f, 1.0f },
      "Descriptor sets:");

    renderer.drawText(16.0f,
      { position.x + 216.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      str::format(m_descriptorSetCount));

    position.y += 8.0f;
    return position;
  }


  HudMemoryStatsItem::HudMemoryStatsItem(const Rc<DxvkDevice>& device)
  : m_device(device), m_memory(device->adapter()->memoryProperties()) {

  }


  HudMemoryStatsItem::~HudMemoryStatsItem() {

  }


  void HudMemoryStatsItem::update(dxvk::high_resolution_clock::time_point time) {
    for (uint32_t i = 0; i < m_memory.memoryHeapCount; i++)
      m_heaps[i] = m_device->getMemoryStats(i);
  }


  HudPos HudMemoryStatsItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    for (uint32_t i = 0; i < m_memory.memoryHeapCount; i++) {
      bool isDeviceLocal = m_memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;

      uint64_t memUsedMib = m_heaps[i].memoryUsed >> 20;
      uint64_t memAllocatedMib = m_heaps[i].memoryAllocated >> 20;
      uint64_t percentage = m_heaps[i].memoryBudget
        ? (100u * m_heaps[i].memoryAllocated) / m_heaps[i].memoryBudget
        : 0u;

      std::string label = str::format(isDeviceLocal ? "Vidmem" : "Sysmem", " heap ", i, ": ");
      std::string text  = str::format(std::setfill(' '), std::setw(5), memAllocatedMib, " MB (", percentage, "%) ",
        std::setw(5 + (percentage < 10 ? 1 : 0) + (percentage < 100 ? 1 : 0)), memUsedMib, " MB used");

      position.y += 16.0f;
      renderer.drawText(16.0f,
        { position.x, position.y },
        { 1.0f, 1.0f, 0.25f, 1.0f },
        label);

      renderer.drawText(16.0f,
        { position.x + 168.0f, position.y },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        text);
      position.y += 4.0f;
    }

    position.y += 4.0f;
    return position;
  }




  HudMemoryDetailsItem::HudMemoryDetailsItem(const Rc<DxvkDevice>& device)
  : m_device(device) {
    DxvkShaderCreateInfo shaderInfo;
    shaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderInfo.pushConstStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderInfo.pushConstSize = sizeof(ShaderArgs);
    shaderInfo.outputMask = 0x1;

    m_vs = new DxvkShader(shaderInfo, SpirvCodeBuffer(hud_chunk_vert));

    shaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderInfo.outputMask = 0x1;

    m_fsBackground = new DxvkShader(shaderInfo, SpirvCodeBuffer(hud_chunk_frag_background));

    DxvkBindingInfo pageMaskBinding = {
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, VK_IMAGE_VIEW_TYPE_MAX_ENUM,
      VK_SHADER_STAGE_FRAGMENT_BIT, VK_ACCESS_SHADER_READ_BIT };

    shaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderInfo.bindingCount = 1;
    shaderInfo.bindings = &pageMaskBinding;
    shaderInfo.inputMask = 0x1;
    shaderInfo.outputMask = 0x1;

    m_fsVisualize = new DxvkShader(shaderInfo, SpirvCodeBuffer(hud_chunk_frag_visualize));
  }


  HudMemoryDetailsItem::~HudMemoryDetailsItem() {

  }


  void HudMemoryDetailsItem::update(dxvk::high_resolution_clock::time_point time) {
    uint64_t ticks = std::chrono::duration_cast<std::chrono::microseconds>(time - m_lastUpdate).count();

    if (ticks >= UpdateInterval) {
      m_cacheStats = m_device->getMemoryAllocationStats(m_stats);
      m_displayCacheStats |= m_cacheStats.requestCount != 0u;

      m_lastUpdate = time;
    }
  }


  HudPos HudMemoryDetailsItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    uploadChunkData(renderer);

    // Chunk memory per type, not including dedicated allocations
    std::array<VkDeviceSize, VK_MAX_MEMORY_TYPES> chunkMemoryAllocated = { };
    std::array<VkDeviceSize, VK_MAX_MEMORY_TYPES> chunkMemoryUsed = { };

    // Compute layout, align the entire element to the bottom right.
    float maxWidth = 556.0f;

    HudPos pos = {
      float(renderer.surfaceSize().width) / renderer.scale() - 8.0f - maxWidth,
      float(renderer.surfaceSize().height) / renderer.scale() - 8.0f,
    };

    for (uint32_t i = 0; i < m_stats.memoryTypes.size(); i++) {
      const auto& type = m_stats.memoryTypes.at(i);

      if (!type.allocated)
        continue;

      // Reserve space for one line of text
      pos.y -= 20.0f;

      float width = 0.0f;

      for (uint32_t j = 0; j < type.chunkCount; j++) {
        const auto& chunk = m_stats.chunks.at(type.chunkIndex + j);
        chunkMemoryAllocated.at(i) += chunk.capacity;
        chunkMemoryUsed.at(i) += chunk.used;

        float pixels = float((chunk.pageCount + 15u) / 16u);

        if (width + pixels > maxWidth) {
          pos.y -= 30.0f;
          width = 0.0f;
        }

        width += pixels + 6.0f;
      }

      pos.y -= 30.0f + 4.0f;
    }

    if (m_displayCacheStats)
      pos.y -= 20.0f;

    // Actually render the thing
    for (uint32_t i = 0; i < m_stats.memoryTypes.size(); i++) {
      const auto& type = m_stats.memoryTypes.at(i);

      if (!type.allocated)
        continue;

      VkDeviceSize dedicated = type.allocated - chunkMemoryAllocated.at(i);
      VkDeviceSize allocated = chunkMemoryAllocated.at(i) + dedicated;
      VkDeviceSize used = chunkMemoryUsed.at(i) + dedicated;

      std::string headline = str::format("Mem type ", i, " [", type.properties.heapIndex, "]: ",
        type.chunkCount, " chunk", type.chunkCount != 1u ? "s" : "", " (", (allocated >> 20u), " MB, ",
        ((used >= (1u << 20u)) ? used >> 20 : used >> 10),
        (used >= (1u << 20u) ? " MB" : " kB"), " used)");

      renderer.drawText(14.0f,
        { pos.x, pos.y },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        headline);

      pos.y += 8.0f;

      float width = 0.0f;

      for (uint32_t j = 0; j < type.chunkCount; j++) {
        const auto& chunk = m_stats.chunks.at(type.chunkIndex + j);
        float pixels = float((chunk.pageCount + 15u) / 16u);

        if (width + pixels > maxWidth) {
          pos.y += 30.0f;
          width = 0.0f;
        }

        drawChunk(renderer,
          { pos.x + width, pos.y },
          { pixels, 24.0f },
          type.properties, chunk);

        width += pixels + 6.0f;
      }

      pos.y += 46.0f;
    }

    if (m_displayCacheStats) {
      uint32_t hitCount = m_cacheStats.requestCount - m_cacheStats.missCount;
      uint32_t hitRate = (100 * hitCount) / std::max(m_cacheStats.requestCount, 1u);

      std::string cacheStr = str::format("Cache: ", m_cacheStats.size >> 10, " kB (", hitRate, "% hit)");

      renderer.drawText(14.0f,
        { pos.x, pos.y },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        cacheStr);
    }

    return position;
  }


  void HudMemoryDetailsItem::uploadChunkData(HudRenderer& renderer) {
    DxvkContext* context = renderer.getContext();

    VkDeviceSize size = sizeof(uint32_t) * m_stats.pageMasks.size();

    if (m_pageMaskBuffer == nullptr || m_pageMaskBuffer->info().size < size) {
      DxvkBufferCreateInfo info = { };
      info.size = std::max(VkDeviceSize(1u << 14),
        (VkDeviceSize(-1) >> bit::lzcnt(size - 1u)) + 1u);
      info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      info.access = VK_ACCESS_SHADER_READ_BIT;
      info.stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

      m_pageMaskBuffer = m_device->createBuffer(info,
        VK_MEMORY_HEAP_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      DxvkBufferViewKey viewInfo = { };
      viewInfo.format = VK_FORMAT_UNDEFINED;
      viewInfo.offset = 0;
      viewInfo.size = info.size;
      viewInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

      m_pageMaskView = m_pageMaskBuffer->createView(viewInfo);
    }

    if (!m_stats.pageMasks.empty()) {
      context->invalidateBuffer(m_pageMaskBuffer, m_pageMaskBuffer->allocateSlice());
      std::memcpy(m_pageMaskBuffer->mapPtr(0), &m_stats.pageMasks.at(0), size);
    }
  }


  void HudMemoryDetailsItem::drawChunk(
          HudRenderer&      renderer,
          HudPos            pos,
          HudPos            size,
    const VkMemoryType&     memoryType,
    const DxvkMemoryChunkStats& stats) const {
    DxvkContext* context = renderer.getContext();
    VkExtent2D surfaceSize = renderer.surfaceSize();

    static const DxvkInputAssemblyState iaState = {
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      VK_FALSE, 0 };

    context->setInputAssemblyState(iaState);
    context->bindResourceBufferView(VK_SHADER_STAGE_FRAGMENT_BIT, 0, Rc<DxvkBufferView>(m_pageMaskView));

    context->bindShader<VK_SHADER_STAGE_VERTEX_BIT>(Rc<DxvkShader>(m_vs));
    context->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(Rc<DxvkShader>(m_fsBackground));

    ShaderArgs args = { };
    args.pos.x = pos.x - 1.0f;
    args.pos.y = pos.y - 1.0f;
    args.size.x = size.x + 2.0f;
    args.size.y = size.y + 2.0f;
    args.scale.x = renderer.scale() / std::max(float(surfaceSize.width), 1.0f);
    args.scale.y = renderer.scale() / std::max(float(surfaceSize.height), 1.0f);
    args.opacity = renderer.opacity();
    args.color = 0xc0000000u;
    args.maskIndex = stats.pageMaskOffset;
    args.pageCount = stats.pageCount;

    context->pushConstants(0, sizeof(args), &args);
    context->draw(4, 1, 0, 0);

    context->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(Rc<DxvkShader>(m_fsVisualize));

    args.pos = pos;
    args.size = size;

    if (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
        args.color = 0xff208020u;
    } else if (!(memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
      if (!stats.mapped)
        args.color = 0xff202020u;
      else
        args.color = 0xff202080u;
    } else if (stats.mapped) {
      args.color = 0xff208080u;
    } else {
      args.color = 0xff804020u;
    }

    context->pushConstants(0, sizeof(args), &args);
    context->draw(4, 1, 0, 0);
  }




  HudCsThreadItem::HudCsThreadItem(const Rc<DxvkDevice>& device)
  : m_device(device) {

  }


  HudCsThreadItem::~HudCsThreadItem() {

  }


  void HudCsThreadItem::update(dxvk::high_resolution_clock::time_point time) {
    uint64_t ticks = std::chrono::duration_cast<std::chrono::microseconds>(time - m_lastUpdate).count();

    // Capture the maximum here since it's more useful to
    // identify stutters than using any sort of average
    DxvkStatCounters counters = m_device->getStatCounters();
    uint64_t currCsSyncCount = counters.getCtr(DxvkStatCounter::CsSyncCount);
    uint64_t currCsSyncTicks = counters.getCtr(DxvkStatCounter::CsSyncTicks);

    m_maxCsSyncCount = std::max(m_maxCsSyncCount, currCsSyncCount - m_prevCsSyncCount);
    m_maxCsSyncTicks = std::max(m_maxCsSyncTicks, currCsSyncTicks - m_prevCsSyncTicks);

    m_prevCsSyncCount = currCsSyncCount;
    m_prevCsSyncTicks = currCsSyncTicks;

    m_updateCount++;

    if (ticks >= UpdateInterval) {
      uint64_t currCsChunks = counters.getCtr(DxvkStatCounter::CsChunkCount);
      uint64_t diffCsChunks = (currCsChunks - m_prevCsChunks) / m_updateCount;
      m_prevCsChunks = currCsChunks;

      uint64_t syncTicks = m_maxCsSyncTicks / 100;

      m_csChunkString = str::format(diffCsChunks);
      m_csSyncString = m_maxCsSyncCount
        ? str::format(m_maxCsSyncCount, " (", (syncTicks / 10), ".", (syncTicks % 10), " ms)")
        : str::format(m_maxCsSyncCount);

      m_maxCsSyncCount = 0;
      m_maxCsSyncTicks = 0;

      m_updateCount = 0;
      m_lastUpdate = time;
    }
  }


  HudPos HudCsThreadItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.25f, 1.0f, 0.25f, 1.0f },
      "CS chunks:");

    renderer.drawText(16.0f,
      { position.x + 132.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_csChunkString);

    position.y += 20.0f;
    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.25f, 1.0f, 0.25f, 1.0f },
      "CS syncs:");

    renderer.drawText(16.0f,
      { position.x + 132.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_csSyncString);

    position.y += 8.0f;
    return position;
  }


  HudGpuLoadItem::HudGpuLoadItem(const Rc<DxvkDevice>& device)
  : m_device(device) {

  }


  HudGpuLoadItem::~HudGpuLoadItem() {

  }


  void HudGpuLoadItem::update(dxvk::high_resolution_clock::time_point time) {
    uint64_t ticks = std::chrono::duration_cast<std::chrono::microseconds>(time - m_lastUpdate).count();

    if (ticks >= UpdateInterval) {
      DxvkStatCounters counters = m_device->getStatCounters();
      uint64_t currGpuIdleTicks = counters.getCtr(DxvkStatCounter::GpuIdleTicks);

      m_diffGpuIdleTicks = currGpuIdleTicks - m_prevGpuIdleTicks;
      m_prevGpuIdleTicks = currGpuIdleTicks;

      uint64_t busyTicks = ticks > m_diffGpuIdleTicks
        ? uint64_t(ticks - m_diffGpuIdleTicks)
        : uint64_t(0);

      m_gpuLoadString = str::format((100 * busyTicks) / ticks, "%");
      m_lastUpdate = time;
    }
  }


  HudPos HudGpuLoadItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    position.y += 16.0f;

    renderer.drawText(16.0f,
      { position.x, position.y },
      { 0.25f, 0.5f, 0.25f, 1.0f },
      "GPU:");

    renderer.drawText(16.0f,
      { position.x + 60.0f, position.y },
      { 1.0f, 1.0f, 1.0f, 1.0f },
      m_gpuLoadString);

    position.y += 8.0f;
    return position;
  }


  HudCompilerActivityItem::HudCompilerActivityItem(const Rc<DxvkDevice>& device)
  : m_device(device) {

  }


  HudCompilerActivityItem::~HudCompilerActivityItem() {

  }


  void HudCompilerActivityItem::update(dxvk::high_resolution_clock::time_point time) {
    DxvkStatCounters counters = m_device->getStatCounters();

    m_tasksDone = counters.getCtr(DxvkStatCounter::PipeTasksDone);
    m_tasksTotal = counters.getCtr(DxvkStatCounter::PipeTasksTotal);

    bool doShow = m_tasksDone < m_tasksTotal;

    if (!doShow)
      m_timeDone = time;

    if (!m_show) {
      m_timeShown = time;
      m_showPercentage = false;
    } else {
      auto durationShown = std::chrono::duration_cast<std::chrono::milliseconds>(time - m_timeShown);
      auto durationWorking = std::chrono::duration_cast<std::chrono::milliseconds>(time - m_timeDone);

      if (!doShow) {
        m_offset = m_tasksTotal;

        // Ensure the item stays up long enough to be legible
        doShow = durationShown.count() <= MinShowDuration;
      }

      if (!m_showPercentage) {
        // Don't show percentage if it's just going to be stuck at 99%
        // because the workers are not being fed tasks fast enough
        m_showPercentage = durationWorking.count() >= (MinShowDuration / 5)
                        && (computePercentage() < 50);
      }
    }

    m_show = doShow;
  }


  HudPos HudCompilerActivityItem::render(
          HudRenderer&      renderer,
          HudPos            position) {
    if (m_show) {
      std::string string = "Compiling shaders...";

      if (m_showPercentage)
        string = str::format(string, " (", computePercentage(), "%)");

      renderer.drawText(16.0f,
        { position.x, float(renderer.surfaceSize().height) / renderer.scale() - 20.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        string);
    }

    return position;
  }


  uint32_t HudCompilerActivityItem::computePercentage() const {
    if (m_offset == m_tasksTotal)
      return 100;

    return (uint32_t(m_tasksDone - m_offset) * 100)
         / (uint32_t(m_tasksTotal - m_offset));
  }

}
