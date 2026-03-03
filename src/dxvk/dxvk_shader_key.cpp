#include "dxvk_shader_key.h"

namespace dxvk {

  DxvkShaderHash::DxvkShaderHash() {

  }


  DxvkShaderHash::DxvkShaderHash(
          VkShaderStageFlagBits stage,
          uint32_t              codeSize,
    const uint8_t*              hash,
          size_t                hashSize)
  : DxvkShaderHash(stage, codeSize, hash, hashSize, nullptr, 0u) { }


  DxvkShaderHash::DxvkShaderHash(
          VkShaderStageFlagBits stage,
          uint32_t              codeSize,
    const uint8_t*              hash,
          size_t                hashSize,
    const uint8_t*              metaHash,
          size_t                metaSize)
  : m_stage(uint16_t(stage)), m_xfb(metaSize ? 1u : 0u), m_size(codeSize) {
    size_t index = 0u;

    for (size_t i = 0u; i < hashSize; i += 4u) {
      m_hash[index] ^= getDword(&hash[i]);
      index = (index + 1u) % m_hash.size();
    }

    for (size_t i = 0u; i < metaSize; i += 4u) {
      m_hash[index] ^= getDword(&metaHash[i]);
      index = (index + 1u) % m_hash.size();
    }
  }


  std::string DxvkShaderHash::toString() const {
    std::string name;
    name.reserve(48u);

    if (m_xfb) {
      name = "xfb";
    } else {
      switch (m_stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: name = "vs"; break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: name = "tcs"; break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: name = "tes"; break;
        case VK_SHADER_STAGE_GEOMETRY_BIT: name = "gs"; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: name = "fs"; break;
        case VK_SHADER_STAGE_COMPUTE_BIT: name = "cs"; break;
        default: name = "shdr";
      }
    }

    name += ".";

    for (size_t i = 0u; i < m_hash.size(); i++) {
      for (uint32_t j = 0u; j < 4u; j++) {
        name += toHex(m_hash[i] >> (8u * j + 4u));
        name += toHex(m_hash[i] >> (8u * j));
      }
    }

    return name;
  }


  bool DxvkShaderHash::eq(const DxvkShaderHash& other) const {
    bool eq = m_stage == other.m_stage
           && m_xfb == other.m_xfb
           && m_size == other.m_size;

    for (size_t i = 0u; i < m_hash.size(); i++)
      eq = eq && m_hash[i] == other.m_hash[i];

    return eq;
  }


  size_t DxvkShaderHash::hash() const {
    DxvkHashState hash = { };
    hash.add(m_stage);
    hash.add(m_xfb);
    hash.add(m_size);

    for (auto dw : m_hash)
      hash.add(dw);

    return hash;
  }


  size_t DxvkShaderHash::getDword(const uint8_t* dw) {
    return  (uint32_t(dw[0u]))
          | (uint32_t(dw[1u]) << 8u)
          | (uint32_t(dw[2u]) << 16u)
          | (uint32_t(dw[3u]) << 24u);
  }


  char DxvkShaderHash::toHex(uint8_t nibble) {
    static const std::array<char, 16u> ch = {
      '0', '1', '2', '3', '4', '5', '6', '7',
      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };

    return ch[nibble & 0xfu];
  }

}
