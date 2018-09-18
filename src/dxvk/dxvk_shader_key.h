#pragma once

#include "dxvk_hash.h"
#include "dxvk_include.h"

namespace dxvk {

  /**
   * \brief Shader key
   * 
   * Provides a unique key that can be used
   * to look up a specific shader within a
   * structure. This consists of the shader
   * stage and the source hash, which should
   * be generated from the original code.
   */
  class DxvkShaderKey {

  public:

    /**
     * \brief Creates default shader key
     */
    DxvkShaderKey();

    /**
     * \brief Creates shader key
     * 
     * \param [in] shaderStage Shader stage
     * \param [in] sourceCode Shader source
     * \param [in] sourceSize Source length
     */
    DxvkShaderKey(
            VkShaderStageFlagBits shaderStage,
      const void*                 sourceCode,
            size_t                sourceSize);
    
    /**
     * \brief Generates string from shader key
     * \returns String representation of the key
     */
    std::string toString() const;
    
    /**
     * \brief Computes lookup hash
     * \returns Lookup hash
     */
    size_t hash() const;

    /**
     * \brief Checks whether two keys are equal
     * 
     * \param [in] key The shader key to compare to
     * \returns \c true if the two keys are equal
     */
    bool eq(const DxvkShaderKey& key) const;
    
  private:

    VkShaderStageFlags  m_type;
    Sha1Hash            m_sha1;

  };

}