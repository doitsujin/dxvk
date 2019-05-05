#pragma once

#include "dxvk_limits.h"
#include "dxvk_shader.h"

namespace dxvk {

  /**
   * \briefS Specialization constant entry
   * 
   * Used to pass a list of user-defined
   * specialization constants to shaders.
   */
  struct DxvkSpecConstant {
    uint32_t specId;
    uint32_t value;
  };

  
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
     * \brief Sets specialization constant value
     *
     * Always passes the constant value to the driver.
     * \param [in] specId Specialization constant ID
     * \param [in] value Specialization constant value
     */
    template<typename T>
    void set(uint32_t specId, T value) {
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


  /**
   * \brief Computes specialization constant ID
   * 
   * Computest the specId to use within shaders
   * for a given pipeline specialization constant.
   * \param [in] index Spec constant index
   * \returns Specialization constant ID
   */
  inline uint32_t getSpecId(uint32_t index) {
    return uint32_t(DxvkSpecConstantId::FirstPipelineConstant) + index;
  }
  
}