#pragma once

#include "dxvk_adapter.h"
#include "dxvk_buffer.h"
#include "dxvk_compute.h"
#include "dxvk_constant_state.h"
#include "dxvk_context.h"
#include "dxvk_extensions.h"
#include "dxvk_framebuffer.h"
#include "dxvk_image.h"
#include "dxvk_memory.h"
#include "dxvk_options.h"
#include "dxvk_pipecache.h"
#include "dxvk_pipemanager.h"
#include "dxvk_queue.h"
#include "dxvk_query_pool.h"
#include "dxvk_recycler.h"
#include "dxvk_renderpass.h"
#include "dxvk_sampler.h"
#include "dxvk_shader.h"
#include "dxvk_swapchain.h"
#include "dxvk_sync.h"
#include "dxvk_unbound.h"

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
    friend class DxvkContext;
    friend class DxvkSubmissionQueue;
    
    constexpr static VkDeviceSize DefaultStagingBufferSize = 4 * 1024 * 1024;
  public:
    
    DxvkDevice(
      const Rc<DxvkAdapter>&          adapter,
      const Rc<vk::DeviceFn>&         vkd,
      const Rc<DxvkDeviceExtensions>& extensions,
      const VkPhysicalDeviceFeatures& features);
      
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
     * \brief Checks whether an option is enabled
     * 
     * \param [in] option The option to check for
     * \returns \c true if the option is enabled
     */
    bool hasOption(DxvkOption option) const {
      return m_options.test(option);
    }
    
    /**
     * \brief Enabled device extensions
     * \returns Enabled device extensions
     */
    const DxvkDeviceExtensions& extensions() const {
      return *m_extensions;
    }
    
    /**
     * \brief Enabled device features
     * \returns Enabled features
     */
    const VkPhysicalDeviceFeatures& features() const {
      return m_features;
    }
    
    /**
     * \brief Allocates a physical buffer
     * 
     * \param [in] createInfo Buffer create info
     * \param [in] memoryType Memory property flags
     * \returns The buffer resource object
     */
    Rc<DxvkPhysicalBuffer> allocPhysicalBuffer(
      const DxvkBufferCreateInfo& createInfo,
            VkMemoryPropertyFlags memoryType);
    
    /**
     * \brief Allocates a staging buffer
     * 
     * Returns a staging buffer that is at least as large
     * as the requested size. It is usually bigger so that
     * a single staging buffer may serve multiple allocations.
     * \param [in] size Minimum buffer size
     * \returns The staging buffer
     */
    Rc<DxvkStagingBuffer> allocStagingBuffer(
            VkDeviceSize size);
    
    /**
     * \brief Recycles a staging buffer
     * 
     * When a staging buffer is no longer needed, it should
     * be returned to the device so that it can be reused
     * for subsequent allocations.
     * \param [in] buffer The buffer
     */
    void recycleStagingBuffer(
      const Rc<DxvkStagingBuffer>& buffer);
    
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
     * \brief Creates a query pool
     * \param [in] queryType Query type
     */
    Rc<DxvkQueryPool> createQueryPool(
            VkQueryType               queryType,
            uint32_t                  queryCount);
    
    /**
     * \brief Creates a sampler object
     * 
     * \param [in] createInfo Sampler parameters
     * \returns Newly created sampler object
     */
    Rc<DxvkSampler> createSampler(
      const DxvkSamplerCreateInfo&  createInfo);
    
    /**
     * \brief Creates a semaphore object
     * \returns Newly created semaphore
     */
    Rc<DxvkSemaphore> createSemaphore();
    
    /**
     * \brief Creates a shader module
     * 
     * \param [in] stage Shader stage
     * \param [in] slotCount Resource slot count
     * \param [in] slotInfos Resource slot descriptions
     * \param [in] iface Inter-stage interface slots
     * \param [in] code Shader code
     * \returns New shader module
     */
    Rc<DxvkShader> createShader(
            VkShaderStageFlagBits     stage,
            uint32_t                  slotCount,
      const DxvkResourceSlot*         slotInfos,
      const DxvkInterfaceSlots&       iface,
      const SpirvCodeBuffer&          code);
    
    /**
     * \brief Retrieves a compute pipeline
     * 
     * \param [in] layout Pipeline binding layout
     * \param [in] cs Compute shader
     * \returns The compute pipeline
     */
    Rc<DxvkComputePipeline> createComputePipeline(
      const Rc<DxvkShader>&           cs);
    
    /**
     * \brief Retrieves a graphics pipeline object
     * 
     * \param [in] vs Vertex shader
     * \param [in] tcs Tessellation control shader
     * \param [in] tes Tessellation evaluation shader
     * \param [in] gs Geometry shader
     * \param [in] fs Fragment shader
     * \returns The graphics pipeline
     */
    Rc<DxvkGraphicsPipeline> createGraphicsPipeline(
      const Rc<DxvkShader>&           vs,
      const Rc<DxvkShader>&           tcs,
      const Rc<DxvkShader>&           tes,
      const Rc<DxvkShader>&           gs,
      const Rc<DxvkShader>&           fs);
    
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
     * \brief Initializes dummy resources
     * 
     * Should be called after creating the device in
     * case the device initialization was successful
     * and the device is usable.
     */
    void initResources();
    
    /**
     * \brief Presents a swap chain image
     * 
     * This is implicitly called by the swap chain class
     * when presenting an image. Do not use this directly.
     * \param [in] presentInfo Swap image present info
     * \returns Present status
     */
    VkResult presentSwapImage(
      const VkPresentInfoKHR&         presentInfo);
    
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
    void waitForIdle();
    
  private:
    
    Rc<DxvkAdapter>           m_adapter;
    Rc<vk::DeviceFn>          m_vkd;
    Rc<DxvkDeviceExtensions>  m_extensions;
    VkPhysicalDeviceFeatures  m_features;
    
    Rc<DxvkMemoryAllocator>   m_memory;
    Rc<DxvkRenderPassPool>    m_renderPassPool;
    Rc<DxvkPipelineCache>     m_pipelineCache;
    Rc<DxvkPipelineManager>   m_pipelineManager;
    
    DxvkUnboundResources      m_unboundResources;
    DxvkOptions               m_options;
    
    std::mutex m_submissionLock;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue  = VK_NULL_HANDLE;
    
    DxvkRecycler<DxvkCommandList,  16> m_recycledCommandLists;
    DxvkRecycler<DxvkStagingBuffer, 4> m_recycledStagingBuffers;
    
    DxvkSubmissionQueue m_submissionQueue;
    
    void recycleCommandList(
      const Rc<DxvkCommandList>& cmdList);
    
    /**
     * \brief Dummy buffer handle
     * \returns Use for unbound vertex buffers.
     */
    VkBuffer dummyBufferHandle() const {
      return m_unboundResources.bufferHandle();
    }
    
    /**
     * \brief Dummy buffer descriptor
     * \returns Descriptor that points to a dummy buffer
     */
    VkDescriptorBufferInfo dummyBufferDescriptor() const {
      return m_unboundResources.bufferDescriptor();
    }
    
    /**
     * \brief Dummy buffer view descriptor
     * \returns Dummy buffer view handle
     */
    VkBufferView dummyBufferViewDescriptor() const {
      return m_unboundResources.bufferViewDescriptor();
    }
    
    /**
     * \brief Dummy sampler descriptor
     * \returns Descriptor that points to a dummy sampler
     */
    VkDescriptorImageInfo dummySamplerDescriptor() const {
      return m_unboundResources.samplerDescriptor();
    }
    
    /**
     * \brief Dummy image view descriptor
     * 
     * \param [in] type Required view type
     * \returns Descriptor that points to a dummy image
     */
    VkDescriptorImageInfo dummyImageViewDescriptor(VkImageViewType type) const {
      return m_unboundResources.imageViewDescriptor(type);
    }
    
  };
  
}