#pragma once

#include "dxbc_common.h"
#include "dxbc_enums.h"

namespace dxvk {
  
  /**
   * \brief Resource type
   * 
   * The type of a shader resource. Used
   * to determine the DXVK resource slot.
   */
  enum DxbcBindingType : uint32_t {
    ConstantBuffer      = 0,
    ShaderResource      = 1,
    ImageSampler        = 2,
    UnorderedAccessView = 3,
    StreamOutputBuffer  = 4,
    UavCounter          = 5,
  };
  
  
  /**
   * \brief Computes the DXVK resource slot for a binding
   * 
   * \param [in] shaderStage The target shader stage
   * \param [in] bindingType Type of the resource
   * \param [in] bindingIndex Resource binding index
   * \returns DXVK resource slot index
   */
  uint32_t computeResourceSlotId(
          DxbcProgramType shaderStage,
          DxbcBindingType bindingType,
          uint32_t        bindingIndex);
  
  /**
   * \brief Primitive vertex count
   * 
   * Calculates the number of vertices
   * for a given primitive type.
   */
  uint32_t primitiveVertexCount(
          DxbcPrimitive   primitive);
  
}