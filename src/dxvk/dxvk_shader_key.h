#pragma once

#include "dxvk_hash.h"
#include "dxvk_include.h"

namespace dxvk {

  /**
   * \brief Shader look-up key
   *
   * Stores the shader hash itself, as well as some basic
   * metadata like the shader type or hashed xfb metadata.
   */
  class DxvkShaderHash {

  public:

    DxvkShaderHash();

    /**
     * \brief Initializes shader hash
     *
     * \param [in] stage Shader stage
     * \param [in] codeSize Shader code size, in bytes
     * \param [in] hash Pointer to shader hash
     * \param [in] hashSize Shader hash size, in bytes
     */
    DxvkShaderHash(
            VkShaderStageFlagBits stage,
            uint32_t              codeSize,
      const uint8_t*              hash,
            size_t                hashSize);

    /**
     * \brief Initializes shader hash
     *
     * \param [in] stage Shader stage
     * \param [in] codeSize Shader code size, in bytes
     * \param [in] hash Pointer to shader hash
     * \param [in] hashSize Shader hash size, in bytes
     * \param [in] metaHash Metadata hash
     * \param [in] metaSize Metadata hash size, in bytes
     */
    DxvkShaderHash(
            VkShaderStageFlagBits stage,
            uint32_t              codeSize,
      const uint8_t*              hash,
            size_t                hashSize,
      const uint8_t*              metaHash,
            size_t                metaSize);

    /**
     * \brief Shader stage
     * \returns Shader stage
     */
    VkShaderStageFlagBits stage() const {
      return VkShaderStageFlagBits(m_stage);
    }

    /**
     * \brief Whether shader was created using streamout metadata
     * \returns \c true if the shader has transform feedback metadata
     */
    bool hasXfb() const {
      return m_xfb;
    }

    /**
     * \brief Generates shader name for the given hash
     * \returns Shader name to use for tooling etc.
     */
    std::string toString() const;

    /**
     * \brief Compares two shader hashes
     *
     * \param [in] other Other hash to compare to
     * \returns \c true if the hashes are equal
     */
    bool eq(const DxvkShaderHash& other) const;

    /**
     * \brief Computes look-up hash for shader
     * \returns Hash for internal hash tables
     */
    size_t hash() const;

  private:

    uint16_t                  m_stage = -1;
    uint16_t                  m_xfb   = 0u;
    uint32_t                  m_size  = 0u;
    std::array<uint32_t, 4u>  m_hash  = { };

    static size_t getDword(const uint8_t* dw);

    static char toHex(uint8_t nibble);

  };

}
