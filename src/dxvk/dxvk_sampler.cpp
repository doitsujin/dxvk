#include "dxvk_buffer.h"
#include "dxvk_device.h"
#include "dxvk_format.h"
#include "dxvk_sampler.h"

namespace dxvk {
    
  DxvkSampler::DxvkSampler(
          DxvkSamplerPool*        pool,
    const DxvkSamplerKey&         key,
          uint16_t                index)
  : m_pool(pool), m_key(key) {
    auto vk = m_pool->m_device->vkd();

    auto formatInfo = lookupFormatInfo(VkFormat(key.u.p.viewFormat));

    // We generally want to preserve the border color as-is, and only apply the inverse
    // swizzle if the device applies the image view swizzle to border colors as well.
    VkSamplerBorderColorComponentMappingCreateInfoEXT borderColorSwizzle = { VK_STRUCTURE_TYPE_SAMPLER_BORDER_COLOR_COMPONENT_MAPPING_CREATE_INFO_EXT };
    borderColorSwizzle.components = { VkComponentSwizzle(key.u.p.viewSwizzleR), VkComponentSwizzle(key.u.p.viewSwizzleG),
                                      VkComponentSwizzle(key.u.p.viewSwizzleB), VkComponentSwizzle(key.u.p.viewSwizzleA) };
    borderColorSwizzle.srgb = formatInfo && formatInfo->flags.test(DxvkFormatFlag::ColorSpaceSrgb);

    VkSamplerCustomBorderColorCreateInfoEXT borderColorInfo = { VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT };
    borderColorInfo.customBorderColor = swizzleBorderColor(key.borderColor, borderColorSwizzle.components);

    if (!m_pool->m_device->features().extCustomBorderColor.customBorderColorWithoutFormat)
      borderColorInfo.format = VkFormat(key.u.p.viewFormat);

    VkSamplerReductionModeCreateInfo reductionInfo = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO };
    reductionInfo.reductionMode = VkSamplerReductionMode(key.u.p.reduction);

    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VkFilter(key.u.p.magFilter);
    samplerInfo.minFilter = VkFilter(key.u.p.minFilter);
    samplerInfo.mipmapMode = VkSamplerMipmapMode(key.u.p.mipMode);
    samplerInfo.addressModeU = VkSamplerAddressMode(key.u.p.addressU);
    samplerInfo.addressModeV = VkSamplerAddressMode(key.u.p.addressV);
    samplerInfo.addressModeW = VkSamplerAddressMode(key.u.p.addressW);
    samplerInfo.mipLodBias = bit::decodeFixed<int32_t, 6, 8>(key.u.p.lodBias);
    samplerInfo.anisotropyEnable = key.u.p.anisotropy > 0u;
    samplerInfo.maxAnisotropy = float(key.u.p.anisotropy);
    samplerInfo.compareEnable = key.u.p.compareEnable != 0u;
    samplerInfo.compareOp = VkCompareOp(key.u.p.compareOp);
    samplerInfo.minLod = bit::decodeFixed<uint32_t, 4, 8>(key.u.p.minLod);
    samplerInfo.maxLod = bit::decodeFixed<uint32_t, 4, 8>(key.u.p.maxLod);
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = key.u.p.pixelCoord;

    if (key.u.p.legacyCube && m_pool->m_device->features().extNonSeamlessCubeMap.nonSeamlessCubeMap)
      samplerInfo.flags |= VK_SAMPLER_CREATE_NON_SEAMLESS_CUBE_MAP_BIT_EXT;

    if (!m_pool->m_device->features().core.features.samplerAnisotropy)
      samplerInfo.anisotropyEnable = VK_FALSE;

    if (key.u.p.hasBorder) {
      samplerInfo.borderColor = determineBorderColorType(borderColorInfo);

      if (m_pool->m_device->features().extBorderColorSwizzle.borderColorSwizzle
       && !m_pool->m_device->features().extBorderColorSwizzle.borderColorSwizzleFromImage)
        borderColorSwizzle.pNext = std::exchange(samplerInfo.pNext, &borderColorSwizzle);
    }

