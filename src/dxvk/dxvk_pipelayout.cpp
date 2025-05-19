#include <cstring>
#include <vector>

#include "dxvk_device.h"
#include "dxvk_descriptor_pool.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"

namespace dxvk {
  
  DxvkDescriptorSetLayoutKey::DxvkDescriptorSetLayoutKey() {

  }


  DxvkDescriptorSetLayoutKey::~DxvkDescriptorSetLayoutKey() {

  }


  void DxvkDescriptorSetLayoutKey::add(DxvkDescriptorSetLayoutBinding binding) {
    m_bindings.push_back(binding);
  }


  bool DxvkDescriptorSetLayoutKey::eq(const DxvkDescriptorSetLayoutKey& other) const {
    bool eq = m_bindings.size() == other.m_bindings.size();

    for (size_t i = 0; i < m_bindings.size() && eq; i++)
      eq = m_bindings[i].eq(other.m_bindings[i]);

    return eq;
  }


  size_t DxvkDescriptorSetLayoutKey::hash() const {
    DxvkHashState hash;

    for (size_t i = 0; i < m_bindings.size(); i++)
      hash.add(m_bindings[i].hash());

    return hash;
  }


  DxvkDescriptorSetLayout::DxvkDescriptorSetLayout(
          DxvkDevice*                 device,
    const DxvkDescriptorSetLayoutKey& key)
  : m_device(device) {
    auto vk = m_device->vkd();

    size_t descriptorCount = 0u;

    small_vector<VkDescriptorSetLayoutBinding,    32> bindingInfos;
    small_vector<VkDescriptorUpdateTemplateEntry, 32> templateInfos;

    bindingInfos.reserve(key.getBindingCount());
    templateInfos.reserve(key.getBindingCount());

    for (uint32_t i = 0; i < key.getBindingCount(); i++) {
      auto entry = key.getBinding(i);

      if (entry.getDescriptorCount()) {
        VkDescriptorSetLayoutBinding bindingInfo;
        bindingInfo.binding = i;
        bindingInfo.descriptorType = entry.getDescriptorType();
        bindingInfo.descriptorCount = entry.getDescriptorCount();
        bindingInfo.stageFlags = entry.getStageMask();
        bindingInfo.pImmutableSamplers = nullptr;
        bindingInfos.push_back(bindingInfo);

        VkDescriptorUpdateTemplateEntry templateInfo;
        templateInfo.dstBinding = i;
        templateInfo.dstArrayElement = 0;
        templateInfo.descriptorCount = entry.getDescriptorCount();
        templateInfo.descriptorType = entry.getDescriptorType();
        templateInfo.offset = sizeof(DxvkLegacyDescriptor) * descriptorCount;
        templateInfo.stride = sizeof(DxvkLegacyDescriptor);
        templateInfos.push_back(templateInfo);

        descriptorCount += entry.getDescriptorCount();
      }
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = bindingInfos.size();
    layoutInfo.pBindings = bindingInfos.data();

    if (vk->vkCreateDescriptorSetLayout(vk->device(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
      throw DxvkError("DxvkDescriptorSetLayout: Failed to create descriptor set layout");

    if (layoutInfo.bindingCount) {
      VkDescriptorUpdateTemplateCreateInfo templateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
      templateInfo.descriptorUpdateEntryCount = templateInfos.size();
      templateInfo.pDescriptorUpdateEntries = templateInfos.data();
      templateInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
      templateInfo.descriptorSetLayout = m_layout;

      if (vk->vkCreateDescriptorUpdateTemplate(vk->device(), &templateInfo, nullptr, &m_template) != VK_SUCCESS)
        throw DxvkError("DxvkDescriptorSetLayout: Failed to create descriptor update template");
    }
  }


  DxvkDescriptorSetLayout::~DxvkDescriptorSetLayout() {
    auto vk = m_device->vkd();

    vk->vkDestroyDescriptorSetLayout(vk->device(), m_layout, nullptr);
    vk->vkDestroyDescriptorUpdateTemplate(vk->device(), m_template, nullptr);
  }


  DxvkPipelineLayout::DxvkPipelineLayout(
          DxvkDevice*                 device,
    const DxvkPipelineLayoutKey&      key)
  : m_device(device) {
    auto vk = m_device->vkd();

    // Gather descriptor set layout objects, some of these may be null.
    std::array<VkDescriptorSetLayout, DxvkPipelineLayoutKey::MaxSets> setLayouts = { };

    for (uint32_t i = 0; i < key.getDescriptorSetCount(); i++) {
      m_setLayouts[i] = key.getDescriptorSetLayout(i);

      if (m_setLayouts[i])
        setLayouts[i] = m_setLayouts[i]->getSetLayout();
    }

    // If we're creating a graphics pipeline layout, and if pipeline libraries are
    // supported by the implementation, create a set layout that is compatible with
    // pipeline libraries.
    if (device->canUseGraphicsPipelineLibrary() && (key.getStageMask() & VK_SHADER_STAGE_ALL_GRAPHICS)) {
      VkPushConstantRange pushConstants = key.getPushConstantRange().getPushConstantRange(true);

      VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
      layoutInfo.flags = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;
      layoutInfo.setLayoutCount = key.getDescriptorSetCount();

      if (layoutInfo.setLayoutCount)
        layoutInfo.pSetLayouts = setLayouts.data();

      if (pushConstants.size) {
        layoutInfo.pushConstantRangeCount = 1u;
        layoutInfo.pPushConstantRanges = &pushConstants;
      }

      if (vk->vkCreatePipelineLayout(vk->device(), &layoutInfo, nullptr, &m_layoutIndependent))
        throw DxvkError("DxvkPipelineLayout: Failed to create independent pipeline layout");
    }

    // If all descriptor set layouts are defined, create a pipeline layout object
    // that is optimal for monolithic pipelines and discards unused push constants.
    if (key.isComplete()) {
      VkPushConstantRange pushConstants = key.getPushConstantRange().getPushConstantRange(false);

      VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
      layoutInfo.setLayoutCount = key.getDescriptorSetCount();

      if (layoutInfo.setLayoutCount)
        layoutInfo.pSetLayouts = setLayouts.data();

      if (pushConstants.size) {
        layoutInfo.pushConstantRangeCount = 1u;
        layoutInfo.pPushConstantRanges = &pushConstants;
      }

      if (vk->vkCreatePipelineLayout(vk->device(), &layoutInfo, nullptr, &m_layoutComplete))
        throw DxvkError("DxvkPipelineLayout: Failed to create complete pipeline layout");
    }
  }


  DxvkPipelineLayout::~DxvkPipelineLayout() {
    auto vk = m_device->vkd();

    vk->vkDestroyPipelineLayout(vk->device(), m_layoutIndependent, nullptr);
    vk->vkDestroyPipelineLayout(vk->device(), m_layoutComplete, nullptr);
  }


  DxvkShaderBindingMap::DxvkShaderBindingMap() {

  }


  DxvkShaderBindingMap::~DxvkShaderBindingMap() {

  }


  void DxvkShaderBindingMap::add(DxvkShaderBinding srcBinding, DxvkShaderBinding dstBinding) {
    m_entries.insert_or_assign(srcBinding, dstBinding);
  }


  const DxvkShaderBinding* DxvkShaderBindingMap::find(DxvkShaderBinding srcBinding) const {
    auto entry = m_entries.find(srcBinding);

    if (entry == m_entries.end())
      return nullptr;

    return &entry->second;
  }


  DxvkPipelineBindings::DxvkPipelineBindings(
          DxvkPipelineManager*        manager,
    const DxvkPipelineLayoutBuilder&  builder) {
    m_shaderStageMask = builder.getStageMask();
    m_pushConstants = builder.getPushConstantRange();

    buildPipelineLayout(builder.getBindings(), manager);
  }


  DxvkPipelineBindings::~DxvkPipelineBindings() {

  }


  void DxvkPipelineBindings::buildPipelineLayout(DxvkPipelineBindingRange bindings, DxvkPipelineManager* manager) {
    // Generate descriptor set layout keys from all bindings
    std::array<DxvkDescriptorSetLayoutKey, MaxSets> setLayoutKeys = { };

    for (size_t i = 0; i < bindings.bindingCount; i++) {
      auto binding = bindings.bindings[i];
      auto set = mapToSet(binding);

      DxvkShaderBinding srcMapping(binding.getStageMask(), binding.getSet(), binding.getBinding());
      DxvkShaderBinding dstMapping(binding.getStageMask(), set, setLayoutKeys[set].getBindingCount());

      m_map.add(srcMapping, dstMapping);

      setLayoutKeys[set].add(DxvkDescriptorSetLayoutBinding(binding));

      if (binding.getDescriptorCount()) {
        appendDescriptors(m_setDescriptors[set], binding, dstMapping);

        if (binding.getDescriptorType() == VK_DESCRIPTOR_TYPE_SAMPLER
         || binding.getDescriptorType() == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
          appendDescriptors(m_setSamplers[set], binding, dstMapping);

        if (binding.getDescriptorType() != VK_DESCRIPTOR_TYPE_SAMPLER) {
          if (binding.isUniformBuffer()) {
            appendDescriptors(m_setUniformBuffers[set], binding, dstMapping);
          } else {
            appendDescriptors(m_setResources[set], binding, dstMapping);

            if (binding.getAccess() & vk::AccessWriteMask) {
              appendDescriptors(m_readWriteResources, binding, dstMapping);

              if (binding.getAccessOp() == DxvkAccessOp::None)
                m_hazardousStageMask |= binding.getStageMask();
            } else {
              appendDescriptors(m_setReadOnlyResources[set], binding, dstMapping);
            }
          }
        }

        m_barrier.stages |= util::pipelineStages(binding.getStageMask());
        m_barrier.access |= binding.getAccess();

        m_descriptorCount += binding.getDescriptorCount();
      }

      m_setMask |= 1u << set;
    }

    // Create set layouts for all stages covered by the layout
    uint32_t nonNullMask = getSetMaskForStages(m_shaderStageMask);
    std::array<const DxvkDescriptorSetLayout*, MaxSets> setLayouts = { };

    for (uint32_t i = 0; i < MaxSets; i++) {
      if (nonNullMask & (1u << i))
        setLayouts[i] = manager->createDescriptorSetLayout(setLayoutKeys[i]);
    }

    // Create pipeline layout with all known push constants and sets
    uint32_t setCount = getSetCountForStages(m_shaderStageMask);

    m_layout = manager->createPipelineLayout(DxvkPipelineLayoutKey(
      m_shaderStageMask, m_pushConstants, setCount, setLayouts.data()));
  }


  uint32_t DxvkPipelineBindings::mapToSet(const DxvkShaderDescriptor& binding) const {
    VkShaderStageFlags stage = binding.getStageMask();

    if (stage & VK_SHADER_STAGE_COMPUTE_BIT)
      return uint32_t(DxvkDescriptorSets::CsAll);

    if (stage & VK_SHADER_STAGE_FRAGMENT_BIT) {
      return binding.isUniformBuffer()
        ? uint32_t(DxvkDescriptorSets::FsBuffers)
        : uint32_t(DxvkDescriptorSets::FsViews);
    }

    return uint32_t(DxvkDescriptorSets::VsAll);
  }


  uint32_t DxvkPipelineBindings::getSetMaskForStages(VkShaderStageFlags stages) {
    uint32_t mask = 0u;

    if (stages & VK_SHADER_STAGE_COMPUTE_BIT)
      mask |= 1u << uint32_t(DxvkDescriptorSets::CsAll);

    if (stages & VK_SHADER_STAGE_FRAGMENT_BIT) {
      mask |= (1u << uint32_t(DxvkDescriptorSets::FsBuffers))
           |  (1u << uint32_t(DxvkDescriptorSets::FsViews));
    }

    if (stages & (VK_SHADER_STAGE_ALL_GRAPHICS & ~VK_SHADER_STAGE_FRAGMENT_BIT))
      mask |= 1u << uint32_t(DxvkDescriptorSets::VsAll);

    return mask;
  }


  uint32_t DxvkPipelineBindings::getSetCountForStages(VkShaderStageFlags stages) {
    return (stages & VK_SHADER_STAGE_COMPUTE_BIT)
      ? uint32_t(DxvkDescriptorSets::CsSetCount)
      : uint32_t(DxvkDescriptorSets::SetCount);
  }


  DxvkPipelineLayoutBuilder::DxvkPipelineLayoutBuilder() {

  }


  DxvkPipelineLayoutBuilder::DxvkPipelineLayoutBuilder(VkShaderStageFlags stageMask)
  : m_stageMask(stageMask) {

  }


  DxvkPipelineLayoutBuilder::~DxvkPipelineLayoutBuilder() {

  }


  void DxvkPipelineLayoutBuilder::addPushConstants(
          DxvkPushConstantRange     range) {
    m_pushConstants.merge(range);
  }


  void DxvkPipelineLayoutBuilder::addBindings(
          uint32_t                  bindingCount,
    const DxvkShaderDescriptor*     bindings) {
    size_t size = m_bindings.size();
    m_bindings.resize(size + bindingCount);

    for (uint32_t i = 0; i < bindingCount; i++) {
      size_t last = size + i;

      while (last && bindings[i].lt(m_bindings[last - 1u])) {
        m_bindings[last] = m_bindings[last - 1u];
        last -= 1u;
      }

      m_bindings[last] = bindings[i];
    }
  }


  void DxvkPipelineLayoutBuilder::addLayout(
    const DxvkPipelineLayoutBuilder& layout) {
    m_stageMask |= layout.m_stageMask;
    m_pushConstants.merge(layout.m_pushConstants);

    addBindings(layout.m_bindings.size(), layout.m_bindings.data());
  }

}
