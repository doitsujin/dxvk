#include "dxvk_shader_builtin.h"
#include "dxvk_meta_clear.h"
#include "dxvk_device.h"

namespace dxvk {
  
  DxvkMetaClearObjects::DxvkMetaClearObjects(DxvkDevice* device)
  : m_device(device) {

  }
  
  
  DxvkMetaClearObjects::~DxvkMetaClearObjects() {
    auto vk = m_device->vkd();

    for (const auto& p : m_pipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);
  }
  
  
  DxvkMetaClear DxvkMetaClearObjects::getPipeline(const DxvkMetaClear::Key& key) {
    std::lock_guard lock(m_mutex);

    auto entry = m_pipelines.find(key);

    if (entry != m_pipelines.end())
      return entry->second;

    auto pipeline = createPipeline(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }


  VkExtent3D DxvkMetaClearObjects::determineWorkgroupSize(const DxvkMetaClear::Key& key) const {
    // Pick workgroup size in such a way that we get reasonably
    // efficient access patterns while keeping workgroups small.
    VkExtent3D workgroupSize = { 64u, 1u, 1u };

    switch (key.viewType) {
      case VK_IMAGE_VIEW_TYPE_1D:
      case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
        workgroupSize.width = 64u;
        break;

      case VK_IMAGE_VIEW_TYPE_2D:
      case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        workgroupSize.width = 8u;
        workgroupSize.height = 8u;
        break;

      case VK_IMAGE_VIEW_TYPE_3D:
        workgroupSize.width = 4u;
        workgroupSize.height = 4u;
        workgroupSize.depth = 4u;
        break;

      case VK_IMAGE_VIEW_TYPE_MAX_ENUM:
        workgroupSize.width = 64u;
        break;

      default:
        Logger::err(str::format("DxvkMetaClearObjects: Unhandled view type: ", key.viewType));
        break;
    }

    return workgroupSize;
  }


  std::vector<uint32_t> DxvkMetaClearObjects::createShader(const DxvkMetaClear::Key& key, const DxvkPipelineLayout* layout) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key));
    DxvkBuiltInComputeShader compute = helper.buildComputeShader(builder, determineWorkgroupSize(key));

    // Declare UAV as either a buffer or image, depending on view type
    ir::ResourceKind kind = (key.viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM)
      ? ir::ResourceKind::eBufferTyped
      : helper.determineResourceKind(key.viewType, VK_SAMPLE_COUNT_1_BIT);

    ir::SsaDef uav = ir::resourceIsBuffer(kind)
      ? helper.declareTexelBufferUav(builder, 0u, "resource", key.format)
      : helper.declareImageUav(builder, 0u, "resource", key.viewType, key.format);

    // Declare and load shader arguments
    ir::BasicType valueType(helper.determineSampledType(key.format, VK_IMAGE_ASPECT_COLOR_BIT), 4u);
    ir::BasicType coordType(ir::ScalarType::eU32, 3u);

    ir::SsaDef clearValue = helper.declarePushData(builder, valueType, offsetof(DxvkMetaClear::Args, clearValue), "value");
    ir::SsaDef coordOffset = helper.declarePushData(builder, coordType, offsetof(DxvkMetaClear::Args, offset), "offset");
    ir::SsaDef coordExtent = helper.declarePushData(builder, coordType, offsetof(DxvkMetaClear::Args, extent), "extent");

    // Coordinate dimension, ignoring array layer. We know that the array layer will
    // always be in bounds, and corresponds to the .z coordinate of the thread ID.
    auto coordDims = ir::resourceCoordComponentCount(kind);

    // Bound-check against provided image extent
    auto cond = helper.emitBoundCheck(builder, compute.globalId, coordExtent, coordDims);
    helper.emitConditionalBlock(builder, cond);

    // Compute actual coordinates based on the theread index and offset
    auto coord = builder.add(ir::Op::IAdd(ir::BasicType(ir::ScalarType::eU32, coordDims),
      helper.emitExtractVector(builder, coordOffset, 0u, coordDims),
      helper.emitExtractVector(builder, compute.globalId, 0u, coordDims)));

    auto layer = ir::resourceIsLayered(kind)
      ? helper.emitExtractVector(builder, compute.globalId, 2u, 1u)
      : ir::SsaDef();

    // Only write out color components that are actually part of the format
    auto value = helper.emitFormatVector(builder, key.format, clearValue);

    if (ir::resourceIsBuffer(kind))
      builder.add(ir::Op::BufferStore(uav, coord, value, 0u));
    else
      builder.add(ir::Op::ImageStore(uav, layer, coord, value));

    return helper.buildShader(builder);
  }


  DxvkMetaClear DxvkMetaClearObjects::createPipeline(const DxvkMetaClear::Key& key) {
    VkDescriptorType descriptorType = key.viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM
      ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
      : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    DxvkDescriptorSetLayoutBinding binding(descriptorType, 1u, VK_SHADER_STAGE_COMPUTE_BIT);

    DxvkMetaClear result = { };
    result.layout = m_device->createBuiltInPipelineLayout(0u,
      VK_SHADER_STAGE_COMPUTE_BIT, sizeof(DxvkMetaClear::Args), 1u, &binding);

    std::vector<uint32_t> spirv = createShader(key, result.layout);

    result.pipeline = m_device->createBuiltInComputePipeline(result.layout, spirv);
    result.workgroupSize = determineWorkgroupSize(key);
    return result;
  }


  std::string DxvkMetaClearObjects::getName(const DxvkMetaClear::Key& key) {
    std::stringstream name;
    name << "meta_cs_clear_";

    if (key.viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM)
      name << "buffer";
    else
      name << "image_" << str::format(key.viewType).substr(std::strlen("VK_IMAGE_VIEW_TYPE_"));

    name << "_" << str::format(key.format).substr(std::strlen("VK_FORMAT_"));
    return str::tolower(name.str());
  }

}
