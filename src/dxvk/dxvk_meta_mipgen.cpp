#include "dxvk_meta_mipgen.h"

namespace dxvk {

  DxvkMetaMipGenViews::DxvkMetaMipGenViews(
    const Rc<DxvkImageView>&  view)
  : m_view(view) {
    // Determine view type based on image type
    const std::array<std::pair<VkImageViewType, VkImageViewType>, 3> viewTypes = {{
      { VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_1D_ARRAY },
      { VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY },
      { VK_IMAGE_VIEW_TYPE_3D,       VK_IMAGE_VIEW_TYPE_2D_ARRAY },
    }};
    
    m_srcViewType = viewTypes.at(uint32_t(view->image()->info().type)).first;
    m_dstViewType = viewTypes.at(uint32_t(view->image()->info().type)).second;
    
    // Create image views and framebuffers
    m_passes.resize(view->info().mipCount - 1);
    
    for (uint32_t i = 0; i < m_passes.size(); i++)
      m_passes[i] = createViews(i);
  }
  
  
  DxvkMetaMipGenViews::~DxvkMetaMipGenViews() {

  }
  
  
  VkExtent3D DxvkMetaMipGenViews::computePassExtent(uint32_t passId) const {
    VkExtent3D extent = m_view->mipLevelExtent(passId + 1);
    
    if (m_view->image()->info().type != VK_IMAGE_TYPE_3D)
      extent.depth = m_view->info().layerCount;
    
    return extent;
  }
  
  
  DxvkMetaMipGenViews::PassViews DxvkMetaMipGenViews::createViews(uint32_t pass) const {
    PassViews result = { };

    // Source image view
    DxvkImageViewKey srcViewInfo;
    srcViewInfo.viewType = m_srcViewType;
    srcViewInfo.format = m_view->info().format;
    srcViewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    srcViewInfo.aspects = m_view->info().aspects;
    srcViewInfo.mipIndex = m_view->info().mipIndex + pass;
    srcViewInfo.mipCount = 1;
    srcViewInfo.layerIndex = m_view->info().layerIndex;
    srcViewInfo.layerCount = m_view->info().layerCount;

    result.src = m_view->image()->createView(srcViewInfo);
    
    // Create destination image view, which points
    // to the mip level we're going to render to.
    VkExtent3D dstExtent = m_view->mipLevelExtent(pass + 1);
    
    DxvkImageViewKey dstViewInfo;
    dstViewInfo.viewType = m_dstViewType;
    dstViewInfo.format = m_view->info().format;
    dstViewInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    dstViewInfo.aspects = m_view->info().aspects;
    dstViewInfo.mipIndex = m_view->info().mipIndex + pass + 1;
    dstViewInfo.mipCount = 1u;
    
    if (m_view->image()->info().type != VK_IMAGE_TYPE_3D) {
      dstViewInfo.layerIndex = m_view->info().layerIndex;
      dstViewInfo.layerCount = m_view->info().layerCount;
    } else {
      dstViewInfo.layerIndex = 0;
      dstViewInfo.layerCount = dstExtent.depth;
    }

    result.dst = m_view->image()->createView(dstViewInfo);

    return result;
  }
  
}