    if (samplerInfo.borderColor == VK_BORDER_COLOR_FLOAT_CUSTOM_EXT
     || samplerInfo.borderColor == VK_BORDER_COLOR_INT_CUSTOM_EXT)
      borderColorInfo.pNext = std::exchange(samplerInfo.pNext, &borderColorInfo);

    if (reductionInfo.reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE)
      reductionInfo.pNext = std::exchange(samplerInfo.pNext, &reductionInfo);

    m_descriptor = m_pool->m_descriptorHeap.createSampler(index, &samplerInfo);
  }


  DxvkSampler::~DxvkSampler() {
    m_pool->m_descriptorHeap.freeSampler(m_descriptor);
  }


  void DxvkSampler::release() {
    m_pool->releaseSampler(m_descriptor.samplerIndex);
  }


  VkBorderColor DxvkSampler::determineBorderColorType(const VkSamplerCustomBorderColorCreateInfoEXT& info) const {
    static const std::array<std::pair<VkClearColorValue, VkBorderColor>, 4> s_borderColors = {{
      { { { 0.0f, 0.0f, 0.0f, 0.0f } }, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK },
      { { { 0.0f, 0.0f, 0.0f, 1.0f } }, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK },
      { { { 1.0f, 1.0f, 1.0f, 1.0f } }, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE },
    }};

    // Iterate over border colors and try to find an exact match
    uint32_t componentCount = m_key.u.p.compareEnable ? 1u : 4u;

    for (const auto& e : s_borderColors) {
      bool allEqual = true;

      for (uint32_t i = 0; i < componentCount; i++)
        allEqual &= info.customBorderColor.float32[i] == e.first.float32[i];

      if (allEqual)
        return e.second;
    }

    // If custom border colors are supported, use that
    if (m_pool->m_device->features().extCustomBorderColor.customBorderColors
     && (m_pool->m_device->features().extCustomBorderColor.customBorderColorWithoutFormat || info.format))
      return VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;

    // Otherwise, use the sum of absolute differences to find the
    // closest fallback value. Some D3D9 games may rely on this.
    Logger::warn("DXVK: Custom border colors not supported");

    VkBorderColor result = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    float minSad = -1.0f;

    for (const auto& e : s_borderColors) {
      float sad = 0.0f;

      for (uint32_t i = 0; i < componentCount; i++)
        sad += std::abs(info.customBorderColor.float32[i] - e.first.float32[i]);

      if (sad < minSad || minSad < 0.0f) {
        minSad = sad;
        result = e.second;
      }
    }

    return result;
  }


  VkClearColorValue DxvkSampler::swizzleBorderColor(const VkClearColorValue& color, VkComponentMapping mapping) {
    // Normalize component mapping for inverse look-up
    if (mapping.r == VK_COMPONENT_SWIZZLE_IDENTITY) mapping.r = VK_COMPONENT_SWIZZLE_R;
    if (mapping.g == VK_COMPONENT_SWIZZLE_IDENTITY) mapping.g = VK_COMPONENT_SWIZZLE_G;
    if (mapping.b == VK_COMPONENT_SWIZZLE_IDENTITY) mapping.b = VK_COMPONENT_SWIZZLE_B;
    if (mapping.a == VK_COMPONENT_SWIZZLE_IDENTITY) mapping.a = VK_COMPONENT_SWIZZLE_A;

    VkClearColorValue result = { };
    result.float32[0] = mapBorderColorComponent(color, mapping, VK_COMPONENT_SWIZZLE_R);
    result.float32[1] = mapBorderColorComponent(color, mapping, VK_COMPONENT_SWIZZLE_G);
    result.float32[2] = mapBorderColorComponent(color, mapping, VK_COMPONENT_SWIZZLE_B);
    result.float32[3] = mapBorderColorComponent(color, mapping, VK_COMPONENT_SWIZZLE_A);
    return result;
  }


  float DxvkSampler::mapBorderColorComponent(const VkClearColorValue& color, const VkComponentMapping& mapping, VkComponentSwizzle which) {
    // Apply inverse swizzle so that applying the view swizzle
    // returns the intended border color to the extent possible.
    if (mapping.r == which) return color.float32[0u];
    if (mapping.g == which) return color.float32[1u];
    if (mapping.b == which) return color.float32[2u];
    if (mapping.a == which) return color.float32[3u];

    // The border color component itself isn't used at all,
    // check whether it is mapped to a special value.
    VkComponentSwizzle swizzle = which;

    if (which == VK_COMPONENT_SWIZZLE_R) swizzle = mapping.r;
    if (which == VK_COMPONENT_SWIZZLE_G) swizzle = mapping.g;
    if (which == VK_COMPONENT_SWIZZLE_B) swizzle = mapping.b;
    if (which == VK_COMPONENT_SWIZZLE_A) swizzle = mapping.a;

    return swizzle == VK_COMPONENT_SWIZZLE_ONE ? 1.0f : 0.0f;
  }




  DxvkSamplerDescriptorHeap::DxvkSamplerDescriptorHeap(
          DxvkDevice*               device,
          uint32_t                  size)
  : m_device(device), m_descriptorCount(size) {
    initDescriptorLayout();

    if (device->canUseDescriptorBuffer())
      initDescriptorBuffer();
    else
      initDescriptorPool();
  }


  DxvkSamplerDescriptorHeap::~DxvkSamplerDescriptorHeap() {
    auto vk = m_device->vkd();

    vk->vkDestroyDescriptorPool(vk->device(), m_legacy.pool, nullptr);
    vk->vkDestroyDescriptorSetLayout(vk->device(), m_legacy.setLayout, nullptr);
  }


  DxvkSamplerDescriptorSet DxvkSamplerDescriptorHeap::getDescriptorSetInfo() const {
    DxvkSamplerDescriptorSet result = { };
    result.set = m_legacy.set;
    result.layout = m_legacy.setLayout;
    return result;
  }


  DxvkDescriptorHeapBindingInfo DxvkSamplerDescriptorHeap::getDescriptorHeapInfo() const {
    auto bufferInfo = m_heap.buffer->getSliceInfo();

    DxvkDescriptorHeapBindingInfo result = { };
    result.buffer = bufferInfo.buffer;
    result.gpuAddress = bufferInfo.gpuAddress;
    result.heapSize = m_heap.descriptorSize * m_descriptorCount;
    result.bufferSize = bufferInfo.size;
    return result;
  }


  DxvkSamplerDescriptor DxvkSamplerDescriptorHeap::createSampler(
          uint16_t              index,
    const VkSamplerCreateInfo*  createInfo) {
    auto vk = m_device->vkd();

    DxvkSamplerDescriptor descriptor = { };
    descriptor.samplerIndex = index;

    VkResult vr = vk->vkCreateSampler(vk->device(), createInfo, nullptr, &descriptor.samplerObject);

    if (vr)
      throw DxvkError(str::format("Failed to create sampler object: ", vr));

    if (m_heap.buffer) {
      VkDescriptorGetInfoEXT info = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
      info.type = VK_DESCRIPTOR_TYPE_SAMPLER;
      info.data.pSampler = &descriptor.samplerObject;

      vk->vkGetDescriptorEXT(vk->device(), &info, m_heap.descriptorSize,
        m_heap.buffer->mapPtr(m_heap.descriptorOffset + m_heap.descriptorSize * index));
    } else {
      VkDescriptorImageInfo samplerInfo = { };
      samplerInfo.sampler = descriptor.samplerObject;

      VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
      write.dstSet = m_legacy.set;
      write.dstArrayElement = index;
      write.descriptorCount = 1u;
      write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      write.pImageInfo = &samplerInfo;

      vk->vkUpdateDescriptorSets(vk->device(), 1u, &write, 0u, nullptr);
    }

    return descriptor;
  }


  void DxvkSamplerDescriptorHeap::freeSampler(
          DxvkSamplerDescriptor sampler) {
    auto vk = m_device->vkd();

    vk->vkDestroySampler(vk->device(), sampler.samplerObject, nullptr);
  }


  void DxvkSamplerDescriptorHeap::initDescriptorLayout() {
    auto vk = m_device->vkd();

    VkDescriptorSetLayoutBinding binding = { };
    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    binding.descriptorCount = m_descriptorCount;
    binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorBindingFlags bindingFlags = 0u;

    if (!m_device->canUseDescriptorBuffer()) {
      bindingFlags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                   |  VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
                   |  VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo layoutFlags = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
    layoutFlags.bindingCount = 1u;
    layoutFlags.pBindingFlags = &bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &layoutFlags };
    layoutInfo.flags = m_device->canUseDescriptorBuffer()
      ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT
      : VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = 1u;
    layoutInfo.pBindings = &binding;

    VkResult vr = vk->vkCreateDescriptorSetLayout(vk->device(), &layoutInfo, nullptr, &m_legacy.setLayout);

    if (vr)
      throw DxvkError(str::format("Failed to create sampler descriptor set layout: ", vr));
  }


  void DxvkSamplerDescriptorHeap::initDescriptorPool() {
    auto vk = m_device->vkd();

    VkDescriptorPoolSize poolSize = { };
    poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSize.descriptorCount = m_descriptorCount;

    VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 1u;
    poolInfo.poolSizeCount = 1u;
    poolInfo.pPoolSizes = &poolSize;

    VkResult vr = vk->vkCreateDescriptorPool(vk->device(), &poolInfo, nullptr, &m_legacy.pool);

    if (vr)
      throw DxvkError(str::format("Failed to create sampler pool: ", vr));

    VkDescriptorSetAllocateInfo setInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    setInfo.descriptorPool = m_legacy.pool;
    setInfo.descriptorSetCount = 1u;
    setInfo.pSetLayouts = &m_legacy.setLayout;

    if ((vr = vk->vkAllocateDescriptorSets(vk->device(), &setInfo, &m_legacy.set)))
      throw DxvkError(str::format("Failed to allocate sampler descriptor set: ", vr));
  }


  void DxvkSamplerDescriptorHeap::initDescriptorBuffer() {
    auto vk = m_device->vkd();

    DxvkBufferCreateInfo bufferInfo = { };
    bufferInfo.usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
                     | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.debugName = "Sampler heap";

    vk->vkGetDescriptorSetLayoutSizeEXT(vk->device(), m_legacy.setLayout, &bufferInfo.size);
    vk->vkGetDescriptorSetLayoutBindingOffsetEXT(vk->device(), m_legacy.setLayout, 0u, &m_heap.descriptorOffset);

    Logger::info(str::format("Creating sampler descriptor heap (", bufferInfo.size >> 10u, " kB)"));

    m_heap.buffer = m_device->createBuffer(bufferInfo,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    m_heap.descriptorSize = m_device->getDescriptorProperties().getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_SAMPLER).size;
  }




  DxvkSamplerPool::DxvkSamplerPool(DxvkDevice* device)
  : m_device(device), m_descriptorHeap(device, MaxSamplerCount) {
    // Set up LRU list as a sort-of free list to allocate fresh samplers
    m_lruHead = 0u;
    m_lruTail = MaxSamplerCount - 1u;

    for (uint32_t i = 0u; i < MaxSamplerCount; i++) {
      if (i)
        m_samplers[i].lruPrev = i - 1u;
      if (i + 1u < MaxSamplerCount)
        m_samplers[i].lruNext = i + 1u;
    }

    // Default sampler, implicitly used for null descriptors or when creating
    // additional samplers fails for any reason. Keep a persistent reference
    // so that this sampler does not accidentally get recycled.
    DxvkSamplerKey defaultKey;
    defaultKey.setFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR);
    defaultKey.setLodRange(-256.0f, 256.0f, 0.0f);
    defaultKey.setAddressModes(
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    defaultKey.setReduction(VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE);

    m_default = createSampler(defaultKey);
  }


  DxvkSamplerPool::~DxvkSamplerPool() {

  }


  Rc<DxvkSampler> DxvkSamplerPool::createSampler(const DxvkSamplerKey& key) {
    std::unique_lock lock(m_mutex);
    auto entry = m_samplerLut.find(key);

    if (entry != m_samplerLut.end()) {
      auto& sampler = m_samplers.at(entry->second);

      // Remove the sampler from the LRU list if it's in there. Due
      // to the way releasing samplers is implemented upon reaching
      // a ref count of 0, it is possible that we reach this before
      // the releasing thread inserted the list into the LRU list.
      if (!sampler.object->m_refCount.fetch_add(1u, std::memory_order_acquire)) {
        removeLru(sampler, entry->second);

        m_samplersLive.store(m_samplersLive.load() + 1u, std::memory_order_relaxed);
      }

      // We already took a reference, forward the pointer as-is
      return Rc<DxvkSampler>::unsafeCreate(&sampler.object.value());
    }

    // If there are no samplers we can allocate, fall back to the default
    if (m_lruHead < 0) {
      Logger::err("Failed to allocate sampler, using default one.");
      return m_default;
    }

    // Use the least recently used sampler entry. This may be a previously
    // unused sampler, or an object that has not yet been initialized.
    int32_t samplerIndex = m_lruHead;

    // Destroy existing sampler and remove the corresponding LUT entry
    auto& sampler = m_samplers.at(samplerIndex);

    if (sampler.object) {
      m_samplerLut.erase(sampler.object->key());
      sampler.object.reset();
    }

    removeLru(sampler, samplerIndex);

    // Create new sampler object and set up the corresponding LUT entry
    sampler.object.emplace(this, key, uint16_t(samplerIndex));
    m_samplerLut.insert_or_assign(key, samplerIndex);

    // Update statistics
    m_samplersLive.store(m_samplersLive.load() + 1u, std::memory_order_relaxed);
    return &sampler.object.value();
  }


  void DxvkSamplerPool::releaseSampler(int32_t index) {
    std::unique_lock lock(m_mutex);

    // Always decrement live counter here since it will be incremented
    // again whenever the sampler is reacquired.
    m_samplersLive.store(m_samplersLive.load() - 1u);

    // Back off if another thread has re-aquired the sampler. This is
    // safe since the ref count can only be incremented from zero when
    // the pool is locked.
    auto& sampler = m_samplers.at(index);

    if (sampler.object->m_refCount.load(std::memory_order_relaxed))
      return;

    // It is also possible that two threads end up here while the ref
    // count is zero. Make sure to not add the sampler to the LRU list
    // more than once in that case.
    if (samplerIsInLruList(sampler, index))
      return;

    // Add sampler to the end of the LRU list, but keep the sampler
    // object itself as well as the look-up table entry intact in
    // case the app wants to recreate the same sampler later.
    appendLru(sampler, index);
  }


  void DxvkSamplerPool::appendLru(SamplerEntry& sampler, int32_t index) {
    sampler.lruPrev = m_lruTail;
    sampler.lruNext = -1;

    if (m_lruTail >= 0)
      m_samplers.at(m_lruTail).lruNext = index;
    else
      m_lruHead = index;

    m_lruTail = index;
  }


  void DxvkSamplerPool::removeLru(SamplerEntry& sampler, int32_t index) {
    if (sampler.lruPrev >= 0)
      m_samplers.at(sampler.lruPrev).lruNext = sampler.lruNext;
    else if (m_lruHead == index)
      m_lruHead = sampler.lruNext;

    if (sampler.lruNext >= 0)
      m_samplers.at(sampler.lruNext).lruPrev = sampler.lruPrev;
    else if (m_lruTail == index)
      m_lruTail = sampler.lruPrev;

    sampler.lruPrev = -1;
    sampler.lruNext = -1;
  }


  bool DxvkSamplerPool::samplerIsInLruList(SamplerEntry& sampler, int32_t index) const {
    return sampler.lruPrev >= 0 || m_lruHead == index;
  }

}
