#include "d3d9_format_helpers.h"

#include <d3d9_convert_yuy2_uyvy.h>

namespace dxvk {

  D3D9FormatHelper::D3D9FormatHelper(const Rc<DxvkDevice>& device)
    : m_device(device), m_context(m_device->createContext()) {
    m_context->beginRecording(
      m_device->createCommandList());

    InitShaders();
  }


  void D3D9FormatHelper::ConvertVideoFormat(
          D3D9_VIDEO_FORMAT_INFO   videoFormat,
    const Rc<DxvkImage>&           dstImage,
          VkImageSubresourceLayers dstSubresource,
    const Rc<DxvkBuffer>&          srcBuffer) {
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
    imageExtent = VkExtent3D{ imageExtent.width  / videoFormat.MacroPixelSize.width,
                              imageExtent.height / videoFormat.MacroPixelSize.height,
                              1 };

    DxvkBufferViewCreateInfo bufferViewInfo;
    bufferViewInfo.format      = VK_FORMAT_R32_UINT;
    bufferViewInfo.rangeOffset = 0;
    bufferViewInfo.rangeLength = srcBuffer->info().size;
    auto tmpBufferView = m_device->createBufferView(srcBuffer, bufferViewInfo);

    if (videoFormat.FormatType == D3D9VideoFormat_UYVY
     || videoFormat.FormatType == D3D9VideoFormat_YUY2) {
      m_context->setSpecConstant(0, videoFormat.FormatType == D3D9VideoFormat_UYVY);
    }

    m_context->bindResourceView(BindingIds::Image,  tmpImageView, nullptr);
    m_context->bindResourceView(BindingIds::Buffer, nullptr,     tmpBufferView);
    m_context->bindShader(VK_SHADER_STAGE_COMPUTE_BIT, m_shaders[videoFormat.FormatType]);
    m_context->pushConstants(0, sizeof(VkExtent2D), &imageExtent);
    m_context->dispatch(
      (imageExtent.width  / 8) + (imageExtent.width  % 8),
      (imageExtent.height / 8) + (imageExtent.height % 8),
      1);
    
    m_context->flushCommandList();
  }


  void D3D9FormatHelper::InitShaders() {
    m_shaders[D3D9VideoFormat_YUY2] = InitShader(d3d9_convert_yuy2_uyvy);
    m_shaders[D3D9VideoFormat_UYVY] = m_shaders[D3D9VideoFormat_YUY2];
  }


  Rc<DxvkShader> D3D9FormatHelper::InitShader(SpirvCodeBuffer code) {
    const std::array<DxvkResourceSlot, 2> resourceSlots = { {
      { BindingIds::Image,  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  VK_IMAGE_VIEW_TYPE_2D },
      { BindingIds::Buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_IMAGE_VIEW_TYPE_1D },
    } };

    return m_device->createShader(
      VK_SHADER_STAGE_COMPUTE_BIT,
      resourceSlots.size(), resourceSlots.data(),
      { 0u, 0u, 0u, sizeof(VkExtent2D) }, code);
  }

}