#pragma once

#include <mutex>
#include <vector>
#include <unordered_map>

#include "../util/sync/sync_list.h"

#include "dxvk_hash.h"
#include "dxvk_include.h"
#include "dxvk_limits.h"

namespace dxvk {

  class DxvkDevice;
  
  /**
   * \brief Color attachment transitions
   * 
   * Stores the load/store ops and the initial
   * and final layout of a single attachment.
   */
  struct DxvkColorAttachmentOps {
    VkAttachmentLoadOp  loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkImageLayout       loadLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout       storeLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkClearColorValue   clearValue  = VkClearColorValue();
  };
  
  
  /**
   * \brief Depth attachment transitions
   * 
   * Stores the load/store ops and the initial and
   * final layout of the depth-stencil attachment.
   */
  struct DxvkDepthAttachmentOps {
    VkAttachmentLoadOp  loadOpD     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentLoadOp  loadOpS     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkImageLayout       loadLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout       storeLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkClearDepthStencilValue clearValue = VkClearDepthStencilValue();
  };
  
  
  /**
   * \brief Render pass transitions
   * 
   * Stores transitions for all depth and color attachments.
   * This is used to select a specific render pass object
   * from a group of render passes with the same format.
   */
  struct DxvkRenderPassOps {
    DxvkDepthAttachmentOps depthOps;
    DxvkColorAttachmentOps colorOps[MaxNumRenderTargets];
  };
 
}