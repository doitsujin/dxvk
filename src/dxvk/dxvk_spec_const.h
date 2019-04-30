#pragma once

#include "dxvk_limits.h"
#include "dxvk_shader.h"

namespace dxvk {
  
  /**
   * \brief Specialization constant info
   * 
   * Accumulates specialization constant data for
   * constants that use non-default values.
   */
  class DxvkSpecConstants {

  public:

    DxvkSpecConstants();

    ~DxvkSpecConstants();

    /**
     * \brief Sets specialization constant value
     *
     * If the given value is different from the constant's
     * default value, this will store the new value and add
     * a map entry so that it gets applied properly. Each
     * constant may only be set once.
     * \param [in] specId Specialization constant ID
     * \param [in] value Specialization constant value
     * \param [in] defaultValue Default value
     */
    template<typename T>
    void set(uint32_t specId, T value, T defaultValue) {
      if (value != defaultValue)
        setAsUint32(specId, uint32_t(value));
    }

    /**
     * \brief Generates specialization info structure
     * \returns Specialization info for shader module
     */
    VkSpecializationInfo getSpecInfo() const;

  private:

    std::vector<uint32_t>                 m_data = { };
    std::vector<VkSpecializationMapEntry> m_map  = { };

    void setAsUint32(uint32_t specId, uint32_t value);

  };
  
}