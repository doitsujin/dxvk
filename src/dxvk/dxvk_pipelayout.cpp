#include <cstring>
#include <vector>

#include "dxvk_device.h"
#include "dxvk_descriptor.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"

namespace dxvk {
  
  uint32_t DxvkBindingInfo::computeSetIndex() const {
    if (stage == VK_SHADER_STAGE_COMPUTE_BIT) {
      // Use one single set for compute shaders
      return DxvkDescriptorSets::CsAll;
    } else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      // For fragment shaders, create a separate set for UBOs
      return uboSet
        ? DxvkDescriptorSets::FsBuffers
        : DxvkDescriptorSets::FsViews;
    } else {
      // Put all vertex shader resources into the last set.
      // Vertex shader UBOs are usually updated every draw,
      // and other resource types are rarely used.
      return DxvkDescriptorSets::VsAll;
    }
  }


  uint32_t DxvkBindingInfo::value() const {
    return (uint32_t(stage) << 24) | (uint32_t(descriptorType) << 16) | resourceBinding;
  }


  bool DxvkBindingInfo::eq(const DxvkBindingInfo& other) const {
    return descriptorType  == other.descriptorType
        && resourceBinding == other.resourceBinding
        && viewType        == other.viewType
        && stage           == other.stage
        && access          == other.access
        && accessOp        == other.accessOp
        && uboSet          == other.uboSet
        && isMultisampled  == other.isMultisampled;
  }


  size_t DxvkBindingInfo::hash() const {
    DxvkHashState hash;
    hash.add(uint32_t(descriptorType));
    hash.add(resourceBinding);
    hash.add(uint32_t(viewType));
    hash.add(uint32_t(stage));
    hash.add(access);
    hash.add(uint32_t(accessOp));
    hash.add(uint32_t(uboSet));
    hash.add(uint32_t(isMultisampled));
    return hash;
  }


  DxvkBindingList::DxvkBindingList() {

  }


  DxvkBindingList::~DxvkBindingList() {

  }


  void DxvkBindingList::addBinding(const DxvkBindingInfo& binding) {
    auto iter = m_bindings.begin();

    while (iter != m_bindings.end() && iter->value() <= binding.value())
      iter++;

    m_bindings.insert(iter, binding);
  }


  void DxvkBindingList::merge(const DxvkBindingList& list) {
    for (const auto& binding : list.m_bindings)
      addBinding(binding);
  }


  bool DxvkBindingList::eq(const DxvkBindingList& other) const {
    if (getBindingCount() != other.getBindingCount())
      return false;

    for (uint32_t i = 0; i < getBindingCount(); i++) {
      if (!getBinding(i).eq(other.getBinding(i)))
        return false;
    }

    return true;
  }


  size_t DxvkBindingList::hash() const {
    DxvkHashState hash;

    for (const auto& binding : m_bindings)
      hash.add(binding.hash());

    return hash;
  }


  DxvkDescriptorSetLayoutKey::DxvkDescriptorSetLayoutKey() {

  }


  DxvkDescriptorSetLayoutKey::DxvkDescriptorSetLayoutKey(const DxvkBindingList& list) {
    m_bindings.reserve(list.getBindingCount());

    for (uint32_t i = 0; i < list.getBindingCount(); i++) {
      add(DxvkDescriptorSetLayoutBinding(
        list.getBinding(i).descriptorType, 1u,
        list.getBinding(i).stage));
    }
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
        templateInfo.offset = sizeof(DxvkDescriptorInfo) * descriptorCount;
        templateInfo.stride = sizeof(DxvkDescriptorInfo);
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
      VkPushConstantRange pushConstants = key.getPushConstantRange(true).getPushConstantRange();

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
      VkPushConstantRange pushConstants = key.getPushConstantRange(false).getPushConstantRange();

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

    m_pushConstantsIndependent = builder.getPushConstantRange();
    m_pushConstantsComplete.maskStages(m_shaderStageMask);

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

            if (binding.getAccess() & vk::AccessWriteMask)
              appendDescriptors(m_readWriteResources, binding, dstMapping);
            else
              appendDescriptors(m_setReadOnlyResources[set], binding, dstMapping);
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
      m_shaderStageMask, m_pushConstantsIndependent, setCount, setLayouts.data()));
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





  DxvkBindingLayout::DxvkBindingLayout(VkShaderStageFlags stages)
  : m_pushConst { 0, 0, 0 }, m_pushConstStages(0), m_stages(stages), m_hazards(0u) {

  }


  DxvkBindingLayout::~DxvkBindingLayout() {

  }


  uint32_t DxvkBindingLayout::getSetMask() const {
    uint32_t mask = 0;

    if (m_stages & VK_SHADER_STAGE_COMPUTE_BIT)
      mask |= (1u << DxvkDescriptorSets::CsAll);

    if (m_stages & VK_SHADER_STAGE_FRAGMENT_BIT) {
      mask |= (1u << DxvkDescriptorSets::FsViews)
           |  (1u << DxvkDescriptorSets::FsBuffers);
    }

    if (m_stages & VK_SHADER_STAGE_VERTEX_BIT)
      mask |= (1u << DxvkDescriptorSets::VsAll);

    return mask;
  }


  void DxvkBindingLayout::addBinding(const DxvkBindingInfo& binding) {
    uint32_t set = binding.computeSetIndex();
    m_bindings[set].addBinding(binding);

    if ((binding.access & VK_ACCESS_2_SHADER_WRITE_BIT) && binding.accessOp == DxvkAccessOp::None)
      m_hazards |= 1u << set;
  }


  void DxvkBindingLayout::addPushConstantRange(VkPushConstantRange range) {
    uint32_t oldEnd = m_pushConst.offset + m_pushConst.size;
    uint32_t newEnd = range.offset + range.size;

    m_pushConst.stageFlags |= range.stageFlags;
    m_pushConst.offset = std::min(m_pushConst.offset, range.offset);
    m_pushConst.size = std::max(oldEnd, newEnd) - m_pushConst.offset;
  }


  void DxvkBindingLayout::addPushConstantStage(VkShaderStageFlagBits stage) {
    m_pushConstStages |= stage;
  }


  void DxvkBindingLayout::merge(const DxvkBindingLayout& layout) {
    for (uint32_t i = 0; i < layout.m_bindings.size(); i++)
      m_bindings[i].merge(layout.m_bindings[i]);

    addPushConstantRange(layout.m_pushConst);
    m_pushConstStages |= layout.m_pushConstStages;

    m_hazards |= layout.m_hazards;
  }


  bool DxvkBindingLayout::eq(const DxvkBindingLayout& other) const {
    if (m_stages != other.m_stages)
      return false;

    for (uint32_t i = 0; i < m_bindings.size(); i++) {
      if (!m_bindings[i].eq(other.m_bindings[i]))
        return false;
    }

    if (m_pushConstStages != other.m_pushConstStages)
      return false;

    if (m_pushConst.stageFlags != other.m_pushConst.stageFlags
     || m_pushConst.offset     != other.m_pushConst.offset
     || m_pushConst.size       != other.m_pushConst.size)
      return false;

    return true;
  }


  size_t DxvkBindingLayout::hash() const {
    DxvkHashState hash;
    hash.add(m_stages);

    for (uint32_t i = 0; i < m_bindings.size(); i++)
      hash.add(m_bindings[i].hash());

    hash.add(m_pushConstStages);
    hash.add(m_pushConst.stageFlags);
    hash.add(m_pushConst.offset);
    hash.add(m_pushConst.size);
    return hash;
  }


  DxvkBindingLayoutObjects::DxvkBindingLayoutObjects(
          DxvkDevice*                 device,
    const DxvkBindingLayout&          layout,
    const DxvkDescriptorSetLayout**   setObjects)
  : m_device(device), m_layout(layout) {
    auto vk = m_device->vkd();

    std::array<VkDescriptorSetLayout, DxvkDescriptorSets::SetCount> setLayouts = { };

    // Use minimum number of sets for the given pipeline layout type
    uint32_t setCount = m_layout.getStages() == VK_SHADER_STAGE_COMPUTE_BIT
      ? DxvkDescriptorSets::CsSetCount
      : DxvkDescriptorSets::SetCount;

    for (uint32_t i = 0; i < setCount; i++) {
      m_bindingObjects[i] = setObjects[i];

      // Sets can be null for partial layouts
      if (setObjects[i]) {
        setLayouts[i] = setObjects[i]->getSetLayout();

        uint32_t bindingCount = m_layout.getBindingCount(i);

        for (uint32_t j = 0; j < bindingCount; j++) {
          const DxvkBindingInfo& binding = m_layout.getBinding(i, j);

          m_map.add(
            DxvkShaderBinding(binding.stage, 0, binding.resourceBinding),
            DxvkShaderBinding(binding.stage, i, j));
        }

        if (bindingCount) {
          m_bindingCount += bindingCount;
          m_setMask |= 1u << i;
        }
      }
    }

    // Create pipeline layout objects
    VkPushConstantRange pushConstComplete = m_layout.getPushConstantRange(false);
    VkPushConstantRange pushConstIndependent = m_layout.getPushConstantRange(true);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = setCount;
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();

    if (pushConstComplete.stageFlags && pushConstComplete.size) {
      pipelineLayoutInfo.pushConstantRangeCount = 1;
      pipelineLayoutInfo.pPushConstantRanges = &pushConstComplete;
    }

    // If the full set is defined, create a layout without INDEPENDENT_SET_BITS
    if (m_layout.getSetMask() == (1u << setCount) - 1) {
      if (vk->vkCreatePipelineLayout(vk->device(), &pipelineLayoutInfo, nullptr, &m_completeLayout))
        throw DxvkError("DxvkBindingLayoutObjects: Failed to create pipeline layout");
    }

    // If graphics pipeline libraries are supported, also create a variand with the
    // bit. It will be used to create shader-based libraries and link pipelines.
    if (m_device->canUseGraphicsPipelineLibrary() && (m_layout.getStages() & VK_SHADER_STAGE_ALL_GRAPHICS)) {
      pipelineLayoutInfo.flags = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;

      if (pushConstIndependent.stageFlags && pushConstIndependent.size) {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstIndependent;
      }

      if (vk->vkCreatePipelineLayout(vk->device(), &pipelineLayoutInfo, nullptr, &m_independentLayout))
        throw DxvkError("DxvkBindingLayoutObjects: Failed to create pipeline layout");
    }
  }


  DxvkBindingLayoutObjects::~DxvkBindingLayoutObjects() {
    auto vk = m_device->vkd();

    vk->vkDestroyPipelineLayout(vk->device(), m_completeLayout, nullptr);
    vk->vkDestroyPipelineLayout(vk->device(), m_independentLayout, nullptr);
  }


  DxvkGlobalPipelineBarrier DxvkBindingLayoutObjects::getGlobalBarrier() const {
    DxvkGlobalPipelineBarrier barrier = { };

    for (uint32_t i = 0; i < DxvkDescriptorSets::SetCount; i++) {
      for (uint32_t j = 0; j < m_layout.getBindingCount(i); j++) {
        const auto& binding = m_layout.getBinding(i, j);
        barrier.stages |= util::pipelineStages(binding.stage);
        barrier.access |= binding.access;
      }
    }

    return barrier;
  }

}
