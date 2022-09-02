#pragma once

#include "d3d9_include.h"
#include "../dxvk/dxvk_device.h"

namespace dxvk {

  class D3D9DepthBias {

  public:
    D3D9DepthBias(const Rc<DxvkDevice>& Device);

    float GetFactor(VkFormat Format);
    void DetermineFactors();

  private:
    uint32_t GetFormatIndex(VkFormat Format);
    uint32_t DetermineFixedFactor(VkFormat Format);

    std::array<float, 5> m_depthBiasFactors;

    Rc<DxvkDevice> m_device;
    Rc<DxvkContext> m_context;
    Rc<DxvkBuffer> m_readbackBuffer;
    Rc<DxvkShader> m_vertexShader;
  };

}
