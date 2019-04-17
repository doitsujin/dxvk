#pragma once

#include "dxbc_common.h"
#include "dxbc_enums.h"

namespace dxvk {

  /**
   * \brief Binding numbers and properties
   */
  enum DxbcBindingProperties : uint32_t {
    DxbcConstBufBindingIndex  = 0,
    DxbcConstBufBindingCount  = 16,
    DxbcSamplerBindingIndex   = DxbcConstBufBindingIndex
                              + DxbcConstBufBindingCount,
    DxbcSamplerBindingCount   = 16,
    DxbcResourceBindingIndex  = DxbcSamplerBindingIndex
                              + DxbcSamplerBindingCount,
    DxbcResourceBindingCount  = 128,
    DxbcStageBindingCount     = DxbcConstBufBindingCount
                              + DxbcSamplerBindingCount
                              + DxbcResourceBindingCount,
    DxbcUavBindingIndex       = DxbcStageBindingCount * 6,
    DxbcUavBindingCount       = 64,
  };

  
  /**
   * \brief Computes first binding index for a given stage
   *
   * \param [in] stage The shader stage
   * \returns Index of first binding
   */
  inline uint32_t computeStageBindingOffset(DxbcProgramType stage) {
    return DxbcStageBindingCount * uint32_t(stage);
  }


  /**
   * \brief Computes first UAV binding index offset for a given stage
   *
   * \param [in] stage The shader stage
   * \returns Index of first UAV binding
   */
  inline uint32_t computeStageUavBindingOffset(DxbcProgramType stage) {
    return DxbcUavBindingIndex
         + DxbcUavBindingCount * (stage == DxbcProgramType::ComputeShader ? 2 : 0);
  }


  /**
   * \brief Computes constant buffer binding index
   * 
   * \param [in] stage Shader stage
   * \param [in] index Constant buffer index
   * \returns Binding index
   */
  inline uint32_t computeConstantBufferBinding(DxbcProgramType stage, uint32_t index) {
    return computeStageBindingOffset(stage) + DxbcConstBufBindingIndex + index;
  }


  /**
   * \brief Computes sampler binding index
   * 
   * \param [in] stage Shader stage
   * \param [in] index Sampler index
   * \returns Binding index
   */
  inline uint32_t computeSamplerBinding(DxbcProgramType stage, uint32_t index) {
    return computeStageBindingOffset(stage) + DxbcSamplerBindingIndex + index;
  }


  /**
   * \brief Computes resource binding index
   * 
   * \param [in] stage Shader stage
   * \param [in] index Resource index
   * \returns Binding index
   */
  inline uint32_t computeSrvBinding(DxbcProgramType stage, uint32_t index) {
    return computeStageBindingOffset(stage) + DxbcResourceBindingIndex + index;
  }


  /**
   * \brief Computes UAV binding offset
   *
   * \param [in] stage Shader stage
   * \param [in] index UAV index
   * \returns Binding index
   */
  inline uint32_t computeUavBinding(DxbcProgramType stage, uint32_t index) {
    return computeStageUavBindingOffset(stage) + index;
  }
  
  
  /**
   * \brief Computes UAV counter binding offset
   *
   * \param [in] stage Shader stage
   * \param [in] index UAV index
   * \returns Binding index
   */
  inline uint32_t computeUavCounterBinding(DxbcProgramType stage, uint32_t index) {
    return computeStageUavBindingOffset(stage) + DxbcUavBindingCount + index;
  }
  
  /**
   * \brief Primitive vertex count
   * 
   * Calculates the number of vertices
   * for a given primitive type.
   */
  uint32_t primitiveVertexCount(
          DxbcPrimitive   primitive);
  
}