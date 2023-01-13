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
    return (uint32_t(descriptorType) << 24) | resourceBinding;
  }


  bool DxvkBindingInfo::eq(const DxvkBindingInfo& other) const {
    return descriptorType  == other.descriptorType
        && resourceBinding == other.resourceBinding
        && viewType        == other.viewType
        && stage           == other.stage
        && access          == other.access
        && uboSet          == other.uboSet;
  }


  size_t DxvkBindingInfo::hash() const {
    DxvkHashState hash;
    hash.add(uint32_t(descriptorType));
    hash.add(resourceBinding);
    hash.add(uint32_t(viewType));
    hash.add(uint32_t(stage));
    hash.add(access);
    hash.add(uint32_t(uboSet));
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


  DxvkBindingSetLayoutKey::DxvkBindingSetLayoutKey(const DxvkBindingList& list) {
    m_bindings.resize(list.getBindingCount());

    for (uint32_t i = 0; i < list.getBindingCount(); i++) {
      m_bindings[i].descriptorType = list.getBinding(i).descriptorType;
      m_bindings[i].stages         = list.getBinding(i).stage;
    }
  }


  DxvkBindingSetLayoutKey::~DxvkBindingSetLayoutKey() {

  }


  bool DxvkBindingSetLayoutKey::eq(const DxvkBindingSetLayoutKey& other) const {
    if (m_bindings.size() != other.m_bindings.size())
      return false;

    for (size_t i = 0; i < m_bindings.size(); i++) {
      if (m_bindings[i].descriptorType != other.m_bindings[i].descriptorType
       || m_bindings[i].stages         != other.m_bindings[i].stages)
        return false;
    }

    return true;
  }


  size_t DxvkBindingSetLayoutKey::hash() const {
    DxvkHashState hash;

    for (size_t i = 0; i < m_bindings.size(); i++) {
      hash.add(m_bindings[i].descriptorType);
      hash.add(m_bindings[i].stages);
    }

    return hash;
  }


  DxvkBindingSetLayout::DxvkBindingSetLayout(
          DxvkDevice*           device,
    const DxvkBindingSetLayoutKey& key)
  : m_device(device) {
    auto vk = m_device->vkd();

    std::vector<VkDescriptorSetLayoutBinding> bindingInfos;
    std::vector<VkDescriptorUpdateTemplateEntry> templateInfos;

    bindingInfos.reserve(key.getBindingCount());
    templateInfos.reserve(key.getBindingCount());

    for (uint32_t i = 0; i < key.getBindingCount(); i++) {
      auto entry = key.getBinding(i);

      VkDescriptorSetLayoutBinding bindingInfo;
      bindingInfo.binding = i;
      bindingInfo.descriptorType = entry.descriptorType;
      bindingInfo.descriptorCount = 1;
      bindingInfo.stageFlags = entry.stages;
      bindingInfo.pImmutableSamplers = nullptr;
      bindingInfos.push_back(bindingInfo);

      VkDescriptorUpdateTemplateEntry templateInfo;
      templateInfo.dstBinding = i;
      templateInfo.dstArrayElement = 0;
      templateInfo.descriptorCount = 1;
      templateInfo.descriptorType = entry.descriptorType;
      templateInfo.offset = sizeof(DxvkDescriptorInfo) * i;
      templateInfo.stride = sizeof(DxvkDescriptorInfo);
      templateInfos.push_back(templateInfo);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = bindingInfos.size();
    layoutInfo.pBindings = bindingInfos.data();

    if (vk->vkCreateDescriptorSetLayout(vk->device(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
      throw DxvkError("DxvkBindingSetLayoutKey: Failed to create descriptor set layout");

    if (layoutInfo.bindingCount) {
      VkDescriptorUpdateTemplateCreateInfo templateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
      templateInfo.descriptorUpdateEntryCount = templateInfos.size();
      templateInfo.pDescriptorUpdateEntries = templateInfos.data();
      templateInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
      templateInfo.descriptorSetLayout = m_layout;

      if (vk->vkCreateDescriptorUpdateTemplate(vk->device(), &templateInfo, nullptr, &m_template) != VK_SUCCESS)
        throw DxvkError("DxvkBindingLayoutObjects: Failed to create descriptor update template");
    }
  }


  DxvkBindingSetLayout::~DxvkBindingSetLayout() {
    auto vk = m_device->vkd();

    vk->vkDestroyDescriptorSetLayout(vk->device(), m_layout, nullptr);
    vk->vkDestroyDescriptorUpdateTemplate(vk->device(), m_template, nullptr);
  }


  DxvkBindingLayout::DxvkBindingLayout(VkShaderStageFlags stages)
  : m_pushConst { 0, 0, 0 }, m_stages(stages) {

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
  }


  void DxvkBindingLayout::addPushConstantRange(VkPushConstantRange range) {
    uint32_t oldEnd = m_pushConst.offset + m_pushConst.size;
    uint32_t newEnd = range.offset + range.size;

    m_pushConst.stageFlags |= range.stageFlags;
    m_pushConst.offset = std::min(m_pushConst.offset, range.offset);
    m_pushConst.size = std::max(oldEnd, newEnd) - m_pushConst.offset;
  }


  void DxvkBindingLayout::merge(const DxvkBindingLayout& layout) {
    for (uint32_t i = 0; i < layout.m_bindings.size(); i++)
      m_bindings[i].merge(layout.m_bindings[i]);

    addPushConstantRange(layout.m_pushConst);
  }


  bool DxvkBindingLayout::eq(const DxvkBindingLayout& other) const {
    if (m_stages != other.m_stages)
      return false;

    for (uint32_t i = 0; i < m_bindings.size(); i++) {
      if (!m_bindings[i].eq(other.m_bindings[i]))
        return false;
    }

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

    hash.add(m_pushConst.stageFlags);
    hash.add(m_pushConst.offset);
    hash.add(m_pushConst.size);
    return hash;
  }


  DxvkBindingLayoutObjects::DxvkBindingLayoutObjects(
          DxvkDevice*             device,
    const DxvkBindingLayout&      layout,
    const DxvkBindingSetLayout**  setObjects)
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

          DxvkBindingKey key;
          key.stage = binding.stage;
          key.binding = binding.resourceBinding;

          DxvkBindingMapping mapping;
          mapping.set = i;
          mapping.binding = j;

          m_mapping.insert({ key, mapping });
        }

        if (bindingCount) {
          m_bindingCount += bindingCount;
          m_setMask |= 1u << i;
        }
      }
    }

    // Create pipeline layout objects
    VkPushConstantRange pushConst = m_layout.getPushConstantRange();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = setCount;
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();

    if (pushConst.stageFlags && pushConst.size) {
      pipelineLayoutInfo.pushConstantRangeCount = 1;
      pipelineLayoutInfo.pPushConstantRanges = &pushConst;
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