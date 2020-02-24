#pragma once

#include "d3d9_include.h"
#include "d3d9_format.h"
#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_context.h"

namespace dxvk {

  class D3D9FormatHelper {

  public:

    D3D9FormatHelper(const Rc<DxvkDevice>& device);

    void ConvertFormat(
            D3D9_CONVERSION_FORMAT_INFO   conversionFormat,
      const Rc<DxvkImage>&                dstImage,
            VkImageSubresourceLayers      dstSubresource,
      const Rc<DxvkBuffer>&               srcBuffer);

  private:

    void ConvertGenericFormat(
            D3D9_CONVERSION_FORMAT_INFO   videoFormat,
      const Rc<DxvkImage>&                dstImage,
            VkImageSubresourceLayers      dstSubresource,
      const Rc<DxvkBuffer>&               srcBuffer,
            VkFormat                      bufferFormat);

    void ConvertVideoFormat(
            D3D9_CONVERSION_FORMAT_INFO   videoFormat,
      const Rc<DxvkImage>&                dstImage,
            VkImageSubresourceLayers      dstSubresource,
      const Rc<DxvkBuffer>&               srcBuffer);

    enum BindingIds : uint32_t {
      Image  = 0,
      Buffer = 1,
    };

    void InitShaders();

    Rc<DxvkShader> InitShader(SpirvCodeBuffer code);

    Rc<DxvkDevice>    m_device;
    Rc<DxvkContext>   m_context;

    std::array<Rc<DxvkShader>, D3D9ConversionFormat_Count> m_shaders;

  };
  
}