#include "d3d9_format_helpers.h"

#include <d3d9_convert_yuy2_uyvy.h>
#include <d3d9_convert_l6v5u5.h>
#include <d3d9_convert_x8l8v8u8.h>
#include <d3d9_convert_a2w10v10u10.h>
#include <d3d9_convert_w11v11u10.h>
#include <d3d9_convert_nv12.h>
#include <d3d9_convert_yv12.h>

namespace dxvk {

  D3D9FormatHelper::D3D9FormatHelper(const Rc<DxvkDevice>& device)
  : m_device          (device)
  , m_layout          (CreatePipelineLayout()) {
    InitPipelines();
  }


  D3D9FormatHelper::~D3D9FormatHelper() {
    auto vk = m_device->vkd();

    for (auto& p : m_pipelines)
      vk->vkDestroyPipeline(vk->device(), p, nullptr);
  }


  void D3D9FormatHelper::ConvertFormat(
    const Rc<DxvkCommandList>&          ctx,
          D3D9_CONVERSION_FORMAT_INFO   conversionFormat,
    const Rc<DxvkImage>&                dstImage,
          VkImageSubresourceLayers      dstSubresource,
    const DxvkBufferSlice&              srcSlice) {
    switch (conversionFormat.FormatType) {
      case D3D9ConversionFormat_YUY2:
      case D3D9ConversionFormat_UYVY: {
        ConvertGenericFormat(ctx, conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R32_UINT, { 2u, 1u });
        break;
      }

      case D3D9ConversionFormat_NV12:
        ConvertGenericFormat(ctx, conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R16_UINT, { 2u, 1u });
        break;

      case D3D9ConversionFormat_YV12:
        ConvertGenericFormat(ctx, conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R8_UINT, { 1u, 1u });
        break;

      case D3D9ConversionFormat_L6V5U5:
        ConvertGenericFormat(ctx, conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R16_UINT, { 1u, 1u });
        break;

      case D3D9ConversionFormat_X8L8V8U8:
        ConvertGenericFormat(ctx, conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R32_UINT, { 1u, 1u });
        break;

      case D3D9ConversionFormat_A2W10V10U10:
        ConvertGenericFormat(ctx, conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R32_UINT, { 1u, 1u });
        break;

      case D3D9ConversionFormat_W11V11U10:
        ConvertGenericFormat(ctx, conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R32_UINT, { 1u, 1u });
        break;

      default:
        Logger::warn("Unimplemented format conversion");
    }
  }


  void D3D9FormatHelper::ConvertGenericFormat(
    const Rc<DxvkCommandList>&          ctx,
          D3D9_CONVERSION_FORMAT_INFO   videoFormat,
    const Rc<DxvkImage>&                dstImage,
          VkImageSubresourceLayers      dstSubresource,
    const DxvkBufferSlice&              srcSlice,
          VkFormat                      bufferFormat,
          VkExtent2D                    macroPixelRun) {
    DxvkImageViewKey imageViewInfo;
    imageViewInfo.viewType  = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format    = dstImage->info().format;
    imageViewInfo.usage     = VK_IMAGE_USAGE_STORAGE_BIT;
    imageViewInfo.layout    = VK_IMAGE_LAYOUT_GENERAL;
    imageViewInfo.aspects   = dstSubresource.aspectMask;
    imageViewInfo.mipIndex  = dstSubresource.mipLevel;
    imageViewInfo.mipCount  = 1;
    imageViewInfo.layerIndex = dstSubresource.baseArrayLayer;
    imageViewInfo.layerCount = dstSubresource.layerCount;
    auto tmpImageView = dstImage->createView(imageViewInfo);

    VkExtent3D imageExtent = dstImage->mipLevelExtent(dstSubresource.mipLevel);
    imageExtent = VkExtent3D{ imageExtent.width  / macroPixelRun.width,
                              imageExtent.height / macroPixelRun.height,
                              1 };

    DxvkBufferViewKey bufferViewInfo;
    bufferViewInfo.format = bufferFormat;
    bufferViewInfo.offset = srcSlice.offset();
    bufferViewInfo.size = srcSlice.length();
    bufferViewInfo.usage = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    auto tmpBufferView = srcSlice.buffer()->createView(bufferViewInfo);

    std::array<DxvkDescriptorWrite, 2> descriptors = { };

    auto& imageDescriptor = descriptors[0u];
    imageDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imageDescriptor.descriptor = tmpImageView->getDescriptor();

    auto& bufferDescriptor = descriptors[1u];
    bufferDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    bufferDescriptor.descriptor = tmpBufferView->getDescriptor(false);

    ctx->cmdBindPipeline(DxvkCmdBuffer::ExecBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines[videoFormat.FormatType]);

    ctx->bindResources(DxvkCmdBuffer::ExecBuffer,
      m_layout, descriptors.size(), descriptors.data(),
      sizeof(imageExtent), &imageExtent);

    ctx->cmdDispatch(DxvkCmdBuffer::ExecBuffer,
      ((imageExtent.width + 7u) / 8u),
      ((imageExtent.height + 7u) / 8u),
      1u);

    // We can reasonably assume that the image is in GENERAL layout anyway
    VkMemoryBarrier2 memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    memoryBarrier.dstStageMask = dstImage->info().stages | srcSlice.buffer()->info().stages;
    memoryBarrier.dstAccessMask = dstImage->info().access | srcSlice.buffer()->info().access;

    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.memoryBarrierCount = 1u;
    depInfo.pMemoryBarriers = &memoryBarrier;

    ctx->cmdPipelineBarrier(DxvkCmdBuffer::ExecBuffer, &depInfo);

    ctx->track(tmpImageView->image(), DxvkAccess::Write);
    ctx->track(tmpBufferView->buffer(), DxvkAccess::Read);
  }


