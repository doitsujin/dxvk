#pragma once

#include <mutex>
#include <unordered_map>

#include "../spirv/spirv_code_buffer.h"

#include "dxvk_barrier.h"
#include "dxvk_cmdlist.h"
#include "dxvk_resource.h"

namespace dxvk {

  /**
   * \brief Resolve pipeline
   * 
   * Stores the objects for a single pipeline
   * that is used for fragment shader resolve.
   */
  struct DxvkMetaResolvePipeline {
    VkRenderPass          renderPass;
    VkDescriptorSetLayout dsetLayout;
    VkPipelineLayout      pipeLayout;
    VkPipeline            pipeHandle;
  };

  /**
   * \brief Meta resolve render pass
   * 
   * Stores a framebuffer and image view objects
   * for a meta resolve operation. Can be tracked.
   */
  class DxvkMetaResolveRenderPass : public DxvkResource {
    
  public:
    
    DxvkMetaResolveRenderPass(
      const Rc<vk::DeviceFn>&   vkd,
      const Rc<DxvkImageView>&  dstImageView,
      const Rc<DxvkImageView>&  srcImageView);
    
    ~DxvkMetaResolveRenderPass();
    
    VkRenderPass renderPass() const {
      return m_renderPass;
    }

    VkFramebuffer framebuffer() const {
      return m_framebuffer;
    }

  private:
    
    const Rc<vk::DeviceFn>  m_vkd;

    const Rc<DxvkImageView> m_dstImageView;
    const Rc<DxvkImageView> m_srcImageView;
    
    VkRenderPass  m_renderPass  = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    VkRenderPass createRenderPass() const;

    VkFramebuffer createFramebuffer() const;

  };
  
}