#include "d3d9_format_helpers.h"

#include <d3d9_convert_yuy2_uyvy.h>
#include <d3d9_convert_l6v5u5.h>
#include <d3d9_convert_x8l8v8u8.h>
#include <d3d9_convert_a2w10v10u10.h>
#include <d3d9_convert_nv12.h>
#include <d3d9_convert_yv12.h>

namespace dxvk {

  D3D9FormatHelper::D3D9FormatHelper(const Rc<DxvkDevice>& device)
    : m_device(device), m_context(m_device->createContext()) {
    m_context->beginRecording(
      m_device->createCommandList());

    InitShaders();
  }


  void D3D9FormatHelper::Flush() {
    if (m_transferCommands != 0)
      FlushInternal();
  }


  void D3D9FormatHelper::ConvertFormat(
          D3D9_CONVERSION_FORMAT_INFO   conversionFormat,
    const Rc<DxvkImage>&                dstImage,
          VkImageSubresourceLayers      dstSubresource,
    const DxvkBufferSlice&              srcSlice) {
    switch (conversionFormat.FormatType) {
      case D3D9ConversionFormat_YUY2:
      case D3D9ConversionFormat_UYVY: {
        uint32_t specConstant = conversionFormat.FormatType == D3D9ConversionFormat_UYVY ? 1 : 0;
        ConvertGenericFormat(conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R32_UINT, specConstant, { 2u, 1u });
        break;
      }

      case D3D9ConversionFormat_NV12:
        ConvertGenericFormat(conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R16_UINT, 0, { 2u, 1u });
        break;

      case D3D9ConversionFormat_YV12:
        ConvertGenericFormat(conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R8_UINT, 0, { 1u, 1u });
        break;

      case D3D9ConversionFormat_L6V5U5:
        ConvertGenericFormat(conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R16_UINT, 0, { 1u, 1u });
        break;

      case D3D9ConversionFormat_X8L8V8U8:
        ConvertGenericFormat(conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R32_UINT, 0, { 1u, 1u });
        break;

      case D3D9ConversionFormat_A2W10V10U10:
        ConvertGenericFormat(conversionFormat, dstImage, dstSubresource, srcSlice, VK_FORMAT_R32_UINT, 0, { 1u, 1u });
        break;

      default:
        Logger::warn("Unimplemented format conversion");
    }
  }


  void D3D9FormatHelper::ConvertGenericFormat(
          D3D9_CONVERSION_FORMAT_INFO   videoFormat,
    const Rc<DxvkImage>&                dstImage,
          VkImageSubresourceLayers      dstSubresource,
    const DxvkBufferSlice&              srcSlice,
          VkFormat                      bufferFormat,
          uint32_t                      specConstantValue,
          VkExtent2D                    macroPixelRun) {
    DxvkImageViewCreateInfo imageViewInfo;
    imageViewInfo.type      = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format    = dstImage->info().format;
    imageViewInfo.usage     = VK_IMAGE_USAGE_STORAGE_BIT;
    imageViewInfo.aspect    = dstSubresource.aspectMask;
    imageViewInfo.minLevel  = dstSubresource.mipLevel;
    imageViewInfo.numLevels = 1;
    imageViewInfo.minLayer  = dstSubresource.baseArrayLayer;
    imageViewInfo.numLayers = dstSubresource.layerCount;
    auto tmpImageView = m_device->createImageView(dstImage, imageViewInfo);

    VkExtent3D imageExtent = dstImage->mipLevelExtent(dstSubresource.mipLevel);
    imageExtent = VkExtent3D{ imageExtent.width  / macroPixelRun.width,
                              imageExtent.height / macroPixelRun.height,
                              1 };

    DxvkBufferViewCreateInfo bufferViewInfo;
    bufferViewInfo.format      = bufferFormat;
    bufferViewInfo.rangeOffset = srcSlice.offset();
    bufferViewInfo.rangeLength = srcSlice.length();
    auto tmpBufferView = m_device->createBufferView(srcSlice.buffer(), bufferViewInfo);

    if (specConstantValue)
      m_context->setSpecConstant(VK_PIPELINE_BIND_POINT_COMPUTE, 0, specConstantValue);

    m_context->bindResourceView(BindingIds::Image,  tmpImageView, nullptr);
    m_context->bindResourceView(BindingIds::Buffer, nullptr,     tmpBufferView);
    m_context->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, m_shaders[videoFormat.FormatType]);
    m_context->pushConstants(0, sizeof(VkExtent2D), &imageExtent);
    m_context->dispatch(
      (imageExtent.width  / 8) + (imageExtent.width  % 8),
      (imageExtent.height / 8) + (imageExtent.height % 8),
      1);

    // Reset the spec constants used...
    if (specConstantValue)
      m_context->setSpecConstant(VK_PIPELINE_BIND_POINT_COMPUTE, 0, 0);
    
    m_transferCommands += 1;
  }


  void D3D9FormatHelper::InitShaders() {
    m_shaders[D3D9ConversionFormat_YUY2] = InitShader(d3d9_convert_yuy2_uyvy);
    m_shaders[D3D9ConversionFormat_UYVY] = m_shaders[D3D9ConversionFormat_YUY2];
    m_shaders[D3D9ConversionFormat_L6V5U5] = InitShader(d3d9_convert_l6v5u5);
    m_shaders[D3D9ConversionFormat_X8L8V8U8] = InitShader(d3d9_convert_x8l8v8u8);
    m_shaders[D3D9ConversionFormat_A2W10V10U10] = InitShader(d3d9_convert_a2w10v10u10);
    m_shaders[D3D9ConversionFormat_NV12] = InitShader(d3d9_convert_nv12);
    m_shaders[D3D9ConversionFormat_YV12] = InitShader(d3d9_convert_yv12);
  }


  Rc<DxvkShader> D3D9FormatHelper::InitShader(SpirvCodeBuffer code) {
    const std::array<DxvkResourceSlot, 2> resourceSlots = { {
      { BindingIds::Image,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,        VK_IMAGE_VIEW_TYPE_2D },
      { BindingIds::Buffer, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_IMAGE_VIEW_TYPE_1D },
    } };

    return m_device->createShader(
      VK_SHADER_STAGE_COMPUTE_BIT,
      resourceSlots.size(), resourceSlots.data(),
      { 0u, 0u, 0u, sizeof(VkExtent2D) }, code);
  }


  void D3D9FormatHelper::FlushInternal() {
    m_context->flushCommandList();
    
    m_transferCommands = 0;
  }

}