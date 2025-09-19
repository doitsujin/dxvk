#pragma once

#include "dxbc_common.h"
#include "dxbc_enums.h"

#include "../d3d11/d3d11_util.h"

namespace dxvk {

  /**
   * \brief Push constant struct
   */
  struct DxbcPushConstants {
    uint32_t rasterizerSampleCount;
  };


  /**
   * \brief Binding numbers and properties
   */
  enum DxbcBindingProperties : uint32_t {
    DxbcConstantBuffersPerStage = 16u,
    DxbcSamplersPerStage        = 16u,

    DxbcSrvPerStage             = 128u,
    DxbcSrvTotal                = DxbcSrvPerStage * 6u,

    DxbcUavPerPipeline          = 64u,
    DxbcUavTotal                = DxbcUavPerPipeline * 4u,

    DxbcUavIndexGraphics        = DxbcSrvTotal,
    DxbcUavIndexCompute         = DxbcUavIndexGraphics + DxbcUavPerPipeline * 2u,

    DxbcGlobalSamplerSet        = 15u,
  };


  /**
   * \brief Computes constant buffer binding index
   * 
   * \param [in] stage Shader stage
   * \param [in] index Constant buffer index
   * \returns Binding index
   */
  inline uint32_t computeConstantBufferBinding(DxbcProgramType stage, uint32_t index) {
    return uint32_t(stage) * DxbcConstantBuffersPerStage + index;
  }


  /**
   * \brief Computes sampler binding index
   * 
   * \param [in] stage Shader stage
   * \param [in] index Sampler index
   * \returns Binding index
   */
  inline uint32_t computeSamplerBinding(DxbcProgramType stage, uint32_t index) {
    return uint32_t(stage) * DxbcSamplersPerStage + index;
  }


  /**
   * \brief Computes resource binding index
   * 
   * \param [in] stage Shader stage
   * \param [in] index Resource index
   * \returns Binding index
   */
  inline uint32_t computeSrvBinding(DxbcProgramType stage, uint32_t index) {
    return uint32_t(stage) * DxbcSrvPerStage + index;
  }


  /**
   * \brief Computes UAV binding offset
   *
   * \param [in] stage Shader stage
   * \param [in] index UAV index
   * \returns Binding index
   */
  inline uint32_t computeUavBinding(DxbcProgramType stage, uint32_t index) {
    return (stage == DxbcProgramType::ComputeShader ? DxbcUavIndexCompute : DxbcUavIndexGraphics) + index;
  }
  
  
  /**
   * \brief Computes UAV counter binding offset
   *
   * \param [in] stage Shader stage
   * \param [in] index UAV index
   * \returns Binding index
   */
  inline uint32_t computeUavCounterBinding(DxbcProgramType stage, uint32_t index) {
    return computeUavBinding(stage, index) + DxbcUavPerPipeline;
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
