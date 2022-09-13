#include "dxvk_meta_mipgen.h"

namespace dxvk {

  DxvkMetaMipGenRenderPass::DxvkMetaMipGenRenderPass(
    const Rc<vk::DeviceFn>&   vkd,
    const Rc<DxvkImageView>&  view)
  : m_vkd(vkd), m_view(view) {
    // Determine view type based on image type
    const std::array<std::pair<VkImageViewType, VkImageViewType>, 3> viewTypes = {{
      { VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_1D_ARRAY },
      { VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY },
      { VK_IMAGE_VIEW_TYPE_3D,       VK_IMAGE_VIEW_TYPE_2D_ARRAY },
    }};
    
    m_srcViewType = viewTypes.at(uint32_t(view->imageInfo().type)).first;
    m_dstViewType = viewTypes.at(uint32_t(view->imageInfo().type)).second;
    
    // Create image views and framebuffers
    m_passes.resize(view->info().numLevels - 1);
    
    for (uint32_t i = 0; i < m_passes.size(); i++)
      m_passes[i] = createViews(i);
  }
  
  
  DxvkMetaMipGenRenderPass::~DxvkMetaMipGenRenderPass() {
    for (const auto& views : m_passes) {
      m_vkd->vkDestroyImageView(m_vkd->device(), views.src, nullptr);
      m_vkd->vkDestroyImageView(m_vkd->device(), views.dst, nullptr);
    }
  }
  
  
  VkExtent3D DxvkMetaMipGenRenderPass::computePassExtent(uint32_t passId) const {
    VkExtent3D extent = m_view->mipLevelExtent(passId + 1);
    
    if (m_view->imageInfo().type != VK_IMAGE_TYPE_3D)
      extent.depth = m_view->info().numLayers;
    
    return extent;
  }
  
  
  DxvkMetaMipGenRenderPass::PassViews DxvkMetaMipGenRenderPass::createViews(uint32_t pass) const {
    PassViews result = { };

    VkImageViewUsageCreateInfo usageInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };

    VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, &usageInfo };
    viewInfo.image = m_view->imageHandle();
    viewInfo.format = m_view->info().format;
    
    // Create source image view, which points to
    // the one mip level we're going to sample.
    VkImageSubresourceRange srcSubresources;
    srcSubresources.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    srcSubresources.baseMipLevel   = m_view->info().minLevel + pass;
    srcSubresources.levelCount     = 1;
    srcSubresources.baseArrayLayer = m_view->info().minLayer;
    srcSubresources.layerCount     = m_view->info().numLayers;
    
    usageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    viewInfo.viewType = m_srcViewType;
    viewInfo.subresourceRange = srcSubresources;

    if (m_vkd->vkCreateImageView(m_vkd->device(), &viewInfo, nullptr, &result.src) != VK_SUCCESS)
      throw DxvkError("DxvkMetaMipGenRenderPass: Failed to create source image view");
    
    // Create destination image view, which points
    // to the mip level we're going to render to.
    VkExtent3D dstExtent = m_view->mipLevelExtent(pass + 1);
    
    VkImageSubresourceRange dstSubresources;
    dstSubresources.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    dstSubresources.baseMipLevel   = m_view->info().minLevel + pass + 1;
    dstSubresources.levelCount     = 1;
    
    if (m_view->imageInfo().type != VK_IMAGE_TYPE_3D) {
      dstSubresources.baseArrayLayer = m_view->info().minLayer;
      dstSubresources.layerCount = m_view->info().numLayers;
    } else {
      dstSubresources.baseArrayLayer = 0;
      dstSubresources.layerCount = dstExtent.depth;
    }
    
    usageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    viewInfo.viewType = m_dstViewType;
    viewInfo.subresourceRange = dstSubresources;
    
    if (m_vkd->vkCreateImageView(m_vkd->device(), &viewInfo, nullptr, &result.dst) != VK_SUCCESS)
      throw DxvkError("DxvkMetaMipGenRenderPass: Failed to create destination image view");

    return result;
  }
  
}
