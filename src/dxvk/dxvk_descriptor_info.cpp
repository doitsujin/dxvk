#include <algorithm>

#include "dxvk_descriptor_info.h"
#include "dxvk_device.h"

#include "../util/util_bit.h"

namespace dxvk {

  template<size_t Size>
  static force_inline void copy_nontemporal(void* dst, const void* src) {
    static_assert(Size == 4u || Size == 8u || Size == 16u);

    #if defined(DXVK_ARCH_X86) && (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
    switch (Size) {
      case 4u: {
        auto dstPtr = reinterpret_cast<      int*>(dst);
        auto srcPtr = reinterpret_cast<const int*>(src);
        _mm_stream_si32(dstPtr, srcPtr[0u]);
      } break;

      case 8u: {
        #if defined(DXVK_ARCH_X86_64)
        auto dstPtr = reinterpret_cast<      long long int*>(dst);
        auto srcPtr = reinterpret_cast<const long long int*>(src);
        _mm_stream_si64(dstPtr, srcPtr[0u]);
        #else
        auto dstPtr = reinterpret_cast<      int32_t*>(dst);
        auto srcPtr = reinterpret_cast<const int32_t*>(src);
        _mm_stream_si32(dstPtr + 0u, srcPtr[0u]);
        _mm_stream_si32(dstPtr + 1u, srcPtr[1u]);
        #endif
      } break;

      case 16u: {
        auto dstPtr = reinterpret_cast<      __m128i*>(dst);
        auto srcPtr = reinterpret_cast<const __m128i*>(src);
        _mm_stream_si128(dstPtr, _mm_loadu_si128(srcPtr));
      } break;
    }
    #else
    std::memcpy(dst, src, Size);
    #endif
  }


  template<size_t Size>
  static force_inline void clear_nontemporal(void* dst) {
    static_assert(Size == 4u || Size == 8u || Size == 16u);

    #if defined(DXVK_ARCH_X86) && (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
    switch (Size) {
      case 4u: {
        auto dstPtr = reinterpret_cast<int*>(dst);
        _mm_stream_si32(dstPtr, 0);
      } break;

      case 8u: {
        #if defined(DXVK_ARCH_X86_64)
        auto dstPtr = reinterpret_cast<long long int*>(dst);
        _mm_stream_si64(dstPtr, 0l);
        #else
        auto dstPtr = reinterpret_cast<int*>(dst);
        _mm_stream_si32(dstPtr + 0u, 0);
        _mm_stream_si32(dstPtr + 1u, 0);
        #endif
      } break;

      case 16u: {
        auto dstPtr = reinterpret_cast<__m128i*>(dst);
        _mm_stream_si128(dstPtr, _mm_setzero_si128());
      } break;
    }
    #else
    std::memset(dst, 0, Size);
    #endif
  }


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
    if (alignment >= 16u || alignment >= size || !alignment) {
      switch (size) {
        case   4u: return &copyAligned< 4u>;
        case   8u: return &copyAligned< 8u>;
        case  16u: return &copyAligned<16u>;
        case  24u: return &copyAligned<24u>;
        case  32u: return &copyAligned<32u>;
        case  48u: return &copyAligned<48u>;
        case  64u: return &copyAligned<64u>;
        case  96u: return &copyAligned<96u>;
        case 128u: return &copyAligned<128u>;
        case 160u: return &copyAligned<160u>;
        case 192u: return &copyAligned<192u>;
        case 224u: return &copyAligned<224u>;
        case 256u: return &copyAligned<256u>;
      }
    }