  void D3D9FormatHelper::InitPipelines() {
    m_pipelines[D3D9ConversionFormat_YUY2] = CreatePipeline(sizeof(d3d9_convert_yuy2_uyvy), d3d9_convert_yuy2_uyvy, 0);
    m_pipelines[D3D9ConversionFormat_UYVY] = CreatePipeline(sizeof(d3d9_convert_yuy2_uyvy), d3d9_convert_yuy2_uyvy, 1);
    m_pipelines[D3D9ConversionFormat_L6V5U5] = CreatePipeline(sizeof(d3d9_convert_l6v5u5), d3d9_convert_l6v5u5, 0);
    m_pipelines[D3D9ConversionFormat_X8L8V8U8] = CreatePipeline(sizeof(d3d9_convert_x8l8v8u8), d3d9_convert_x8l8v8u8, 0);
    m_pipelines[D3D9ConversionFormat_A2W10V10U10] = CreatePipeline(sizeof(d3d9_convert_a2w10v10u10), d3d9_convert_a2w10v10u10, 0);
    m_pipelines[D3D9ConversionFormat_W11V11U10] = CreatePipeline(sizeof(d3d9_convert_w11v11u10), d3d9_convert_w11v11u10, 0);
    m_pipelines[D3D9ConversionFormat_NV12] = CreatePipeline(sizeof(d3d9_convert_nv12), d3d9_convert_nv12, 0);
    m_pipelines[D3D9ConversionFormat_YV12] = CreatePipeline(sizeof(d3d9_convert_yv12), d3d9_convert_yv12, 0);
  }


  const DxvkPipelineLayout* D3D9FormatHelper::CreatePipelineLayout() {
    static const std::array<DxvkDescriptorSetLayoutBinding, 2> bindings = {{
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,        1, VK_SHADER_STAGE_COMPUTE_BIT },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
    }};

    return m_device->createBuiltInPipelineLayout(0u, VK_SHADER_STAGE_COMPUTE_BIT,
      sizeof(VkExtent2D), bindings.size(), bindings.data());
  }


  VkPipeline D3D9FormatHelper::CreatePipeline(size_t size, const uint32_t* code, uint32_t specConstant) {
    auto vk = m_device->vkd();

    VkSpecializationMapEntry specEntry = { };
    specEntry.size = sizeof(specConstant);

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specEntry;
    specInfo.dataSize = sizeof(specConstant);
    specInfo.pData = &specConstant;

    util::DxvkBuiltInShaderStage stage;
    stage.size = size;
    stage.code = code;
    stage.spec = &specInfo;

    return m_device->createBuiltInComputePipeline(m_layout, stage);
  }

}
