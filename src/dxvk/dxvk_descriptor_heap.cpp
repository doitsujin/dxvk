#include "dxvk_descriptor_heap.h"
#include "dxvk_device.h"

namespace dxvk {

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