    return &copyGeneric;
  }


  DxvkDescriptorUpdateFn* DxvkDescriptorUpdateList::getPaddingFn(uint32_t alignment, uint32_t size) {
    if (alignment >= 16u || alignment >= size) {
      switch (size) {
        case  4u: return &padAligned< 4u>;
        case  8u: return &padAligned< 8u>;
        case 12u: return &padAligned<12u>;
        case 16u: return &padAligned<16u>;
        case 24u: return &padAligned<24u>;
        case 32u: return &padAligned<32u>;
        case 40u: return &padAligned<40u>;
        case 48u: return &padAligned<48u>;
        case 56u: return &padAligned<56u>;
        case 64u: return &padAligned<64u>;
        default: return &padAlignedAnySize;
      }
    }

    return &padGeneric;
  }


  void DxvkDescriptorUpdateList::copyGeneric(
          void*                       dst,
    const DxvkDescriptor**            descriptor,
    const DxvkDescriptorUpdateRange&  range) {
    auto dstPtr = reinterpret_cast<char*>(dst) + range.dstOffset;
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
    auto dstPtr = reinterpret_cast<char*>(dst) + range.dstOffset;

    std::memset(dstPtr, 0, range.descriptorSize);
  }


  template<size_t Size>
  void DxvkDescriptorUpdateList::copyAligned(
          void*                       dst,
    const DxvkDescriptor**            descriptor,
    const DxvkDescriptorUpdateRange&  range) {
    auto dstPtr = reinterpret_cast<char*>(dst) + range.dstOffset;
    auto srcBase = descriptor + range.srcIndex;

    for (uint32_t i = 0u; i < range.descriptorCount; i++) {
      auto srcPtr = reinterpret_cast<const char*>(srcBase[i]->descriptor.data());

      for (size_t i = 0u; i < Size / 16u; i++)
        copy_nontemporal<16u>(dstPtr + 16u * i, srcPtr + 16u * i);

      dstPtr += 16u * (Size / 16u);
      srcPtr += 16u * (Size / 16u);

      if (Size & 8u) {
        copy_nontemporal<8u>(dstPtr, srcPtr);

        dstPtr += 8u;
        srcPtr += 8u;
      }

      if (Size & 4u) {
        copy_nontemporal<4u>(dstPtr, srcPtr);

        dstPtr += 4u;
      }
    }
  }


  template<size_t Size>
  void DxvkDescriptorUpdateList::padAligned(
          void*                       dst,
    const DxvkDescriptor**            descriptor,
    const DxvkDescriptorUpdateRange&  range) {
    auto dstPtr = reinterpret_cast<char*>(dst) + range.dstOffset;

    if (Size & 4u) {
      clear_nontemporal<4u>(dstPtr);
      dstPtr += 4u;
    }

    if (Size & 8u) {
      clear_nontemporal<8u>(dstPtr);
      dstPtr += 8u;
    }

    for (size_t i = 0u; i < Size / 16u; i++)
      clear_nontemporal<16u>(dstPtr + 16u * i);
  }


  void DxvkDescriptorUpdateList::padAlignedAnySize(
          void*                       dst,
    const DxvkDescriptor**            descriptor,
    const DxvkDescriptorUpdateRange&  range) {
    auto dstPtr = reinterpret_cast<char*>(dst) + range.dstOffset;

    if (range.descriptorSize & 4u) {
      clear_nontemporal<4u>(dstPtr);
      dstPtr += 4u;
    }

    if (range.descriptorSize & 8u) {
      clear_nontemporal<8u>(dstPtr);
      dstPtr += 8u;
    }

    for (size_t i = 0u; i < range.descriptorSize / 16u; i++)
      clear_nontemporal<16u>(dstPtr + 16u * i);
  }




  DxvkDescriptorProperties::DxvkDescriptorProperties(DxvkDevice* device) {
    if (device->canUseDescriptorHeap())
      initDescriptorHeapProperties(device);
    else if (device->canUseDescriptorBuffer())
      initDescriptorBufferProperties(device);
  }


  DxvkDescriptorProperties::~DxvkDescriptorProperties() {

  }


  void DxvkDescriptorProperties::initDescriptorHeapProperties(const DxvkDevice* device) {
    auto vkd = device->vkd();
    auto vki = device->adapter()->vki();

    // Query tight descriptor sizes for each type, but pad them out to the required
    // alignment since we have no use for the memory in between descriptors. This
    // may still be useful on devicesw here raw buffer descriptors are smaller than
    // texel buffer descriptors.
    auto properties = device->properties().extDescriptorHeap;

    std::array<std::pair<VkDescriptorType, VkDeviceSize>, 7> types = {{
      { VK_DESCRIPTOR_TYPE_SAMPLER,              properties.samplerDescriptorAlignment },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,       properties.bufferDescriptorAlignment  },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       properties.bufferDescriptorAlignment  },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, properties.imageDescriptorAlignment   },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, properties.imageDescriptorAlignment   },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        properties.imageDescriptorAlignment   },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,        properties.imageDescriptorAlignment   },
    }};

    for (const auto& s : types) {
      auto type = uint32_t(s.first);

      VkDeviceSize size = vki->vkGetPhysicalDeviceDescriptorSizeEXT(device->adapter()->handle(), s.first);
      VkDeviceSize alignment = s.second;

      auto& info = m_descriptorTypes[type];
      info.size       = align(size, alignment);
      info.alignment  = alignment;

      m_setAlignment = std::max(m_setAlignment, alignment);

      if (s.first != VK_DESCRIPTOR_TYPE_SAMPLER) {
        VkResourceDescriptorInfoEXT nullInfo = { VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT };
        nullInfo.type = s.first;

        VkHostAddressRangeEXT nullData = m_nullDescriptors[type].getHostAddressRange();
        vkd->vkWriteResourceDescriptorsEXT(vkd->device(), 1u, &nullInfo, &nullData);
      }
    }

    // Pad to full cache lines for better write patterns
    m_setAlignment = std::max<VkDeviceSize>(m_setAlignment, CACHE_LINE_SIZE);

    logDescriptorProperties();
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
