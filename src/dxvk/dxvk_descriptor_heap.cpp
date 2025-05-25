#include <algorithm>

#include "dxvk_descriptor_heap.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkDescriptorUpdateList::DxvkDescriptorUpdateList(
          DxvkDevice*               device,
          uint32_t                  setSize,
          uint32_t                  descriptorCount,
    const DxvkDescriptorUpdateInfo* descriptorInfos)
  : m_device(device) {
    // Concatenate update infos with view indices (if any) and sort
    // by offset, so that we can more easily process the list.
    using Entry = std::pair<int32_t, DxvkDescriptorUpdateInfo>;

    std::vector<Entry> list;

    for (uint32_t i = 0u; i < descriptorCount; i++)
      list.push_back({ int32_t(i), descriptorInfos[i] });

    std::sort(list.begin(), list.end(), [] (const Entry& a, const Entry& b) {
      return a.second.offset < b.second.offset;
    });

    // Iterate over ranges and insert padding and copies as necessary,
    // while merging ranges as best we can. Skip buffers here as they
    // will be written separately with an API call.
    DxvkDescriptorUpdateRange range = { };
    VkDescriptorType rangeType = VK_DESCRIPTOR_TYPE_MAX_ENUM;

    for (const Entry& e : list) {
      const auto& index = e.first;
      const auto& info = e.second;

      // Merge consecutive ranges of the same descriptor type
      bool canMerge = info.descriptorType == rangeType &&
        uint32_t(index) == range.srcIndex + range.descriptorCount;

      if (canMerge && range.descriptorCount > 1u)
        canMerge = info.offset == uint32_t(range.dstOffset + range.descriptorCount * range.descriptorSize);

      // If there is padding between descriptors of the same type for
      // whatever reason, just increase the amount of data we copy.
      if (canMerge && range.descriptorCount == 1u)
        range.descriptorSize = info.offset - range.dstOffset;

      if (canMerge) {
        range.descriptorCount += 1u;
      } else {
        addCopy(range);
        addPadding(range.dstOffset + range.descriptorSize * range.descriptorCount, info.offset);

        range = { };
        range.dstOffset = info.offset;
        range.srcIndex = uint32_t(index);
        range.descriptorCount = 1u;
        range.descriptorSize = getDescriptorSize(info.descriptorType);

        rangeType = info.descriptorType;
      }
    }

    // Add final copy range and padding to ensure we fill entire cache
    // lines and do not accidentally read back memory during updates.
    addCopy(range);
    addPadding(range.dstOffset + range.descriptorSize * range.descriptorCount, setSize);
  }


  DxvkDescriptorUpdateList::~DxvkDescriptorUpdateList() {

  }


  void DxvkDescriptorUpdateList::addCopy(const DxvkDescriptorUpdateRange& range) {
    if (!range.descriptorCount)
      return;

    uint32_t offsetAlignment = range.dstOffset & -range.dstOffset;

    auto& entry = m_entries.emplace_back();
    entry.range = range;
    entry.fn = getCopyFn(offsetAlignment, range.descriptorSize);
  }


  void DxvkDescriptorUpdateList::addPadding(uint32_t loOffset, uint32_t hiOffset) {
    if (loOffset >= hiOffset)
      return;

    uint32_t offsetAlignment = hiOffset & -hiOffset;

    auto& entry = m_entries.emplace_back();
    entry.range.dstOffset = loOffset;
    entry.range.descriptorCount = 1u;
    entry.range.descriptorSize = hiOffset - loOffset;
    entry.fn = getPaddingFn(offsetAlignment, hiOffset - loOffset);
  }


  uint32_t DxvkDescriptorUpdateList::getDescriptorSize(VkDescriptorType type) const {
    return m_device->getDescriptorProperties().getDescriptorTypeInfo(type).size;
  }


  DxvkDescriptorUpdateFn* DxvkDescriptorUpdateList::getCopyFn(uint32_t alignment, uint32_t size) {
    // TODO pre-compile optimized variants
    return &copyGeneric;
  }


  DxvkDescriptorUpdateFn* DxvkDescriptorUpdateList::getPaddingFn(uint32_t alignment, uint32_t size) {
    // TODO pre-compile optimized variants
    return &padGeneric;
  }


  void DxvkDescriptorUpdateList::copyGeneric(
          void*                       dst,
    const DxvkDescriptor**            descriptor,
    const DxvkDescriptorUpdateRange&  range) {
    auto dstPtr = reinterpret_cast<char*>(dst);
    auto srcPtr = descriptor + range.srcIndex;

    for (uint32_t i = 0u; i < range.descriptorCount; i++) {
      std::memcpy(dstPtr, srcPtr[i]->descriptor.data(), range.descriptorSize);
      dstPtr += range.descriptorSize;
    }
  }


  void DxvkDescriptorUpdateList::padGeneric(
          void*                       dst,
    const DxvkDescriptor**            descriptor,
    const DxvkDescriptorUpdateRange&  range) {
    auto dstPtr = reinterpret_cast<char*>(dst);

    std::memset(dstPtr + range.dstOffset, 0, range.descriptorSize);
  }




  DxvkDescriptorProperties::DxvkDescriptorProperties(DxvkDevice* device) {
    if (device->canUseDescriptorBuffer())
      initDescriptorBufferProperties(device);
  }


  DxvkDescriptorProperties::~DxvkDescriptorProperties() {

  }


  void DxvkDescriptorProperties::initDescriptorBufferProperties(const DxvkDevice* device) {
    auto vk = device->vkd();
    auto properties = device->properties().extDescriptorBuffer;

    std::array<std::pair<VkDescriptorType, size_t>, 7u> sizes = {{
      { VK_DESCRIPTOR_TYPE_SAMPLER,               properties.samplerDescriptorSize                  },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,        properties.robustUniformBufferDescriptorSize      },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,        properties.robustStorageBufferDescriptorSize      },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,  properties.robustUniformTexelBufferDescriptorSize },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,  properties.robustStorageTexelBufferDescriptorSize },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,         properties.sampledImageDescriptorSize             },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,         properties.storageImageDescriptorSize             },
    }};

    for (const auto& s : sizes) {
      auto type = uint32_t(s.first);

      // We don't get alignments from this extension
      auto& info = m_descriptorTypes[type];
      info.size       = s.second;
      info.alignment  = 1u;

      if (s.first != VK_DESCRIPTOR_TYPE_SAMPLER) {
        VkDescriptorGetInfoEXT nullInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
        nullInfo.type = s.first;

        vk->vkGetDescriptorEXT(vk->device(),
          &nullInfo, s.second, m_nullDescriptors[type].descriptor.data());
      }
    }

    m_setAlignment = std::max<uint32_t>(CACHE_LINE_SIZE, properties.descriptorBufferOffsetAlignment);

    logDescriptorProperties();
  }


  void DxvkDescriptorProperties::logDescriptorProperties() {
    Logger::info(str::format(
      "Descriptor sizes (set alignment: ", m_setAlignment, ")",
      "\n  Sampler              : ", getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_SAMPLER).size,
      "\n  Uniform buffer       : ", getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER).size,
      "\n  Storage buffer       : ", getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER).size,
      "\n  Uniform texel buffer : ", getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER).size,
      "\n  Storage texel buffer : ", getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER).size,
      "\n  Sampled image        : ", getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE).size,
      "\n  Storage image        : ", getDescriptorTypeInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE).size));
  }

}
