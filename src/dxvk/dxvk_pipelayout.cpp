#include <cstring>

#include "dxvk_device.h"
#include "dxvk_descriptor.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"

namespace dxvk {
  
  uint32_t DxvkBindingInfo::computeSetIndex() const {
    if (stages & (VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT)) {
      // For fragment shaders, create a separate set for UBOs
      if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
       || descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        return DxvkDescriptorSets::FsBuffers;

      return DxvkDescriptorSets::FsViews;
    } else {
      // Put all vertex shader resources into the last set.
      // Vertex shader UBOs are usually updated every draw,
      // and other resource types are rarely used.
      return DxvkDescriptorSets::VsAll;
    }
  }


  bool DxvkBindingInfo::canMerge(const DxvkBindingInfo& binding) const {
    if ((stages & VK_SHADER_STAGE_FRAGMENT_BIT) != (binding.stages & VK_SHADER_STAGE_FRAGMENT_BIT))
      return false;

    return descriptorType    == binding.descriptorType
        && resourceBinding   == binding.resourceBinding
        && viewType          == binding.viewType;
  }


  void DxvkBindingInfo::merge(const DxvkBindingInfo& binding) {
    stages |= binding.stages;
    access |= binding.access;
  }


  bool DxvkBindingInfo::eq(const DxvkBindingInfo& other) const {
    return descriptorType    == other.descriptorType
        && resourceBinding   == other.resourceBinding
        && viewType          == other.viewType
        && stages            == other.stages
        && access            == other.access;
  }


  size_t DxvkBindingInfo::hash() const {
    DxvkHashState hash;
    hash.add(descriptorType);
    hash.add(resourceBinding);
    hash.add(viewType);
    hash.add(stages);
    hash.add(access);
    return hash;
  }


  DxvkBindingList::DxvkBindingList() {

  }


  DxvkBindingList::~DxvkBindingList() {

  }


  void DxvkBindingList::addBinding(const DxvkBindingInfo& binding) {
    for (auto& b : m_bindings) {
      if (b.canMerge(binding)) {
        b.merge(binding);
        return;
      }
    }

    m_bindings.push_back(binding);
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
      m_bindings[i].stages         = list.getBinding(i).stages;
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

    std::array<VkDescriptorSetLayoutBinding, MaxNumActiveBindings> bindingInfos;
    std::array<VkDescriptorUpdateTemplateEntry, MaxNumActiveBindings> templateInfos;

    VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = key.getBindingCount();
    layoutInfo.pBindings = bindingInfos.data();

    for (uint32_t i = 0; i < key.getBindingCount(); i++) {
      auto entry = key.getBinding(i);

      VkDescriptorSetLayoutBinding& bindingInfo = bindingInfos[i];
      bindingInfo.binding = i;
      bindingInfo.descriptorType = entry.descriptorType;
      bindingInfo.descriptorCount = 1;
      bindingInfo.stageFlags = entry.stages;
      bindingInfo.pImmutableSamplers = nullptr;

      VkDescriptorUpdateTemplateEntry& templateInfo = templateInfos[i];
      templateInfo.dstBinding = i;
      templateInfo.dstArrayElement = 0;
      templateInfo.descriptorCount = 1;
      templateInfo.descriptorType = entry.descriptorType;
      templateInfo.offset = sizeof(DxvkDescriptorInfo) * i;
      templateInfo.stride = sizeof(DxvkDescriptorInfo);
    }

    if (vk->vkCreateDescriptorSetLayout(vk->device(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
      throw DxvkError("DxvkBindingSetLayoutKey: Failed to create descriptor set layout");

    if (layoutInfo.bindingCount) {
      VkDescriptorUpdateTemplateCreateInfo templateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
      templateInfo.descriptorUpdateEntryCount = layoutInfo.bindingCount;
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


  DxvkBindingLayout::DxvkBindingLayout()
  : m_pushConst { 0, 0, 0 } {

  }


  DxvkBindingLayout::~DxvkBindingLayout() {

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

    uint32_t constId = 0;

    std::array<VkDescriptorSetLayout, DxvkDescriptorSets::SetCount> setLayouts;

    for (uint32_t i = 0; i < DxvkDescriptorSets::SetCount; i++) {
      m_bindingOffsets[i] = constId;
      m_bindingObjects[i] = setObjects[i];
      setLayouts[i] = setObjects[i]->getSetLayout();

      uint32_t bindingCount = m_layout.getBindingCount(i);

      for (uint32_t j = 0; j < bindingCount; j++) {
        const DxvkBindingInfo& binding = m_layout.getBinding(i, j);

        DxvkBindingMapping mapping;
        mapping.set = i;
        mapping.binding = j;
        mapping.constId = constId++;

        m_mapping.insert({ binding.resourceBinding, mapping });
      }

      if (bindingCount)
        m_setMask |= 1u << i;
    }

    VkPushConstantRange pushConst = m_layout.getPushConstantRange();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = setLayouts.size();
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();

    if (pushConst.stageFlags && pushConst.size) {
      pipelineLayoutInfo.pushConstantRangeCount = 1;
      pipelineLayoutInfo.pPushConstantRanges = &pushConst;
    }

    if (vk->vkCreatePipelineLayout(vk->device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout))
      throw DxvkError("DxvkBindingLayoutObjects: Failed to create pipeline layout");
  }


  DxvkBindingLayoutObjects::~DxvkBindingLayoutObjects() {
    auto vk = m_device->vkd();

    vk->vkDestroyPipelineLayout(vk->device(), m_pipelineLayout, nullptr);
  }


  VkAccessFlags DxvkBindingLayoutObjects::getAccessFlags() const {
    VkAccessFlags flags = 0;

    for (uint32_t i = 0; i < DxvkDescriptorSets::SetCount; i++) {
      for (uint32_t j = 0; j < m_layout.getBindingCount(i); j++)
        flags |= m_layout.getBinding(i, j).access;
    }

    return flags;
  }

}