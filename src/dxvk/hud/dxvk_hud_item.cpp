#include "dxvk_hud_item.h"

#include <hud_chunk_frag_background.h>
#include <hud_chunk_frag_visualize.h>
#include <hud_chunk_vert_background.h>
#include <hud_chunk_vert_visualize.h>

#include <hud_frame_time_eval.h>

#include <hud_graph_frag.h>
#include <hud_graph_vert.h>

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


  void HudItemSet::render(
      const DxvkContextObjects& ctx,
      const HudPipelineKey&     key,
      const HudOptions&         options,
            HudRenderer&        renderer) {
    HudPos position = { 8, 8 };

    for (const auto& item : m_items)
      position = item->render(ctx, key, options, renderer, position);
  }


  void HudItemSet::parseOption(const std::string& str, float& value) {
    try {
      value = std::stof(str);
    } catch (const std::invalid_argument&) {
      return;
    }
  }


  HudPos HudVersionItem::render(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xffffffffu, "DXVK " DXVK_VERSION);

    position.y += 8;
    return position;
  }


  HudClientApiItem::HudClientApiItem(std::string api)
  : m_api(std::move(api)) {

  }


  HudClientApiItem::~HudClientApiItem() {

  }


  HudPos HudClientApiItem::render(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    std::lock_guard lock(m_mutex);

    position.y += 16;
    renderer.drawText(16, position, 0xffffffffu, m_api);

    position.y += 8;
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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xffffffffu, m_deviceName);
    
    position.y += 24;
    renderer.drawText(16, position, 0xffffffffu, m_driverName);
    
    position.y += 20;
    renderer.drawText(16, position, 0xffffffffu, m_driverVer);

    position.y += 8;
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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;

    renderer.drawText(16, position, 0xff4040ffu, "FPS:");
    renderer.drawText(16, { position.x + 60, position.y },
      0xffffffffu, m_frameRate);

    position.y += 8;
    return position;
  }


  HudFrameTimeItem::HudFrameTimeItem(const Rc<DxvkDevice>& device, HudRenderer* renderer)
  : m_device            (device),
    m_gfxSetLayout      (createDescriptorSetLayout()),
    m_gfxPipelineLayout (createPipelineLayout()) {
    createComputePipeline(*renderer);

    renderer->createShaderModule(m_vs, VK_SHADER_STAGE_VERTEX_BIT, sizeof(hud_graph_vert), hud_graph_vert);
    renderer->createShaderModule(m_fs, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(hud_graph_frag), hud_graph_frag);
  }


  HudFrameTimeItem::~HudFrameTimeItem() {
    auto vk = m_device->vkd();

    for (const auto& p : m_gfxPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second, nullptr);

    vk->vkDestroyShaderModule(vk->device(), m_vs.stageInfo.module, nullptr);
    vk->vkDestroyShaderModule(vk->device(), m_fs.stageInfo.module, nullptr);

    vk->vkDestroyPipeline(vk->device(), m_computePipeline, nullptr);
    vk->vkDestroyPipelineLayout(vk->device(), m_computePipelineLayout, nullptr);
    vk->vkDestroyDescriptorSetLayout(vk->device(), m_computeSetLayout, nullptr);

    vk->vkDestroyPipelineLayout(vk->device(), m_gfxPipelineLayout, nullptr);
    vk->vkDestroyDescriptorSetLayout(vk->device(), m_gfxSetLayout, nullptr);
  }


  HudPos HudFrameTimeItem::render(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    if (!m_gpuBuffer)
      createResources(ctx);

    HudPos minPos = {  12, -128 };
    HudPos maxPos = { 162, -128 };

    HudPos graphPos = { 8, -120 };
    HudPos graphSize = { NumDataPoints, 80 };

    uint32_t dataPoint = m_nextDataPoint++;

    processFrameTimes(ctx, key, renderer,
      dataPoint, minPos, maxPos);

    drawFrameTimeGraph(ctx, key, renderer,
      dataPoint, graphPos, graphSize);

    if (m_nextDataPoint >= NumDataPoints)
      m_nextDataPoint = 0u;

    return position;
  }


  void HudFrameTimeItem::processFrameTimes(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
          HudRenderer&        renderer,
          uint32_t            dataPoint,
          HudPos              minPos,
          HudPos              maxPos) {
    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture))) {
      ctx.cmd->cmdBeginDebugUtilsLabel(DxvkCmdBuffer::InitBuffer,
        vk::makeLabel(0xf0c0dc, "HUD frame time processing"));
    }

    // Write current time stamp to the buffer
    DxvkBufferSliceHandle sliceHandle = m_gpuBuffer->getSliceHandle();
    std::pair<VkQueryPool, uint32_t> query = m_query->getQuery();

    ctx.cmd->cmdResetQueryPool(DxvkCmdBuffer::InitBuffer,
      query.first, query.second, 1);

    ctx.cmd->cmdWriteTimestamp(DxvkCmdBuffer::InitBuffer,
      VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      query.first, query.second);

    ctx.cmd->cmdCopyQueryPoolResults(DxvkCmdBuffer::InitBuffer,
      query.first, query.second, 1, sliceHandle.handle,
      sliceHandle.offset + (dataPoint & 1u) * sizeof(uint64_t), sizeof(uint64_t),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.memoryBarrierCount = 1u;
    depInfo.pMemoryBarriers = &barrier;

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::InitBuffer, &depInfo);

    // Process contents of the buffer and write out text draws
    VkDescriptorSet set = ctx.descriptorPool->alloc(m_computeSetLayout);

    auto bufferLayout = computeBufferLayout();

    VkDescriptorBufferInfo frameTimeBuffer = m_gpuBuffer->getDescriptor(
      0, bufferLayout.timestampSize).buffer;

    VkDescriptorBufferInfo drawInfoBuffer = m_gpuBuffer->getDescriptor(
      bufferLayout.drawInfoOffset, bufferLayout.drawInfoSize).buffer;

    VkDescriptorBufferInfo drawParamBuffer = m_gpuBuffer->getDescriptor(
      bufferLayout.drawParamOffset, bufferLayout.drawParamSize).buffer;

    VkBufferView textBufferView = m_textView->handle();

    std::array<VkWriteDescriptorSet, 4> descriptorWrites = {{
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &frameTimeBuffer },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &drawParamBuffer },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &drawInfoBuffer },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, nullptr, nullptr, &textBufferView },
    }};

    ctx.cmd->updateDescriptorSets(
      descriptorWrites.size(),
      descriptorWrites.data());

    ctx.cmd->cmdBindPipeline(DxvkCmdBuffer::InitBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);

    ctx.cmd->cmdBindDescriptorSet(DxvkCmdBuffer::InitBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout,
      set, 0, nullptr);

    ComputePushConstants pushConstants = { };
    pushConstants.msPerTick = m_device->properties().core.properties.limits.timestampPeriod / 1000000.0f;
    pushConstants.dataPoint = dataPoint;
    pushConstants.textPosMinX = minPos.x + 48;
    pushConstants.textPosMinY = minPos.y;
    pushConstants.textPosMaxX = maxPos.x + 48;
    pushConstants.textPosMaxY = maxPos.y;

    ctx.cmd->cmdPushConstants(DxvkCmdBuffer::InitBuffer,
      m_computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
      0, sizeof(pushConstants), &pushConstants);

    ctx.cmd->cmdDispatch(DxvkCmdBuffer::InitBuffer, 1, 1, 1);

    barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = m_gpuBuffer->info().access;
    barrier.dstStageMask = m_gpuBuffer->info().stages;

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::InitBuffer, &depInfo);

    // Display the min/max numbers
    renderer.drawText(12, minPos, 0xff4040ff, "min:");
    renderer.drawText(12, maxPos, 0xff4040ff, "max:");

    renderer.drawTextIndirect(ctx, key, drawParamBuffer,
      drawInfoBuffer, textBufferView, 2u);

    if (unlikely(m_device->debugFlags().test(DxvkDebugFlag::Capture)))
      ctx.cmd->cmdEndDebugUtilsLabel(DxvkCmdBuffer::InitBuffer);

    // Make sure GPU resources are being kept alive as necessary
    ctx.cmd->track(m_gpuBuffer, DxvkAccess::Write);
    ctx.cmd->track(m_query);
  }


  void HudFrameTimeItem::drawFrameTimeGraph(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
          HudRenderer&        renderer,
          uint32_t            dataPoint,
          HudPos              graphPos,
          HudPos              graphSize) {
    ctx.cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, getPipeline(renderer, key));

    auto set = ctx.descriptorPool->alloc(m_gfxSetLayout);

    VkDescriptorBufferInfo bufferDescriptor = m_gpuBuffer->getDescriptor(0,
      computeBufferLayout().timestampSize).buffer;

    VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    descriptorWrite.dstSet = set;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferDescriptor;

    ctx.cmd->updateDescriptorSets(1, &descriptorWrite);

    ctx.cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, m_gfxPipelineLayout,
      set, 0, nullptr);

    RenderPushConstants pushConstants = { };
    pushConstants.hud = renderer.getPushConstants();
    pushConstants.x = graphPos.x;
    pushConstants.y = graphPos.y;
    pushConstants.w = graphSize.x;
    pushConstants.h = graphSize.y;
    pushConstants.frameIndex = dataPoint;

    ctx.cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer, m_gfxPipelineLayout,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      0, sizeof(pushConstants), &pushConstants);

    ctx.cmd->cmdDraw(4, 1, 0, 0);

    ctx.cmd->track(m_gpuBuffer, DxvkAccess::Read);
  }


  void HudFrameTimeItem::createResources(
    const DxvkContextObjects& ctx) {
    auto bufferLayout = computeBufferLayout();

    DxvkBufferCreateInfo bufferInfo = { };
    bufferInfo.size = bufferLayout.totalSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                     | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                     | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                     | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                     | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
                     | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    bufferInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                      | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
                      | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                      | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                      | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    bufferInfo.access = VK_ACCESS_TRANSFER_READ_BIT
                      | VK_ACCESS_TRANSFER_WRITE_BIT
                      | VK_ACCESS_INDIRECT_COMMAND_READ_BIT
                      | VK_ACCESS_SHADER_READ_BIT
                      | VK_ACCESS_SHADER_WRITE_BIT;
    bufferInfo.debugName = "HUD frame time data";

    m_gpuBuffer = m_device->createBuffer(bufferInfo, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);

    DxvkBufferViewKey textViewInfo = { };
    textViewInfo.format = VK_FORMAT_R8_UINT;
    textViewInfo.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    textViewInfo.offset = bufferLayout.textOffset;
    textViewInfo.size = bufferLayout.textSize;

    m_textView = m_gpuBuffer->createView(textViewInfo);

    // Zero-init buffer so we don't display random garbage at the start
    DxvkBufferSliceHandle bufferSlice = m_gpuBuffer->getSliceHandle();

    ctx.cmd->cmdFillBuffer(DxvkCmdBuffer::InitBuffer,
      bufferSlice.handle, bufferSlice.offset, bufferSlice.length, 0u);

    VkMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    barrier.dstAccessMask = m_gpuBuffer->info().access;
    barrier.dstStageMask = m_gpuBuffer->info().stages;

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.memoryBarrierCount = 1u;
    depInfo.pMemoryBarriers = &barrier;

    ctx.cmd->cmdPipelineBarrier(DxvkCmdBuffer::InitBuffer, &depInfo);
    ctx.cmd->track(m_gpuBuffer, DxvkAccess::Write);

    m_query = m_device->createRawQuery(VK_QUERY_TYPE_TIMESTAMP);
  }


  void HudFrameTimeItem::createComputePipeline(
          HudRenderer&        renderer) {
    auto vk = m_device->vkd();

    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       1, VK_SHADER_STAGE_COMPUTE_BIT },
      { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       1, VK_SHADER_STAGE_COMPUTE_BIT },
      { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       1, VK_SHADER_STAGE_COMPUTE_BIT },
      { 3, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
    }};

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setLayoutInfo.bindingCount = bindings.size();
    setLayoutInfo.pBindings = bindings.data();

    VkResult vr = vk->vkCreateDescriptorSetLayout(vk->device(),
      &setLayoutInfo, nullptr, &m_computeSetLayout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create frame time compute set layout: ", vr));

    VkPushConstantRange pushConstantRange = { };
    pushConstantRange.offset = 0u;
    pushConstantRange.size = sizeof(ComputePushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = 1u;
    pipelineLayoutInfo.pSetLayouts = &m_computeSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    vr = vk->vkCreatePipelineLayout(vk->device(),
      &pipelineLayoutInfo, nullptr, &m_computePipelineLayout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create frame time compute pipeline layout: ", vr));

    HudShaderModule shader = { };
    renderer.createShaderModule(shader, VK_SHADER_STAGE_COMPUTE_BIT,
      sizeof(hud_frame_time_eval), hud_frame_time_eval);

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.stage = shader.stageInfo;
    info.layout = m_computePipelineLayout;
    info.basePipelineIndex = -1;

    vr = vk->vkCreateComputePipelines(vk->device(),
      VK_NULL_HANDLE, 1, &info, nullptr, &m_computePipeline);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create frame time compute pipeline: ", vr));

    vk->vkDestroyShaderModule(vk->device(), shader.stageInfo.module, nullptr);
  }


  VkDescriptorSetLayout HudFrameTimeItem::createDescriptorSetLayout() {
    auto vk = m_device->vkd();

    std::array<VkDescriptorSetLayoutBinding, 1> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = bindings.size();
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateDescriptorSetLayout(
      vk->device(), &info, nullptr, &layout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create frame time graphics pipeline descriptor set layout: ", vr));

    return layout;
  }


  VkPipelineLayout HudFrameTimeItem::createPipelineLayout() {
    auto vk = m_device->vkd();

    VkPushConstantRange pushConstantRange = { };
    pushConstantRange.offset = 0u;
    pushConstantRange.size = sizeof(RenderPushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount = 1u;
    info.pSetLayouts = &m_gfxSetLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushConstantRange;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreatePipelineLayout(
      vk->device(), &info, nullptr, &layout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create frame time graphics pipeline layout: ", vr));

    return layout;
  }


  VkPipeline HudFrameTimeItem::getPipeline(
          HudRenderer&        renderer,
    const HudPipelineKey&     key) {
    auto entry = m_gfxPipelines.find(key);

    if (entry != m_gfxPipelines.end())
      return entry->second;

    VkPipeline pipeline = createPipeline(renderer, key);
    m_gfxPipelines.insert({ key, pipeline });
    return pipeline;
  }


  VkPipeline HudFrameTimeItem::createPipeline(
          HudRenderer&        renderer,
    const HudPipelineKey&     key) {
    auto vk = m_device->vkd();

    HudSpecConstants specConstants = renderer.getSpecConstants(key);
    VkSpecializationInfo specInfo = renderer.getSpecInfo(&specConstants);

    std::array<VkPipelineShaderStageCreateInfo, 2> stages = { };
    stages[0] = m_vs.stageInfo;
    stages[1] = m_fs.stageInfo;
    stages[1].pSpecializationInfo = &specInfo;

    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rtInfo.colorAttachmentCount = 1;
    rtInfo.pColorAttachmentFormats = &key.format;

    VkPipelineVertexInputStateCreateInfo viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

    VkPipelineRasterizationStateCreateInfo rsState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsState.cullMode = VK_CULL_MODE_NONE;
    rsState.polygonMode = VK_POLYGON_MODE_FILL;
    rsState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.lineWidth = 1.0f;

    constexpr uint32_t sampleMask = 0x1;

    VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msState.pSampleMask = &sampleMask;

    VkPipelineColorBlendAttachmentState cbAttachment = { };
    cbAttachment.blendEnable = VK_TRUE;
    cbAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    cbAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    cbAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbOpaqueState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbOpaqueState.attachmentCount = 1;
    cbOpaqueState.pAttachments = &cbAttachment;

    static const std::array<VkDynamicState, 2> dynStates = {
      VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    };

    VkPipelineDynamicStateCreateInfo dynState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount = dynStates.size();
    dynState.pDynamicStates = dynStates.data();

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rtInfo };
    info.stageCount = stages.size();
    info.pStages = stages.data();
    info.pVertexInputState = &viState;
    info.pInputAssemblyState = &iaState;
    info.pViewportState = &vpState;
    info.pRasterizationState = &rsState;
    info.pMultisampleState = &msState;
    info.pColorBlendState = &cbOpaqueState;
    info.pDynamicState = &dynState;
    info.layout = m_gfxPipelineLayout;
    info.basePipelineIndex = -1;

    VkPipeline pipeline = { };
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(),
      VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create HUD memory detail pipeline 1: ", vr));

    return pipeline;
  }


  HudFrameTimeItem::BufferLayout HudFrameTimeItem::computeBufferLayout() {
    struct ComputeTimestampBuffer {
      std::array<uint64_t, 2u> timestamps;
      std::array<float, NumDataPoints> intervals;
      float avgMs;
      float minMs;
      float maxMs;
    };

    BufferLayout result = { };
    result.timestampSize = align(sizeof(ComputeTimestampBuffer), 256u);
    result.drawInfoOffset = result.timestampSize;
    result.drawInfoSize = align(sizeof(HudTextDrawInfo) * NumTextDraws, 256u);
    result.drawParamOffset = result.drawInfoOffset + result.drawInfoSize;
    result.drawParamSize = align(sizeof(VkDrawIndirectCommand) * NumTextDraws, 256u);
    result.textOffset = result.drawParamOffset + result.drawParamSize;
    result.textSize = 256u;
    result.totalSize = result.textOffset + result.textSize;
    return result;
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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xff4080ff, "Queue submissions:");
    renderer.drawText(16, { position.x + 228, position.y }, 0xffffffffu, m_submitString);

    position.y += 20;
    renderer.drawText(16, position, 0xff4080ff, "Queue syncs:");
    renderer.drawText(16, { position.x + 228, position.y }, 0xffffffffu, m_syncString);

    position.y += 8;
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
      m_drawCallCount   = diffCounters.getCtr(DxvkStatCounter::CmdDrawCalls);
      m_drawCount       = diffCounters.getCtr(DxvkStatCounter::CmdDrawsMerged) + m_drawCallCount;
      m_dispatchCount   = diffCounters.getCtr(DxvkStatCounter::CmdDispatchCalls);
      m_renderPassCount = diffCounters.getCtr(DxvkStatCounter::CmdRenderPassCount);
      m_barrierCount    = diffCounters.getCtr(DxvkStatCounter::CmdBarrierCount);

      m_lastUpdate = time;
    }

    m_prevCounters = counters;
  }


  HudPos HudDrawCallStatsItem::render(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    std::string drawCount = m_drawCount > m_drawCallCount
      ? str::format(m_drawCallCount, " (", m_drawCount, ")")
      : str::format(m_drawCallCount);

    position.y += 16;
    renderer.drawText(16, position, 0xffff8040, "Draw calls:");
    renderer.drawText(16, { position.x + 192, position.y }, 0xffffffffu, drawCount);
    
    position.y += 20;
    renderer.drawText(16, position, 0xffff8040, "Dispatch calls:");
    renderer.drawText(16, { position.x + 192, position.y }, 0xffffffffu, str::format(m_dispatchCount));
    
    position.y += 20;
    renderer.drawText(16, position, 0xffff8040, "Render passes:");
    renderer.drawText(16, { position.x + 192, position.y }, 0xffffffffu, str::format(m_renderPassCount));
    
    position.y += 20;
    renderer.drawText(16, position, 0xffff8040, "Barriers:");
    renderer.drawText(16, { position.x + 192, position.y }, 0xffffffffu, str::format(m_barrierCount));
    
    position.y += 8;
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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xffff40ff, "Graphics pipelines:");
    renderer.drawText(16, { position.x + 240, position.y }, 0xffffffffu, str::format(m_graphicsPipelines));

    if (m_graphicsLibraries) {
      position.y += 20;
      renderer.drawText(16, position, 0xffff40ff, "Graphics shaders:");
      renderer.drawText(16, { position.x + 240, position.y }, 0xffffffffu, str::format(m_graphicsLibraries));
    }

    position.y += 20;
    renderer.drawText(16, position, 0xffff40ff, "Compute shaders:");
    renderer.drawText(16, { position.x + 240, position.y }, 0xffffffffu, str::format(m_computePipelines));

    position.y += 8;
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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xff8040ff, "Descriptor pools:");
    renderer.drawText(16, { position.x + 216, position.y }, 0xffffffffu, str::format(m_descriptorPoolCount));

    position.y += 20;
    renderer.drawText(16, position, 0xff8040ff, "Descriptor sets:");
    renderer.drawText(16, { position.x + 216, position.y }, 0xffffffffu, str::format(m_descriptorSetCount));

    position.y += 8;
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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
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

      position.y += 16;
      renderer.drawText(16, position, 0xff40ffffu, label);
      renderer.drawText(16, { position.x + 168, position.y }, 0xffffffffu, text);

      position.y += 4;
    }

    position.y += 4;
    return position;
  }




  HudMemoryDetailsItem::HudMemoryDetailsItem(
    const Rc<DxvkDevice>&     device,
          HudRenderer*        renderer)
  : m_device          (device),
    m_setLayout       (createSetLayout()),
    m_pipelineLayout  (createPipelineLayout()) {
    renderer->createShaderModule(m_fsBackground, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(hud_chunk_frag_background), hud_chunk_frag_background);
    renderer->createShaderModule(m_vsBackground, VK_SHADER_STAGE_VERTEX_BIT, sizeof(hud_chunk_vert_background), hud_chunk_vert_background);
    renderer->createShaderModule(m_fsVisualize, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(hud_chunk_frag_visualize), hud_chunk_frag_visualize);
    renderer->createShaderModule(m_vsVisualize, VK_SHADER_STAGE_VERTEX_BIT, sizeof(hud_chunk_vert_visualize), hud_chunk_vert_visualize);
  }


  HudMemoryDetailsItem::~HudMemoryDetailsItem() {
    auto vk = m_device->vkd();

    for (const auto& p : m_pipelines) {
      vk->vkDestroyPipeline(vk->device(), p.second.background, nullptr);
      vk->vkDestroyPipeline(vk->device(), p.second.visualize, nullptr);
    }

    vk->vkDestroyShaderModule(vk->device(), m_vsBackground.stageInfo.module, nullptr);
    vk->vkDestroyShaderModule(vk->device(), m_fsBackground.stageInfo.module, nullptr);

    vk->vkDestroyShaderModule(vk->device(), m_vsVisualize.stageInfo.module, nullptr);
    vk->vkDestroyShaderModule(vk->device(), m_fsVisualize.stageInfo.module, nullptr);

    vk->vkDestroyPipelineLayout(vk->device(), m_pipelineLayout, nullptr);
    vk->vkDestroyDescriptorSetLayout(vk->device(), m_setLayout, nullptr);
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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    // Layout, align the entire element to the bottom right.
    int32_t x = -564;
    int32_t y = -20;

    if (m_displayCacheStats) {
      uint32_t hitCount = m_cacheStats.requestCount - m_cacheStats.missCount;
      uint32_t hitRate = (100 * hitCount) / std::max(m_cacheStats.requestCount, 1u);

      std::string cacheStr = str::format("Cache: ", m_cacheStats.size >> 10, " kB (", hitRate, "% hit)");
      renderer.drawText(14, { x, y }, 0xffffffffu, cacheStr);

      y -= 24;
    }

    for (uint32_t i = m_stats.memoryTypes.size(); i; i--) {
      const auto& type = m_stats.memoryTypes.at(i - 1);

      if (!type.allocated)
        continue;

      // Compute layout and gather memory stats
      DxvkMemoryStats stats = { };

      int32_t w = 0;
      int32_t h = 0;

      for (uint32_t j = 0; j < type.chunkCount; j++) {
        const auto& chunk = m_stats.chunks.at(type.chunkIndex + j);
        stats.memoryAllocated += chunk.capacity;
        stats.memoryUsed += chunk.used;

        int32_t chunkWidth = (chunk.pageCount + 15u) / 16u + 2;

        if (x + w + chunkWidth > -8) {
          w = 0;
          h += 34;
        }

        w += chunkWidth + 6;
      }

      if (w)
        h += 34;

      y -= h;

      w = 0;
      h = 8;

      // Draw individual chunks
      for (uint32_t j = 0; j < type.chunkCount; j++) {
        const auto& chunk = m_stats.chunks.at(type.chunkIndex + j);

        // Default VRAM, blue
        uint32_t color = 0xff804020u;

        if (type.properties.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
          // Cached memory, green
          color = 0xff208020u;
        } else if (!(type.properties.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
          if (!chunk.mapped) {
            // Fallback allocation, grey
            color = 0xff202020u;
          } else {
            // Uncached memory, red
            color = 0xff202080u;
          }
        } else if (chunk.mapped) {
          // Host-visible VRAM, yellow
          color = 0xff208080u;
        }

        int32_t chunkWidth = (chunk.pageCount + 15u) / 16u + 2;

        if (x + w + chunkWidth > -8) {
          w = 0;
          h += 34;
        }

        drawChunk({ x + w, y + h }, { chunkWidth, 24 }, color, chunk);

        w += chunkWidth + 6;
      }

      // Render descriptive text
      std::string headline = str::format("Mem type ", (i - 1), " [", type.properties.heapIndex, "]: ",
        type.chunkCount, " chunk", type.chunkCount != 1u ? "s" : "", " (", (stats.memoryAllocated >> 20u), " MB, ",
        ((stats.memoryUsed >= (1u << 20u)) ? stats.memoryUsed >> 20 : stats.memoryUsed >> 10),
        (stats.memoryUsed >= (1u << 20u) ? " MB" : " kB"), " used)");

      renderer.drawText(14, { x, y }, 0xffffffffu, headline);

      y -= 24;
    }

    flushDraws(ctx, key, options, renderer);
    return position;
  }


  void HudMemoryDetailsItem::drawChunk(
          HudPos            pos,
          HudPos            size,
          uint32_t          color,
    const DxvkMemoryChunkStats& chunk) {
    auto& draw = m_drawInfos.emplace_back();
    draw.x = pos.x;
    draw.y = pos.y;
    draw.w = size.x;
    draw.h = size.y;
    draw.pageMask = chunk.pageMaskOffset;
    draw.pageCountAndActiveBit = chunk.pageCount;
    draw.color = color;

    if (chunk.active)
      draw.pageCountAndActiveBit |= 1u << 15;
  }


  void HudMemoryDetailsItem::flushDraws(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer) {
    if (m_drawInfos.empty())
      return;

    PipelinePair pipelines = getPipeline(renderer, key);

    ctx.cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.background);

    // Bind resources
    VkDescriptorSet set = ctx.descriptorPool->alloc(m_setLayout);

    VkDescriptorBufferInfo drawDescriptor = { };
    VkDescriptorBufferInfo dataDescriptor = { };

    updateDataBuffer(ctx, drawDescriptor, dataDescriptor);

    std::array<VkWriteDescriptorSet, 2> descriptorWrites = {{
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &drawDescriptor },
      { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        set, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &dataDescriptor },
    }};

    ctx.cmd->updateDescriptorSets(
      descriptorWrites.size(),
      descriptorWrites.data());

    ctx.cmd->cmdBindDescriptorSet(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
      set, 0, nullptr);

    HudPushConstants pushConstants = renderer.getPushConstants();

    ctx.cmd->cmdPushConstants(DxvkCmdBuffer::ExecBuffer, m_pipelineLayout,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      0, sizeof(pushConstants), &pushConstants);

    ctx.cmd->cmdDraw(4, m_drawInfos.size(), 0, 0);

    ctx.cmd->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.visualize);
    ctx.cmd->cmdDraw(4, m_drawInfos.size(), 0, 0);

    // Track data buffer lifetime
    ctx.cmd->track(m_dataBuffer, DxvkAccess::Read);

    m_drawInfos.clear();
  }


  void HudMemoryDetailsItem::updateDataBuffer(
    const DxvkContextObjects& ctx,
          VkDescriptorBufferInfo& drawDescriptor,
          VkDescriptorBufferInfo& dataDescriptor) {
    size_t drawInfoSize = m_drawInfos.size() * sizeof(DrawInfo);
    size_t drawInfoSizeAligned = align(drawInfoSize, 256u);

    size_t chunkDataSize = m_stats.pageMasks.size() * sizeof(uint32_t);
    size_t chunkDataSizeAligned = align(chunkDataSize, 256u);

    size_t bufferSize = align(drawInfoSizeAligned + chunkDataSizeAligned, 2048u);

    if (!m_dataBuffer || m_dataBuffer->info().size < bufferSize) {
      DxvkBufferCreateInfo bufferInfo;
      bufferInfo.size = bufferSize;
      bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      bufferInfo.access = VK_ACCESS_SHADER_READ_BIT;
      bufferInfo.stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      bufferInfo.debugName = "HUD memory data";

      m_dataBuffer = m_device->createBuffer(bufferInfo,
        VK_MEMORY_HEAP_DEVICE_LOCAL_BIT |
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    } else {
      // Ensure we can update the buffer without overriding live data
      auto allocation = m_dataBuffer->assignStorage(m_dataBuffer->allocateStorage());
      ctx.cmd->track(std::move(allocation));
    }

    // Update draw infos and pad unused area with zeroes
    std::memcpy(m_dataBuffer->mapPtr(0), m_drawInfos.data(), drawInfoSize);
    std::memset(m_dataBuffer->mapPtr(drawInfoSize), 0, drawInfoSizeAligned - drawInfoSize);

    // Update chunk data and pad with zeroes
    std::memcpy(m_dataBuffer->mapPtr(drawInfoSizeAligned), m_stats.pageMasks.data(), chunkDataSize);
    std::memset(m_dataBuffer->mapPtr(drawInfoSizeAligned + chunkDataSize), 0, chunkDataSizeAligned - chunkDataSize);

    // Write back descriptors
    drawDescriptor = m_dataBuffer->getDescriptor(0, drawInfoSizeAligned).buffer;
    dataDescriptor = m_dataBuffer->getDescriptor(drawInfoSizeAligned, chunkDataSizeAligned).buffer;
  }


  VkDescriptorSetLayout HudMemoryDetailsItem::createSetLayout() {
    auto vk = m_device->vkd();

    static const std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT   },
      { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = bindings.size();
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateDescriptorSetLayout(vk->device(), &info, nullptr, &layout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create HUD descriptor set layout: ", vr));

    return layout;
  }


  VkPipelineLayout HudMemoryDetailsItem::createPipelineLayout() {
    auto vk = m_device->vkd();

    VkPushConstantRange pushConstantRange = { };
    pushConstantRange.offset = 0u;
    pushConstantRange.size = sizeof(HudPushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount = 1;
    info.pSetLayouts = &m_setLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushConstantRange;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreatePipelineLayout(vk->device(), &info, nullptr, &layout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create HUD descriptor set layout: ", vr));

    return layout;
  }


  HudMemoryDetailsItem::PipelinePair HudMemoryDetailsItem::createPipeline(
          HudRenderer&        renderer,
    const HudPipelineKey&     key) {
    auto vk = m_device->vkd();

    HudSpecConstants specConstants = renderer.getSpecConstants(key);
    VkSpecializationInfo specInfo = renderer.getSpecInfo(&specConstants);

    std::array<VkPipelineShaderStageCreateInfo, 2> backgroundStages = { };
    backgroundStages[0] = m_vsBackground.stageInfo;
    backgroundStages[1] = m_fsBackground.stageInfo;
    backgroundStages[1].pSpecializationInfo = &specInfo;

    std::array<VkPipelineShaderStageCreateInfo, 2> visualizeStages = { };
    visualizeStages[0] = m_vsVisualize.stageInfo;
    visualizeStages[1] = m_fsVisualize.stageInfo;
    visualizeStages[1].pSpecializationInfo = &specInfo;

    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rtInfo.colorAttachmentCount = 1;
    rtInfo.pColorAttachmentFormats = &key.format;

    VkPipelineVertexInputStateCreateInfo viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

    VkPipelineRasterizationStateCreateInfo rsState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsState.cullMode = VK_CULL_MODE_NONE;
    rsState.polygonMode = VK_POLYGON_MODE_FILL;
    rsState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.lineWidth = 1.0f;

    constexpr uint32_t sampleMask = 0x1;

    VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msState.pSampleMask = &sampleMask;

    VkPipelineColorBlendAttachmentState cbAttachment = { };
    cbAttachment.blendEnable = VK_TRUE;
    cbAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    cbAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    cbAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbOpaqueState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbOpaqueState.attachmentCount = 1;
    cbOpaqueState.pAttachments = &cbAttachment;

    static const std::array<VkDynamicState, 2> dynStates = {
      VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    };

    VkPipelineDynamicStateCreateInfo dynState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount = dynStates.size();
    dynState.pDynamicStates = dynStates.data();

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rtInfo };
    info.stageCount = backgroundStages.size();
    info.pStages = backgroundStages.data();
    info.pVertexInputState = &viState;
    info.pInputAssemblyState = &iaState;
    info.pViewportState = &vpState;
    info.pRasterizationState = &rsState;
    info.pMultisampleState = &msState;
    info.pColorBlendState = &cbOpaqueState;
    info.pDynamicState = &dynState;
    info.layout = m_pipelineLayout;
    info.basePipelineIndex = -1;

    PipelinePair pipelines = { };
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE,
      1, &info, nullptr, &pipelines.background);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create HUD memory detail pipeline 1: ", vr));

    info.stageCount = visualizeStages.size();
    info.pStages = visualizeStages.data();

    vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE,
      1, &info, nullptr, &pipelines.visualize);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create HUD memory detail pipeline 2: ", vr));

    return pipelines;
  }


  HudMemoryDetailsItem::PipelinePair HudMemoryDetailsItem::getPipeline(
          HudRenderer&        renderer,
    const HudPipelineKey&     key) {
    auto entry = m_pipelines.find(key);

    if (entry != m_pipelines.end())
      return entry->second;

    PipelinePair pipeline = createPipeline(renderer, key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
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

      uint64_t currCsIdleTicks = counters.getCtr(DxvkStatCounter::CsIdleTicks);

      m_diffCsIdleTicks = currCsIdleTicks - m_prevCsIdleTicks;
      m_prevCsIdleTicks = currCsIdleTicks;

      uint64_t busyTicks = ticks > m_diffCsIdleTicks
        ? uint64_t(ticks - m_diffCsIdleTicks)
        : uint64_t(0);

      m_csLoadString = str::format((100 * busyTicks) / ticks, "%");

      m_maxCsSyncCount = 0;
      m_maxCsSyncTicks = 0;

      m_updateCount = 0;
      m_lastUpdate = time;
    }
  }


  HudPos HudCsThreadItem::render(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xff40ff40, "CS chunks:");
    renderer.drawText(16, { position.x + 132, position.y }, 0xffffffffu, m_csChunkString);

    position.y += 20;
    renderer.drawText(16, position, 0xff40ff40, "CS syncs:");
    renderer.drawText(16, { position.x + 132, position.y }, 0xffffffffu, m_csSyncString);

    position.y += 20;
    renderer.drawText(16, position, 0xff40ff40, "CS load:");
    renderer.drawText(16, { position.x + 132, position.y }, 0xffffffffu, m_csLoadString);

    position.y += 8;
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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    position.y += 16;
    renderer.drawText(16, position, 0xff408040u, "GPU:");
    renderer.drawText(16, { position.x + 60, position.y }, 0xffffffffu, m_gpuLoadString);

    position.y += 8;
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
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    if (m_show) {
      std::string string = "Compiling shaders...";

      if (m_showPercentage)
        string = str::format(string, " (", computePercentage(), "%)");

      renderer.drawText(16, { position.x, -20 }, 0xffffffffu, string);
    }

    return position;
  }


  uint32_t HudCompilerActivityItem::computePercentage() const {
    if (m_offset == m_tasksTotal)
      return 100;

    return (uint32_t(m_tasksDone - m_offset) * 100)
         / (uint32_t(m_tasksTotal - m_offset));
  }



  HudLatencyItem::HudLatencyItem() {

  }


  HudLatencyItem::~HudLatencyItem() {

  }


  void HudLatencyItem::accumulateStats(const DxvkLatencyStats& stats) {
    std::lock_guard lock(m_mutex);

    if (stats.frameLatency.count()) {
      m_accumStats.frameLatency += stats.frameLatency;
      m_accumStats.sleepDuration += stats.sleepDuration;

      m_accumFrames += 1u;
    } else {
      m_accumStats = { };
      m_accumFrames = 0u;
    }
  }


  void HudLatencyItem::update(dxvk::high_resolution_clock::time_point time) {
    uint64_t ticks = std::chrono::duration_cast<std::chrono::microseconds>(time - m_lastUpdate).count();

    if (ticks >= UpdateInterval) {
      std::lock_guard lock(m_mutex);

      if (m_accumFrames) {
        uint32_t latency = (m_accumStats.frameLatency / m_accumFrames).count() / 100u;
        uint32_t sleep = (m_accumStats.sleepDuration / m_accumFrames).count() / 100u;

        m_latencyString = str::format(latency / 10, ".", latency % 10, " ms");
        m_sleepString = str::format(sleep / 10, ".", sleep % 10, " ms");

        m_accumStats = { };
        m_accumFrames = 0u;

        m_invalidUpdates = 0u;
      } else {
        m_latencyString = "--";
        m_sleepString = "--";

        if (m_invalidUpdates < MaxInvalidUpdates)
          m_invalidUpdates += 1u;
      }

      m_lastUpdate = time;
    }
  }


  HudPos HudLatencyItem::render(
    const DxvkContextObjects& ctx,
    const HudPipelineKey&     key,
    const HudOptions&         options,
          HudRenderer&        renderer,
          HudPos              position) {
    if (m_invalidUpdates >= MaxInvalidUpdates)
      return position;

    position.y += 16;

    renderer.drawText(16, position, 0xffff60a0u, "Latency: ");
    renderer.drawText(16, { position.x + 108, position.y }, 0xffffffffu, m_latencyString);

    position.y += 20;

    renderer.drawText(16, position, 0xffff60a0u, "Sleep: ");
    renderer.drawText(16, { position.x + 108, position.y }, 0xffffffffu, m_sleepString);

    position.y += 8;
    return position;
  }

}
