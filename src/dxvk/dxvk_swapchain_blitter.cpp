#include "dxvk_swapchain_blitter.h"

#include <dxvk_present_frag.h>
#include <dxvk_present_frag_blit.h>
#include <dxvk_present_frag_ms.h>
#include <dxvk_present_frag_ms_amd.h>
#include <dxvk_present_vert.h>

namespace dxvk {

  DxvkSwapchainBlitter::DxvkSwapchainBlitter(const Rc<DxvkDevice>& device)
  : m_device(device) { }


  DxvkSwapchainBlitter::~DxvkSwapchainBlitter() { }


  void DxvkSwapchainBlitter::presentImage(
          DxvkContext*        ctx,
    const Rc<DxvkImageView>&  dstView,
          VkRect2D            dstRect,
    const Rc<DxvkImageView>&  srcView,
          VkRect2D            srcRect) {
    if (m_gammaDirty)
      this->updateGammaTexture(ctx);

    ctx->presentBlit(dstView, dstRect, srcView, srcRect, m_gammaView);
  }


  void DxvkSwapchainBlitter::setGammaRamp(
          uint32_t            cpCount,
    const DxvkGammaCp*        cpData) {
    VkDeviceSize size = cpCount * sizeof(*cpData);

    if (cpCount) {
      if (m_gammaBuffer == nullptr || m_gammaBuffer->info().size < size) {
        DxvkBufferCreateInfo bufInfo;
        bufInfo.size = size;
        bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufInfo.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
        bufInfo.access = VK_ACCESS_TRANSFER_READ_BIT;

        m_gammaBuffer = m_device->createBuffer(bufInfo,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      }

      if (!m_gammaSlice.handle)
        m_gammaSlice = m_gammaBuffer->allocSlice();

      std::memcpy(m_gammaSlice.mapPtr, cpData, size);
    } else {
      m_gammaBuffer = nullptr;
      m_gammaSlice = DxvkBufferSliceHandle();
    }

    m_gammaCpCount = cpCount;
    m_gammaDirty = true;
  }


  void DxvkSwapchainBlitter::updateGammaTexture(DxvkContext* ctx) {
    uint32_t n = m_gammaCpCount;

    if (n) {
      // Reuse existing image if possible
      if (m_gammaImage == nullptr || m_gammaImage->info().extent.width != n) {
        DxvkImageCreateInfo imgInfo;
        imgInfo.type        = VK_IMAGE_TYPE_1D;
        imgInfo.format      = VK_FORMAT_R16G16B16A16_UNORM;
        imgInfo.flags       = 0;
        imgInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.extent      = { n, 1, 1 };
        imgInfo.numLayers   = 1;
        imgInfo.mipLevels   = 1;
        imgInfo.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.stages      = VK_PIPELINE_STAGE_TRANSFER_BIT
                            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        imgInfo.access      = VK_ACCESS_TRANSFER_WRITE_BIT
                            | VK_ACCESS_SHADER_READ_BIT;
        imgInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.layout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        m_gammaImage = m_device->createImage(
          imgInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        DxvkImageViewCreateInfo viewInfo;
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_1D;
        viewInfo.format     = VK_FORMAT_R16G16B16A16_UNORM;
        viewInfo.usage      = VK_IMAGE_USAGE_SAMPLED_BIT;
        viewInfo.aspect     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.minLevel   = 0;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        
        m_gammaView = m_device->createImageView(m_gammaImage, viewInfo);
      }

      ctx->invalidateBuffer(m_gammaBuffer, m_gammaSlice);
      ctx->copyBufferToImage(m_gammaImage,
        VkImageSubresourceLayers { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        VkOffset3D { 0, 0, 0 },
        VkExtent3D { n, 1, 1 },
        m_gammaBuffer, 0, 0, 0);

      m_gammaSlice = DxvkBufferSliceHandle();
    } else {
      m_gammaImage = nullptr;
      m_gammaView  = nullptr;
    }

    m_gammaDirty = false;
  }

}