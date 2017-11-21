#pragma once

#include "dxvk_adapter.h"
#include "dxvk_buffer.h"
#include "dxvk_compute.h"
#include "dxvk_constant_state.h"
#include "dxvk_context.h"
#include "dxvk_framebuffer.h"
#include "dxvk_memory.h"
#include "dxvk_pipemgr.h"
#include "dxvk_renderpass.h"
#include "dxvk_shader.h"
#include "dxvk_swapchain.h"
#include "dxvk_sync.h"

namespace dxvk {
  
  class DxvkInstance;
  
  /**
   * \brief DXVK device
   * 
   * Device object. This is responsible for resource creation,
   * memory allocation, command submission and state tracking.
   * Rendering commands are recorded into command lists using
   * contexts. Multiple contexts can be created for a device.
   */
  class DxvkDevice : public RcObject {
    
  public:
    
    DxvkDevice(
      const Rc<DxvkAdapter>&  adapter,
      const Rc<vk::DeviceFn>& vkd);
    ~DxvkDevice();
    
    /**
     * \brief Vulkan device functions
     * \returns Vulkan device functions
     */
    Rc<vk::DeviceFn> vkd() const {
      return m_vkd;
    }
    
    /**
     * \brief Logical device handle
     * \returns The device handle
     */
    VkDevice handle() const {
      return m_vkd->device();
    }
    
    /**
     * \brief The adapter
     * 
     * The physical device that the
     * device has been created for.
     * \returns Adapter
     */
    Rc<DxvkAdapter> adapter() const {
      return m_adapter;
    }
    
    /**
     * \brief Creates a command list
     * \returns The command list
     */
    Rc<DxvkCommandList> createCommandList();
    
    /**
     * \brief Creates a context
     * 
     * Creates a context object that can
     * be used to record command buffers.
     * \returns The context object
     */
    Rc<DxvkContext> createContext();
    
    /**
     * \brief Creates framebuffer for a set of render targets
     * 
     * Automatically deduces framebuffer dimensions
     * from the supplied render target views.
     * \param [in] renderTargets Render targets
     * \returns The framebuffer object
     */
    Rc<DxvkFramebuffer> createFramebuffer(
      const DxvkRenderTargets& renderTargets);
    
    /**
     * \brief Creates a buffer object
     * 
     * \param [in] createInfo Buffer create info
     * \param [in] memoryType Memory type flags
     * \returns The buffer object
     */
    Rc<DxvkBuffer> createBuffer(
      const DxvkBufferCreateInfo& createInfo,
            VkMemoryPropertyFlags memoryType);
    
    /**
     * \brief Creates a buffer view
     * 
     * \param [in] buffer The buffer to view
     * \param [in] createInfo Buffer view properties
     * \returns The buffer view object
     */
    Rc<DxvkBufferView> createBufferView(
      const Rc<DxvkBuffer>&           buffer,
      const DxvkBufferViewCreateInfo& createInfo);
    
    /**
     * \brief Creates an image object
     * 
     * \param [in] createInfo Image create info
     * \param [in] memoryType Memory type flags
     * \returns The image object
     */
    Rc<DxvkImage> createImage(
      const DxvkImageCreateInfo&  createInfo,
            VkMemoryPropertyFlags memoryType);
    
    /**
     * \brief Creates an image view
     * 
     * \param [in] image The image to create a view for
     * \param [in] createInfo Image view create info
     * \returns The image view
     */
    Rc<DxvkImageView> createImageView(
      const Rc<DxvkImage>&            image,
      const DxvkImageViewCreateInfo&  createInfo);
    
    /**
     * \brief Creates a semaphore object
     * \returns Newly created semaphore
     */
    Rc<DxvkSemaphore> createSemaphore();
    
    /**
     * \brief Creates a shader module
     * 
     * \param [in] stage Shader stage
     * \param [in] code Shader code
     * \returns New shader module
     */
    Rc<DxvkShader> createShader(
            VkShaderStageFlagBits     stage,
      const SpirvCodeBuffer&          code);
    
    /**
     * \brief Creates a swap chain
     * 
     * \param [in] surface The target surface
     * \param [in] properties Swapchain properties
     * \returns The swapchain object
     */
    Rc<DxvkSwapchain> createSwapchain(
      const Rc<DxvkSurface>&          surface,
      const DxvkSwapchainProperties&  properties);
    
    /**
     * \brief Submits a command list
     * 
     * Synchronization arguments are optional. 
     * \param [in] commandList The command list to submit
     * \param [in] waitSync (Optional) Semaphore to wait on
     * \param [in] wakeSync (Optional) Semaphore to notify
     * \returns Synchronization fence
     */
    Rc<DxvkFence> submitCommandList(
      const Rc<DxvkCommandList>&      commandList,
      const Rc<DxvkSemaphore>&        waitSync,
      const Rc<DxvkSemaphore>&        wakeSync);
    
    /**
     * \brief Waits until the device becomes idle
     * 
     * Waits for the GPU to complete the execution of all
     * previously submitted command buffers. This may be
     * used to ensure that resources that were previously
     * used by the GPU can be safely destroyed.
     */
    void waitForIdle() const;
    
  private:
    
    Rc<DxvkAdapter>   m_adapter;
    Rc<vk::DeviceFn>  m_vkd;
    
    Rc<DxvkMemoryAllocator> m_memory;
    Rc<DxvkRenderPassPool>  m_renderPassPool;
    Rc<DxvkPipelineManager> m_pipelineManager;
    
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    
  };
  
}