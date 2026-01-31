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


  uint32_t DxvkDescriptorSetLayoutKey::add(DxvkDescriptorSetLayoutBinding binding) {
    uint32_t index = m_bindings.size();

    m_bindings.push_back(binding);
    return index;
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
  : m_device(device), m_bindingCount(key.getBindingCount()) {
    if (device->canUseDescriptorHeap()) {
      initDescriptorHeapLayout(key);
    } else {
      initSetLayout(key);

      if (m_device->canUseDescriptorBuffer())
        initDescriptorBufferUpdate(key);
    }
  }


  DxvkDescriptorSetLayout::~DxvkDescriptorSetLayout() {
    auto vk = m_device->vkd();

    if (!m_device->canUseDescriptorHeap()) {
      vk->vkDestroyDescriptorSetLayout(vk->device(), m_legacy.layout, nullptr);
      vk->vkDestroyDescriptorUpdateTemplate(vk->device(), m_legacy.updateTemplate, nullptr);
    }
  }


  void DxvkDescriptorSetLayout::initSetLayout(const DxvkDescriptorSetLayoutKey& key) {
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

    if (m_device->canUseDescriptorBuffer())
      layoutInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

    if (vk->vkCreateDescriptorSetLayout(vk->device(), &layoutInfo, nullptr, &m_legacy.layout))
      throw DxvkError("DxvkDescriptorSetLayout: Failed to create descriptor set layout");

    if (layoutInfo.bindingCount && !m_device->canUseDescriptorBuffer()) {
      VkDescriptorUpdateTemplateCreateInfo templateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO };
      templateInfo.descriptorUpdateEntryCount = templateInfos.size();
      templateInfo.pDescriptorUpdateEntries = templateInfos.data();
      templateInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
      templateInfo.descriptorSetLayout = m_legacy.layout;

      if (vk->vkCreateDescriptorUpdateTemplate(vk->device(), &templateInfo, nullptr, &m_legacy.updateTemplate))
        throw DxvkError("DxvkDescriptorSetLayout: Failed to create descriptor update template");
    }
  }


  void DxvkDescriptorSetLayout::initDescriptorBufferUpdate(const DxvkDescriptorSetLayoutKey& key) {
    auto vk = m_device->vkd();

    vk->vkGetDescriptorSetLayoutSizeEXT(vk->device(), m_legacy.layout, &m_heap.memorySize);
    m_heap.memorySize = align(m_heap.memorySize, m_device->getDescriptorProperties().getDescriptorSetAlignment());

    small_vector<DxvkDescriptorUpdateInfo, 32u> descriptors;

    for (uint32_t i = 0u; i < key.getBindingCount(); i++) {
      const auto& binding = key.getBinding(i);

      VkDeviceSize offset = 0u;
      vk->vkGetDescriptorSetLayoutBindingOffsetEXT(vk->device(), m_legacy.layout, i, &offset);

      auto& info = m_heap.bindingLayouts.emplace_back();
      info.descriptorType = binding.getDescriptorType();
      info.offset = uint32_t(offset);

      for (uint32_t j = 0u; j < binding.getDescriptorCount(); j++) {
        auto& e = descriptors.emplace_back();
        e.descriptorType = binding.getDescriptorType();
        e.offset = uint32_t(offset) + j * m_device->getDescriptorProperties().getDescriptorTypeInfo(e.descriptorType).size;
      }
    }

    m_heap.update = DxvkDescriptorUpdateList(m_device,
      m_heap.memorySize, descriptors.size(), descriptors.data());
  }


  void DxvkDescriptorSetLayout::initDescriptorHeapLayout(const DxvkDescriptorSetLayoutKey& key) {
    // As a small optimization, order descriptors by size alignment from
    // large to small. This way, we're guaranteed tight packing and Will
    // only ever have one area of padding at the end of the set.
    uint32_t typeAlignmentMask = 0u;

    for (uint32_t i = 0u; i < key.getBindingCount(); i++) {
      const auto& binding = key.getBinding(i);

      auto size = m_device->getDescriptorProperties().getDescriptorTypeInfo(binding.getDescriptorType()).size;
      size &= -size;

      typeAlignmentMask |= size;
    }

    // Compute the actual binding layout by iterating over the bindings
    // until we've processed all unique descriptor size alignments
    m_heap.bindingLayouts.resize(key.getBindingCount());

    uint32_t offset = 0u;

    while (typeAlignmentMask) {
      uint32_t msb = (0x80000000u >> bit::lzcnt(typeAlignmentMask));

      for (uint32_t i = 0u; i < key.getBindingCount(); i++) {
        const auto& binding = key.getBinding(i);

        auto type = m_device->getDescriptorProperties().getDescriptorTypeInfo(binding.getDescriptorType());

        if ((type.size & -type.size) != msb)
          continue;

        offset = align(offset, type.alignment);

        auto& info = m_heap.bindingLayouts[i];
        info.descriptorType = binding.getDescriptorType();
        info.offset = offset;

        offset += type.size * binding.getDescriptorCount();
      }

      typeAlignmentMask -= msb;
    }

    m_heap.memorySize = align(offset, m_device->getDescriptorProperties().getDescriptorSetAlignment());

    // Iterate over everything again to create the descriptor update list
    small_vector<DxvkDescriptorUpdateInfo, 32u> descriptors;

    for (uint32_t i = 0u; i < key.getBindingCount(); i++) {
      auto& info = m_heap.bindingLayouts[i];

      for (uint32_t j = 0u; j < key.getBinding(i).getDescriptorCount(); j++) {
        auto& e = descriptors.emplace_back();
        e.descriptorType = info.descriptorType;
        e.offset = info.offset + j * m_device->getDescriptorProperties().getDescriptorTypeInfo(e.descriptorType).size;
      }
    }

    m_heap.update = DxvkDescriptorUpdateList(m_device,
      m_heap.memorySize, descriptors.size(), descriptors.data());
  }


  DxvkPipelineLayout::DxvkPipelineLayout(
          DxvkDevice*                 device,
    const DxvkPipelineLayoutKey&      key)
  : m_device(device), m_flags(key.getFlags()) {
    initMetadata(key);

    if (m_device->canUseDescriptorHeap())
      initMappings(key);
    else
      initPipelineLayout(key);
  }


  DxvkPipelineLayout::~DxvkPipelineLayout() {
    auto vk = m_device->vkd();

    if (!m_device->canUseDescriptorHeap())
      vk->vkDestroyPipelineLayout(vk->device(), m_legacy.layout, nullptr);
  }


  void DxvkPipelineLayout::initMetadata(
    const DxvkPipelineLayoutKey&      key) {
    // Determine bind point based on shader stages
    m_bindPoint = (key.getStageMask() == VK_SHADER_STAGE_COMPUTE_BIT)
      ? VK_PIPELINE_BIND_POINT_COMPUTE
      : VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Get set layouts from pipeline layout key and compute memory size
    for (uint32_t i = 0; i < key.getDescriptorSetCount(); i++) {
      m_setLayouts[i] = key.getDescriptorSetLayout(i);

      m_heap.setMemorySize += key.getDescriptorSetLayout(i)
        ? key.getDescriptorSetLayout(i)->getMemorySize()
        : 0u;
    }

    // Compute merged push data block from all used blocks
    m_pushData.blockMask = key.getPushDataMask();

    for (auto i : bit::BitMask(m_pushData.blockMask)) {
      m_pushData.blocks[i] = key.getPushDataBlock(i);
      m_pushData.mergedBlock.merge(m_pushData.blocks[i]);
    }

    // If we can use heaps, and if we're not on AMD or similarly working hardware,
    // scale heap offsets by the minimum set alignment to make the driver aware.
    if (m_device->canUseDescriptorHeap() && !m_device->perfHints().preferDescriptorByteOffsets)
      m_heap.offsetShift = bit::tzcnt(m_device->getDescriptorProperties().getDescriptorSetAlignment());
  }


  void DxvkPipelineLayout::initPipelineLayout(
    const DxvkPipelineLayoutKey&      key) {
    auto vk = m_device->vkd();

    // Gather descriptor set layout objects, some of these may be null.
    small_vector<VkDescriptorSetLayout, DxvkPipelineLayoutKey::MaxSets + 1u> setLayouts;

    if (m_flags.test(DxvkPipelineLayoutFlag::UsesSamplerHeap))
      setLayouts.push_back(m_device->getSamplerDescriptorSet().layout);

    for (uint32_t i = 0; i < key.getDescriptorSetCount(); i++)
      setLayouts.push_back(m_setLayouts[i] ? m_setLayouts[i]->getSetLayout() : VK_NULL_HANDLE);

    // Set up push constant range, if any
    VkPushConstantRange pushConstantRange = { };
    pushConstantRange.stageFlags = m_pushData.mergedBlock.getStageMask();
    pushConstantRange.size = m_pushData.mergedBlock.getSize();

    VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    if (key.getType() == DxvkPipelineLayoutType::Independent)
      layoutInfo.flags = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;

    layoutInfo.setLayoutCount = setLayouts.size();

    if (layoutInfo.setLayoutCount)
      layoutInfo.pSetLayouts = setLayouts.data();

    if (pushConstantRange.size) {
      layoutInfo.pushConstantRangeCount = 1u;
      layoutInfo.pPushConstantRanges = &pushConstantRange;
    }

    if (vk->vkCreatePipelineLayout(vk->device(), &layoutInfo, nullptr, &m_legacy.layout))
      throw DxvkError("DxvkPipelineLayout: Failed to create pipeline layout");
  }


  void DxvkPipelineLayout::initMappings(
    const DxvkPipelineLayoutKey&      key) {
    uint32_t setIndex = 0u;

    // Set up sampler heap mapping at binding (0,0) if used by the layout
    if (m_flags.test(DxvkPipelineLayoutFlag::UsesSamplerHeap)) {
      auto& entry = m_mapping.mappings.emplace_back();
      entry.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
      entry.descriptorSet = setIndex++;
      entry.firstBinding = 0u;
      entry.bindingCount = 1u;
      entry.resourceMask = VK_SPIRV_RESOURCE_TYPE_SAMPLER_BIT_EXT;
      entry.source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;

      auto& samplers = entry.sourceData.constantOffset;
      samplers.heapOffset = m_device->getSamplerDescriptorHeap().reservedSize;
      samplers.heapArrayStride = m_device->getDescriptorProperties().getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_SAMPLER).size;
    }

    // We generally put set offsets first, unless it's a built-in
    // pipeline with a hardcoded push data layout.
    uint32_t pushBase = 0u;

    if (key.getType() == DxvkPipelineLayoutType::BuiltIn)
      pushBase = m_pushData.mergedBlock.getSize();

    for (uint32_t set = 0u; set < m_setLayouts.size(); set++) {
      auto layout = m_setLayouts[set];

      if (!layout)
        continue;

      for (uint32_t i = 0u; i < layout->getBindingCount(); i++) {
        auto bindingInfo = layout->getBindingInfo(i);

        auto& entry = m_mapping.mappings.emplace_back();
        entry.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
        entry.descriptorSet = setIndex + set;
        entry.firstBinding = i;
        entry.bindingCount = 1u;
        entry.resourceMask = VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
        entry.source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;

        auto& pushIndex = entry.sourceData.pushIndex;
        pushIndex.heapOffset = bindingInfo.offset;
        pushIndex.pushOffset = pushBase + sizeof(uint32_t) * set;
        pushIndex.heapIndexStride = 1u << m_heap.offsetShift;
        pushIndex.heapArrayStride = m_device->getDescriptorProperties().getDescriptorTypeInfo(bindingInfo.descriptorType).size;
      }
    }
  }


  DxvkShaderBindingMap::DxvkShaderBindingMap() {

  }


  DxvkShaderBindingMap::~DxvkShaderBindingMap() {

  }


  void DxvkShaderBindingMap::addBinding(DxvkShaderBinding srcBinding, DxvkShaderBinding dstBinding) {
    m_bindings.insert_or_assign(srcBinding, dstBinding);
  }


  void DxvkShaderBindingMap::addPushData(const DxvkPushDataBlock& block, uint32_t offset) {
    m_pushData.push_back(std::make_pair(block, offset));
  }


  const DxvkShaderBinding* DxvkShaderBindingMap::mapBinding(DxvkShaderBinding srcBinding) const {
    auto entry = m_bindings.find(srcBinding);

    if (entry == m_bindings.end())
      return nullptr;

    return &entry->second;
  }


  uint32_t DxvkShaderBindingMap::mapPushData(VkShaderStageFlags stage, uint32_t offset) const {
    for (size_t i = 0u; i < m_pushData.size(); i++) {
      const auto& block = m_pushData[i];

      if ((block.first.getStageMask() & stage)
       && offset >= block.first.getOffset()
       && offset < block.first.getOffset() + block.first.getSize())
        return block.second + offset - block.first.getOffset();
    }

    return -1u;
  }


  DxvkPipelineBindings::DxvkPipelineBindings(
          DxvkDevice*                 device,
          DxvkPipelineManager*        manager,
    const DxvkPipelineLayoutBuilder&  builder) {
    auto stageMask = builder.getStageMask();

    // Fill metadata structures that are independent of set layouts
    buildMetadata(builder);

    // Build pipeline layout for graphics pipeline libraries if applicable
    if ((stageMask & VK_SHADER_STAGE_ALL_GRAPHICS) && device->canUseGraphicsPipelineLibrary())
      buildPipelineLayout(DxvkPipelineLayoutType::Independent, device, builder, manager);

    // Build pipeline layout for monolithic pipelines if binding
    // layouts for all shader stages are known
    bool isComplete = stageMask == VK_SHADER_STAGE_COMPUTE_BIT;

    if (stageMask & VK_SHADER_STAGE_ALL_GRAPHICS) {
      isComplete = (stageMask & VK_SHADER_STAGE_FRAGMENT_BIT)
                && (stageMask & VK_SHADER_STAGE_VERTEX_BIT);
    }

    if (isComplete)
      buildPipelineLayout(DxvkPipelineLayoutType::Merged, device, builder, manager);
  }


  DxvkPipelineBindings::~DxvkPipelineBindings() {

  }


  void DxvkPipelineBindings::buildPipelineLayout(
          DxvkPipelineLayoutType      type,
          DxvkDevice*                 device,
    const DxvkPipelineLayoutBuilder&  builder,
          DxvkPipelineManager*        manager) {
    auto flags = getPipelineLayoutFlags(type, builder);

    auto setInfos = computeSetMaskAndCount(type,
      builder.getStageMask(), builder.getBindings());

    auto pushDataBlocks = buildPushDataBlocks(type, device, setInfos, builder, manager);

    // Descriptor processing needs to know the exact push data offsets
    auto setLayouts = buildDescriptorSetLayouts(type, flags, setInfos, builder, manager);

    // Create the actual pipeline layout
    DxvkPipelineLayoutKey key(type, flags, builder.getStageMask(),
      pushDataBlocks.size(), pushDataBlocks.data(),
      setLayouts.size(), setLayouts.data());

    auto& layout = m_layouts[uint32_t(type)];
    layout.layout = manager->createPipelineLayout(key);
  }


  small_vector<DxvkPushDataBlock, DxvkPushDataBlock::MaxBlockCount>
  DxvkPipelineBindings::buildPushDataBlocks(
          DxvkPipelineLayoutType      type,
          DxvkDevice*                 device,
    const SetInfos&                   setInfos,
    const DxvkPipelineLayoutBuilder&  builder,
          DxvkPipelineManager*        manager) {
    auto& layout = m_layouts[uint32_t(type)];

    // Process and re-map data blocks
    small_vector<DxvkPushDataBlock, DxvkPushDataBlock::MaxBlockCount> pushDataBlocks(DxvkPushDataBlock::MaxBlockCount);

    uint32_t pushDataMask = builder.getPushDataMask();
    uint32_t pushDataSize = 0u;

    // Reserve push data space for heap offsets
    if (type != DxvkPipelineLayoutType::BuiltIn && device->canUseDescriptorHeap())
      pushDataSize += sizeof(uint32_t) * setInfos.count;

    if (type == DxvkPipelineLayoutType::Independent) {
      // For independent layouts, we don't know in advance how the other stages
      // are going to use their push constants, so allocate the maximum amount.
      VkShaderStageFlags stageMask = VK_SHADER_STAGE_ALL_GRAPHICS & util::shaderStages(device->getShaderPipelineStages());

      uint32_t index = DxvkPushDataBlock::computeIndex(stageMask);

      pushDataSize = align(pushDataSize, sizeof(uint64_t));

      pushDataBlocks[index] = DxvkPushDataBlock(stageMask,
        pushDataSize, MaxSharedPushDataSize, 8u, 0u);
      pushDataSize += MaxSharedPushDataSize;

      pushDataMask |= 1u << index;

      for (auto i : bit::BitMask(stageMask)) {
        auto stage = VkShaderStageFlagBits(1u << i);
        index = DxvkPushDataBlock::computeIndex(stage);

        pushDataBlocks[index] = DxvkPushDataBlock(stage,
          pushDataSize, MaxPerStagePushDataSize, 8u, 0u);

        pushDataSize += MaxPerStagePushDataSize;
        pushDataMask |= 1u << index;
      }

      // Move blocks to pre-computed locations
      for (auto i : bit::BitMask(builder.getPushDataMask())) {
        auto block = builder.getPushDataBlock(i);

        block.rebase(
          pushDataBlocks[i].getOffset(),
          pushDataBlocks[i].getSize());

        pushDataBlocks[i] = block;
      }
    } else {
      // Pack push data as tightly as possible
      for (auto i : bit::BitMask(builder.getPushDataMask())) {
        auto block = builder.getPushDataBlock(i);

        pushDataSize = align(pushDataSize, block.getAlignment());

        pushDataBlocks[i] = block;
        pushDataBlocks[i].rebase(pushDataSize, block.getSize());

        pushDataSize += block.getSize();
      }
    }

    for (auto i : bit::BitMask(builder.getPushDataMask()))
      layout.bindingMap.addPushData(builder.getPushDataBlock(i), pushDataBlocks[i].getOffset());

    // Compact the array based on the bit mask
    uint32_t pushDataBlockCount = 0u;

    for (auto i : bit::BitMask(pushDataMask))
      pushDataBlocks[pushDataBlockCount++] = pushDataBlocks[i];

    pushDataBlocks.resize(pushDataBlockCount);
    return pushDataBlocks;
  }


  small_vector<const DxvkDescriptorSetLayout*, DxvkPipelineBindings::MaxSets>
  DxvkPipelineBindings::buildDescriptorSetLayouts(
          DxvkPipelineLayoutType      type,
          DxvkPipelineLayoutFlags     flags,
    const SetInfos&                   setInfos,
    const DxvkPipelineLayoutBuilder&  builder,
          DxvkPipelineManager*        manager) {
    auto bindings = builder.getBindings();

    auto& layout = m_layouts[uint32_t(type)];

    // Generate descriptor set layout keys from all bindings
    std::array<DxvkDescriptorSetLayoutKey, MaxSets> setLayoutKeys = { };

    for (size_t i = 0; i < bindings.bindingCount; i++) {
      auto binding = bindings.bindings[i];
      auto set = computeSetForBinding(type, binding);

      if (set < setInfos.map.size())
        set = setInfos.map[set];

      DxvkShaderBinding srcMapping(binding.getStageMask(), binding.getSet(), binding.getBinding());
      DxvkShaderBinding dstMapping(srcMapping);

      if (binding.usesDescriptor()) {
        uint32_t realSet = set + uint32_t(flags.test(DxvkPipelineLayoutFlag::UsesSamplerHeap));

        auto bindingIndex = setLayoutKeys[set].add(DxvkDescriptorSetLayoutBinding(binding));
        dstMapping = DxvkShaderBinding(binding.getStageMask(), realSet, bindingIndex);

        layout.bindingMap.addBinding(srcMapping, dstMapping);
        layout.setStateMasks[set] |= computeStateMask(binding);
      }

      if (binding.getDescriptorCount()) {
        if (binding.usesDescriptor()) {
          appendDescriptors(layout.setDescriptors[set], binding, dstMapping);

          if (binding.isUniformBuffer())
            appendDescriptors(layout.setUniformBuffers[set], binding, dstMapping);
          else
            appendDescriptors(layout.setResources[set], binding, dstMapping);
        } else {
          // Compute correct push data offset for the resource
          auto offset = layout.bindingMap.mapPushData(
            binding.getStageMask(), binding.getBlockOffset());

          if (offset == -1u)
            throw DxvkError(str::format("No push data mapping found for offset ", binding.getBlockOffset()));

          binding.setBlockOffset(offset);

          // This can be either a sampler or raw buffer address
          if (binding.getDescriptorType() == VK_DESCRIPTOR_TYPE_SAMPLER)
            appendDescriptors(layout.samplers, binding, dstMapping);
          else
            appendDescriptors(layout.vaBindings, binding, dstMapping);
        }
      }
    }

    // Remap sampler descriptor heap bindings
    if (flags.test(DxvkPipelineLayoutFlag::UsesSamplerHeap)) {
      DxvkShaderBinding dstMapping(builder.getStageMask(), 0u, 0u);

      for (uint32_t i = 0u; i < builder.getSamplerHeapBindingCount(); i++) {
        layout.bindingMap.addBinding(builder.getSamplerHeapBinding(i), dstMapping);
      }
    }

    // Create the actual descriptor set layout objects
    small_vector<const DxvkDescriptorSetLayout*, MaxSets> setLayouts(setInfos.count);

    for (uint32_t i = 0u; i < setInfos.count; i++) {
      if (setInfos.mask & (1u << i))
        setLayouts[i] = manager->createDescriptorSetLayout(setLayoutKeys[i]);
    }

    return setLayouts;
  }


  void DxvkPipelineBindings::buildMetadata(
    const DxvkPipelineLayoutBuilder&  builder) {
    auto bindings = builder.getBindings();

    for (size_t i = 0; i < bindings.bindingCount; i++) {
      auto binding = bindings.bindings[i];

      DxvkShaderBinding srcMapping(
        binding.getStageMask(),
        binding.getSet(),
        binding.getBinding());

      if (binding.getDescriptorType() == VK_DESCRIPTOR_TYPE_SAMPLER && binding.usesDescriptor())
        throw DxvkError("Sampler descriptor without push index found");

      if (binding.getDescriptorType() == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        throw DxvkError("Combined image/sampler descriptors not supported");

      if (binding.getDescriptorCount()) {
        if (binding.getDescriptorType() != VK_DESCRIPTOR_TYPE_SAMPLER) {
          if (binding.getAccess() & vk::AccessWriteMask) {
            appendDescriptors(m_readWriteResources, binding, srcMapping);

            if (binding.getAccessOp() == DxvkAccessOp::None)
              m_hazardousStageMask |= binding.getStageMask();
          }

          if (!(binding.getAccess() & vk::AccessWriteMask)) {
            for (auto stageIndex : bit::BitMask(uint32_t(binding.getStageMask())))
              appendDescriptors(m_readOnlyResources[stageIndex], binding, srcMapping);
          }
        }

        m_nonemptyStageMask |= binding.getStageMask();

        m_barrier.stages |= util::pipelineStages(binding.getStageMask());
        m_barrier.access |= binding.getAccess();

        m_descriptorCount += binding.getDescriptorCount();
      }
    }
  }


  DxvkPipelineLayoutFlags DxvkPipelineBindings::getPipelineLayoutFlags(
          DxvkPipelineLayoutType      type,
    const DxvkPipelineLayoutBuilder&  builder) {
    auto bindings = builder.getBindings();

    DxvkPipelineLayoutFlags result;

    if (type == DxvkPipelineLayoutType::Independent) {
      // Always need to assume that at least one stage uses samplers
      result.set(DxvkPipelineLayoutFlag::UsesSamplerHeap);
    } else {
      for (size_t i = 0; i < bindings.bindingCount; i++) {
        auto binding = bindings.bindings[i];

        if (binding.getDescriptorType() == VK_DESCRIPTOR_TYPE_SAMPLER && !binding.usesDescriptor()) {
          result.set(DxvkPipelineLayoutFlag::UsesSamplerHeap);
          break;
        }
      }
    }

    return result;
  }


  uint32_t DxvkPipelineBindings::computeStateMask(const DxvkShaderDescriptor& binding) {
    return DxvkDescriptorState::computeMask(binding.getStageMask(),
      binding.isUniformBuffer() ? DxvkDescriptorClass::Buffer : DxvkDescriptorClass::View);
  }


  uint32_t DxvkPipelineBindings::computeSetForBinding(
          DxvkPipelineLayoutType    type,
    const DxvkShaderDescriptor&     binding) {
    VkShaderStageFlags stage = binding.getStageMask();

    if (!binding.usesDescriptor())
      return -1u;

    if (stage == VK_SHADER_STAGE_COMPUTE_BIT)
      return DxvkDescriptorSets::CpResources;

    if (type == DxvkPipelineLayoutType::Independent) {
      return stage & VK_SHADER_STAGE_FRAGMENT_BIT
        ? DxvkDescriptorSets::GpIndependentFsResources
        : DxvkDescriptorSets::GpIndependentVsResources;
    }

    if (binding.isUniformBuffer())
      return DxvkDescriptorSets::GpBuffers;

    return DxvkDescriptorSets::GpViews;
  }


  DxvkPipelineBindings::SetInfos DxvkPipelineBindings::computeSetMaskAndCount(
          DxvkPipelineLayoutType          type,
          VkShaderStageFlags              stages,
          DxvkPipelineBindingRange        bindings) {
    SetInfos result = { };

    if (type == DxvkPipelineLayoutType::Independent) {
      // For independent layouts, we need to keep the set mapping consistent
      result.count = DxvkDescriptorSets::GpIndependentSetCount;

      if (stages & VK_SHADER_STAGE_FRAGMENT_BIT)
        result.mask |= 1u << DxvkDescriptorSets::GpIndependentFsResources;

      if (stages & VK_SHADER_STAGE_VERTEX_BIT)
        result.mask |= 1u << DxvkDescriptorSets::GpIndependentVsResources;

      for (uint32_t i = 0u; i < result.count; i++)
        result.map[i] = uint8_t(i);
    } else {
      // Iterate over bindings to check which sets are actively used, then
      // filter out any empty sets in order to reduce some overhead that
      // we may otherwise get when there are gaps in used sets.
      std::array<uint16_t, MaxSets> setSizes = { };

      for (size_t i = 0u; i < bindings.bindingCount; i++) {
        if (bindings.bindings[i].usesDescriptor()) {
          uint32_t set = computeSetForBinding(type, bindings.bindings[i]);
          setSizes[set] += bindings.bindings[i].getDescriptorCount();
        }
      }

      // Compute mapping from logical set to real set index
      for (size_t i = 0u; i < MaxSets; i++) {
        if (setSizes[i])
          result.map[i] = result.count++;
      }

      // Compute compact mask of all used sets
      result.mask = (1u << result.count) - 1u;
    }

    return result;
  }


  DxvkPipelineLayoutBuilder::DxvkPipelineLayoutBuilder() {

  }


  DxvkPipelineLayoutBuilder::DxvkPipelineLayoutBuilder(VkShaderStageFlags stageMask)
  : m_stageMask(stageMask) {

  }


  DxvkPipelineLayoutBuilder::~DxvkPipelineLayoutBuilder() {

  }


  void DxvkPipelineLayoutBuilder::addPushData(
          DxvkPushDataBlock         block) {
    uint32_t index = DxvkPushDataBlock::computeIndex(block.getStageMask());

    if (!block.isEmpty()) {
      m_pushMask |= 1u << index;
      m_pushData[index].merge(block);
    }
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


  void DxvkPipelineLayoutBuilder::addSamplerHeap(
    const DxvkShaderBinding&        binding) {
    m_samplerHeaps.push_back(binding);
  }


  void DxvkPipelineLayoutBuilder::addLayout(
    const DxvkPipelineLayoutBuilder& layout) {
    m_stageMask |= layout.m_stageMask;
    m_pushMask |= layout.m_pushMask;

    for (auto i : bit::BitMask(layout.getPushDataMask())) {
      auto srcBlock = layout.getPushDataBlock(i);

      if (m_pushData[i].isEmpty())
        m_pushData[i] = srcBlock;
      else
        m_pushData[i].merge(srcBlock);
    }

    addBindings(layout.m_bindings.size(), layout.m_bindings.data());

    for (uint32_t i = 0u; i < layout.getSamplerHeapBindingCount(); i++)
      addSamplerHeap(layout.getSamplerHeapBinding(i));
  }

}
