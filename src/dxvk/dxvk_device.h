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
#include "dxvk_meta_clear.h"
#include "dxvk_options.h"
#include "dxvk_pipecache.h"
#include "dxvk_pipemanager.h"
#include "dxvk_queue.h"
#include "dxvk_query_pool.h"
#include "dxvk_recycler.h"
#include "dxvk_renderpass.h"
#include "dxvk_sampler.h"
#include "dxvk_shader.h"
#include "dxvk_stats.h"
#include "dxvk_unbound.h"

#include "../vulkan/vulkan_presenter.h"

namespace dxvk {
  
  class DxvkInstance;

  /**
   * \brief Device options
   */
  struct DxvkDeviceOptions {
    uint32_t maxNumDynamicUniformBuffers = 0;
    uint32_t maxNumDynamicStorageBuffers = 0;
  };
  
  /**
   * \brief Device queue
   * 
   * Stores a Vulkan queue and the
   * queue family that it belongs to.
   */
  struct DxvkDeviceQueue {
    uint32_t  queueFamily = 0;
    VkQueue   queueHandle = VK_NULL_HANDLE;
  };
  
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
    friend class DxvkDescriptorPoolTracker;
    
    constexpr static VkDeviceSize DefaultStagingBufferSize = 4 * 1024 * 1024;
  public:
    
    DxvkDevice(
      const Rc<DxvkAdapter>&          adapter,
      const Rc<vk::DeviceFn>&         vkd,
      const DxvkDeviceExtensions&     extensions,
      const DxvkDeviceFeatures&       features);
      
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
     * \brief Device options
     * \returns Device options
     */
    const DxvkOptions& config() const {
      return m_options;
    }
    
    /**
     * \brief Graphics queue properties
     * 
     * Handle and queue family index of
     * the queue used for rendering.
     * \returns Graphics queue info
     */
    DxvkDeviceQueue graphicsQueue() const {
      return m_graphicsQueue;
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
     * \brief Enabled device extensions
     * \returns Enabled device extensions
     */
    const DxvkDeviceExtensions& extensions() const {
      return m_extensions;
    }
    
    /**
     * \brief Enabled device features
     * \returns Enabled features
     */
    const DxvkDeviceFeatures& features() const {
      return m_features;
    }
    
    /**
     * \brief Retrieves device options
     * \returns Device options
     */
    DxvkDeviceOptions options() const;
    
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
     * \brief Creates a descriptor pool
     * 
     * Returns a previously recycled pool, or creates
     * a new one if necessary. The context should take
     * ownership of the returned pool.
     * \returns Descriptor pool
     */
    Rc<DxvkDescriptorPool> createDescriptorPool();
    
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
     * \brief Creates a sampler object
     * 
     * \param [in] createInfo Sampler parameters
     * \returns Newly created sampler object
     */
    Rc<DxvkSampler> createSampler(
      const DxvkSamplerCreateInfo&  createInfo);
    
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
     * \brief Retrieves stat counters
     * 
     * Can be used by the HUD to display some
     * internal information, such as memory
     * usage, draw calls, etc.
     */
    DxvkStatCounters getStatCounters();

    /**
     * \brief Retreves current frame ID
     * \returns Current frame ID
     */
    uint32_t getCurrentFrameId() const;
    
    /**
     * \brief Initializes dummy resources
     * 
     * Should be called after creating the device in
     * case the device initialization was successful
     * and the device is usable.
     */
    void initResources();
    
    /**
     * \brief Registers a shader
     * \param [in] shader Newly compiled shader
     */
    void registerShader(
      const Rc<DxvkShader>&         shader);
    
    /**
     * \brief Presents a swap chain image
     * 
     * Locks the device queues and invokes the
     * presenter's \c presentImage method.
     * \param [in] presenter The presenter
     * \param [in] semaphore Sync semaphore
     */
    VkResult presentImage(
      const Rc<vk::Presenter>&        presenter,
            VkSemaphore               semaphore);
    
    /**
     * \brief Submits a command list
     * 
     * Synchronization arguments are optional. 
     * \param [in] commandList The command list to submit
     * \param [in] waitSync (Optional) Semaphore to wait on
     * \param [in] wakeSync (Optional) Semaphore to notify
     * \returns Synchronization fence
     */
    void submitCommandList(
      const Rc<DxvkCommandList>&      commandList,
            VkSemaphore               waitSync,
            VkSemaphore               wakeSync);
    
    /**
     * \brief Locks submission queue
     * 
     * Since Vulkan queues are only meant to be accessed
     * from one thread at a time, external libraries need
     * to lock the queue before submitting command buffers.
     */
    void lockSubmission() {
      m_submissionLock.lock();
    }
    
    /**
     * \brief Unlocks submission queue
     * 
     * Releases the Vulkan queues again so that DXVK
     * itself can use them for submissions again.
     */
    void unlockSubmission() {
      m_submissionLock.unlock();
    }

    /**
     * \brief Number of pending submissions
     * 
     * A return value of 0 indicates
     * that the GPU is currently idle.
     * \returns Pending submission count
     */
    uint32_t pendingSubmissions() const {
      return m_submissionQueue.pendingSubmissions();
    }
    
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
    
    DxvkOptions                 m_options;

    Rc<DxvkAdapter>             m_adapter;
    Rc<vk::DeviceFn>            m_vkd;
    DxvkDeviceExtensions        m_extensions;

    DxvkDeviceFeatures          m_features;
    VkPhysicalDeviceProperties  m_properties;
    
    Rc<DxvkMemoryAllocator>     m_memory;
    Rc<DxvkRenderPassPool>      m_renderPassPool;
    Rc<DxvkPipelineManager>     m_pipelineManager;

    Rc<DxvkMetaClearObjects>    m_metaClearObjects;
    Rc<DxvkMetaCopyObjects>     m_metaCopyObjects;
    Rc<DxvkMetaMipGenObjects>   m_metaMipGenObjects;
    Rc<DxvkMetaPackObjects>     m_metaPackObjects;
    Rc<DxvkMetaResolveObjects>  m_metaResolveObjects;
    
    DxvkUnboundResources        m_unboundResources;
    
    sync::Spinlock              m_statLock;
    DxvkStatCounters            m_statCounters;
    
    std::mutex                  m_submissionLock;
    DxvkDeviceQueue             m_graphicsQueue;
    DxvkDeviceQueue             m_presentQueue;
    
    DxvkRecycler<DxvkCommandList,    16> m_recycledCommandLists;
    DxvkRecycler<DxvkDescriptorPool, 16> m_recycledDescriptorPools;
    DxvkRecycler<DxvkStagingBuffer,   4> m_recycledStagingBuffers;
    
    DxvkSubmissionQueue m_submissionQueue;
    
    void recycleCommandList(
      const Rc<DxvkCommandList>& cmdList);
    
    void recycleDescriptorPool(
      const Rc<DxvkDescriptorPool>& pool);
    
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