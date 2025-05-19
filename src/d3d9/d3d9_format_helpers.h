#pragma once

#include "d3d9_include.h"
#include "d3d9_format.h"
#include "../dxvk/dxvk_device.h"
#include "../dxvk/dxvk_context.h"

namespace dxvk {

  class D3D9FormatHelper {

  public:

    D3D9FormatHelper(const Rc<DxvkDevice>& device);

    ~D3D9FormatHelper();

    void Flush();

    void ConvertFormat(
      const Rc<DxvkCommandList>&          ctx,
            D3D9_CONVERSION_FORMAT_INFO   conversionFormat,
      const Rc<DxvkImage>&                dstImage,
            VkImageSubresourceLayers      dstSubresource,
      const DxvkBufferSlice&              srcSlice);

  private:

    void ConvertGenericFormat(
      const Rc<DxvkCommandList>&          ctx,
            D3D9_CONVERSION_FORMAT_INFO   videoFormat,
      const Rc<DxvkImage>&                dstImage,
            VkImageSubresourceLayers      dstSubresource,
      const DxvkBufferSlice&              srcSlice,
            VkFormat                      bufferFormat,
            VkExtent2D                    macroPixelRun);

    enum BindingIds : uint32_t {
      Image  = 0,
      Buffer = 1,
    };

    void InitPipelines();

    const DxvkPipelineLayout* CreatePipelineLayout();

    VkPipeline CreatePipeline(size_t size, const uint32_t* code, uint32_t specConstant);

    Rc<DxvkDevice>            m_device;

    const DxvkPipelineLayout* m_layout = nullptr;

    std::array<VkPipeline, D3D9ConversionFormat_Count> m_pipelines = { };

  };
  
}
