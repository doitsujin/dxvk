#pragma once

#include "dxvk_adapter.h"
#include "dxvk_buffer.h"
#include "dxvk_compute.h"
#include "dxvk_constant_state.h"
#include "dxvk_context.h"
#include "dxvk_fence.h"
#include "dxvk_framebuffer.h"
#include "dxvk_image.h"
#include "dxvk_instance.h"
#include "dxvk_latency.h"
#include "dxvk_memory.h"
#include "dxvk_meta_clear.h"
#include "dxvk_objects.h"
#include "dxvk_options.h"
#include "dxvk_pipemanager.h"
#include "dxvk_presenter.h"
#include "dxvk_queue.h"
#include "dxvk_recycler.h"
#include "dxvk_renderpass.h"
#include "dxvk_sampler.h"
#include "dxvk_shader.h"
#include "dxvk_sparse.h"
#include "dxvk_stats.h"
#include "dxvk_unbound.h"

namespace dxvk {
  
  class DxvkInstance;
  class DxvkShaderCache;

  class DxvkIrShader;
  class DxvkIrShaderConverter;
  class DxvkIrShaderCreateInfo;

  /**
   * \brief Device performance hints
   */
  struct DxvkDevicePerfHints {
    VkBool32 preferFbDepthStencilCopy   : 1;
    VkBool32 renderPassClearFormatBug   : 1;
    VkBool32 renderPassResolveFormatBug : 1;
    VkBool32 preferRenderPassOps        : 1;
    VkBool32 preferPrimaryCmdBufs       : 1;
  };
  
  /**
   * \brief Device queue
   * 
   * Stores a Vulkan queue and the
   * queue family that it belongs to.
   */
  struct DxvkDeviceQueue {
    VkQueue   queueHandle = VK_NULL_HANDLE;
    uint32_t  queueFamily = 0;
    uint32_t  queueIndex  = 0;
  };

  /**
   * \brief Device queue infos
   */
  struct DxvkDeviceQueueSet {
    DxvkDeviceQueue graphics;
    DxvkDeviceQueue transfer;
    DxvkDeviceQueue sparse;
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
  public:
    
    DxvkDevice(
      const Rc<DxvkInstance>&         instance,
      const Rc<DxvkAdapter>&          adapter,
      const Rc<vk::DeviceFn>&         vkd,
      const DxvkDeviceFeatures&       features,
      const DxvkDeviceQueueSet&       queues,
      const DxvkQueueCallback&        queueCallback);
      
    ~DxvkDevice();
    
    /**
     * \brief Vulkan device functions
     * \returns Vulkan device functions
     */
    Rc<vk::DeviceFn> vkd() const {
      return m_vkd;
    }
    
    /**
     * \brief Vulkan instance functions
     * \returns Vulkan instance functions
     */
    Rc<vk::InstanceFn> vki() const {
      return m_instance->vki();
    }
    
    /**
     * \brief Logical device handle
     * \returns The device handle
     */
    VkDevice handle() const {
      return m_vkd->device();
    }

    /**
     * \brief D3DKMT device local handle
     * \returns The device D3DKMT local handle
     * \returns \c 0 if there's no matching D3DKMT device
     */
    D3DKMT_HANDLE kmtLocal() const {
      return m_kmtLocal;
    }

    /**
     * \brief Checks whether debug functionality is enabled
     * \returns \c true if debug utils are enabled
     */
    DxvkDebugFlags debugFlags() const {
      return m_debugFlags;
    }

    /**
     * \brief Device options
     * \returns Device options
     */
    const DxvkOptions& config() const {
      return m_options;
    }
    
    /**
     * \brief Queue handles
     * 
     * Handles and queue family indices
     * of all known device queues.
     * \returns Device queue infos
     */
    const DxvkDeviceQueueSet& queues() const {
      return m_queues;
    }

    /**
     * \brief Tests whether a dedicated transfer queue is available
     * \returns \c true if an SDMA queue is supported by the device
     */
    bool hasDedicatedTransferQueue() const {
      return m_queues.transfer.queueHandle
          != m_queues.graphics.queueHandle;
    }

    /**
     * \brief Queries sharing mode info
     * \returns Sharing mode info
     */
    DxvkSharingModeInfo getSharingMode() const {
      DxvkSharingModeInfo result = { };
      result.queueFamilies[0] = m_queues.graphics.queueFamily;
      result.queueFamilies[1] = m_queues.transfer.queueFamily;
      return result;
    }

    /**
     * \brief The instance
     * 
     * The DXVK instance that created this device.
     * \returns Instance
     */
    Rc<DxvkInstance> instance() const {
      return m_instance;
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
     * \brief Enabled device features
     * \returns Enabled features
     */
    const DxvkDeviceFeatures& features() const {
      return m_features;
    }

    /**
     * \brief Device properties
     * \returns Device properties
     */
    const DxvkDeviceInfo& properties() const {
      return m_properties;
    }

    /**
     * \brief Queries format feature support
     *
     * \param [in] format Format to query
     * \returns Format feature bits
     */
    DxvkFormatFeatures getFormatFeatures(VkFormat format) const {
      return m_adapter->getFormatFeatures(format);
    }

    /**
     * \brief Queries format limits
     *
     * \param [in] query Format query info
     * \returns Format limits if the given image is supported
     */
    std::optional<DxvkFormatLimits> getFormatLimits(
      const DxvkFormatQuery&          query) const {
      return m_adapter->getFormatLimits(query);
    }


    /**
     * \brief Queries default shader compile options
     *
     * Can be overridden by the client API. Only applies to
     * shaders using internal IR rather than SPIR-V binaries.
     * \returns Device-global shader compile options.
     */
    DxvkShaderOptions getShaderCompileOptions() const {
      return m_shaderOptions;
    }

    /**
     * \brief Get device status
     * 
     * This may report device loss in
     * case a submission failed.
     * \returns Device status
     */
    VkResult getDeviceStatus() const {
      return m_submissionQueue.getLastError();
    }

    /**
     * \brief Queries mapped image subresource layout
     *
     * Assumes that the image tiling is linear even
     * if not explcitly set in the create info.
     * \param [in] createInfo Image create info
     * \param [in] subresource Subresource to query
     * \returns Subresource layout
     */
    VkSubresourceLayout queryImageSubresourceLayout(
      const DxvkImageCreateInfo&        createInfo,
      const VkImageSubresource&         subresource);

    /**
     * \brief Checks whether this is a UMA system
     *
     * Basically tests whether all heaps are device-local.
     * Can be used for various optimizations in client APIs.
     * \returns \c true if the system has unified memory.
     */
    bool isUnifiedMemoryArchitecture() const;

    /**
     * \brief Checks whether graphics pipeline libraries can be used
     * \returns \c true if all required features are supported.
     */
    bool canUseGraphicsPipelineLibrary() const;

    /**
     * \brief Checks whether pipeline creation cache control can be used
     * \returns \c true if all required features are supported.
     */
    bool canUsePipelineCacheControl() const;

    /**
     * \brief Checks whether pipelines should be tracked
     * \returns \c true if pipelines need to be tracked
     */
    bool mustTrackPipelineLifetime() const;

    /**
     * \brief Checks whether descriptor buffers can be used
     * \returns \c true if all required features are supported.
     */
    bool canUseDescriptorBuffer() const {
      return m_features.extDescriptorBuffer.descriptorBuffer;
    }

    /**
     * \brief Queries default framebuffer size
     * \returns Default framebuffer size
     */
    DxvkFramebufferSize getDefaultFramebufferSize() const;

    /**
     * \brief Queries supported shader stages
     * \returns Supported shader pipeline stages
     */
    VkPipelineStageFlags getShaderPipelineStages() const;

    /**
     * \brief Retrieves performance hints
     * \returns Device-specific perf hints
     */
    DxvkDevicePerfHints perfHints() const {
      return m_perfHints;
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
     * \brief Creates a GPU event
     * \returns New GPU event
     */
    Rc<DxvkEvent> createGpuEvent();

    /**
     * \brief Creates a query
     * 
     * \param [in] type Query type
     * \param [in] flags Query flags
     * \param [in] index Query index
     * \returns New query
     */
    Rc<DxvkQuery> createGpuQuery(
            VkQueryType           type,
            VkQueryControlFlags   flags,
            uint32_t              index);

    /**
     * \brief Creates a raw GPU query
     *
     * \param [in] type Query type
     * \returns New query
     */
    Rc<DxvkGpuQuery> createRawQuery(
            VkQueryType           type);

    /**
     * \brief Creates new fence
     *
     * \param [in] info Fence create info
     * \returns The fence
     */
    Rc<DxvkFence> createFence(
      const DxvkFenceCreateInfo& fenceInfo);
    
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
     * \brief Creates a sampler object
     * 
     * \param [in] createInfo Sampler parameters
     * \returns Newly created sampler object
     */
    Rc<DxvkSampler> createSampler(
      const DxvkSamplerKey&         createInfo);

    /**
     * \brief Creates local allocation cache
     *
     * \param [in] bufferUsage Required buffer usage
     * \param [in] propertyFlags Memory properties
     * \returns Allocation cache object
     */
    DxvkLocalAllocationCache createAllocationCache(
            VkBufferUsageFlags    bufferUsage,
            VkMemoryPropertyFlags propertyFlags);

    /**
     * \brief Creates a sparse page allocator
     * \returns Sparse page allocator
     */
    Rc<DxvkSparsePageAllocator> createSparsePageAllocator();

    /**
     * \brief Creates built-in pipeline layout
     *
     * \param [in] flags Pipeline layout flags
     * \param [in] pushDataStages Push data stage mask
     * \param [in] pushDataSize Push data size
     * \param [in] bindingCount Number of resource bindings
     * \param [in] bindings Resource bindings
     * \returns Unique pipeline layout
     */
    const DxvkPipelineLayout* createBuiltInPipelineLayout(
            DxvkPipelineLayoutFlags         flags,
            VkShaderStageFlags              pushDataStages,
            VkDeviceSize                    pushDataSize,
            uint32_t                        bindingCount,
      const DxvkDescriptorSetLayoutBinding* bindings);

    /**
     * \brief Creates built-in compute pipeline
     *
     * \param [in] layout Pipeline layout
     * \param [in] stage Shader stage info
     */
    VkPipeline createBuiltInComputePipeline(
      const DxvkPipelineLayout*             layout,
      const util::DxvkBuiltInShaderStage&   stage);

    /**
     * \brief Creates built-in graphics pipeline
     *
     * \param [in] layout Pipeline layout
     * \param [in] state Pipeline state
     */
    VkPipeline createBuiltInGraphicsPipeline(
      const DxvkPipelineLayout*             layout,
      const util::DxvkBuiltInGraphicsState& state);

    /**
     * \brief Creates IR shader from cache
     *
     * Will try to look up and retrive the given shader from
     * the shader cache. If no shader converter is provided
     * and the look-up fails, this returns \c nullptr.
     * \param [in] name Shader name
     * \param [in] createInfo Shader create info
     * \param [in] converter Shader converter to
     *    use when cache look-upo fails.
     */
    Rc<DxvkShader> createCachedShader(
      const std::string&                    name,
      const DxvkIrShaderCreateInfo&         createInfo,
      const Rc<DxvkIrShaderConverter>&      converter);

    /**
     * \brief Imports a buffer
     *
     * \param [in] createInfo Buffer create info
     * \param [in] importInfo Buffer import info
     * \param [in] memoryType Memory type flags
     * \returns The buffer object
     */
    Rc<DxvkBuffer> importBuffer(
      const DxvkBufferCreateInfo& createInfo,
      const DxvkBufferImportInfo& importInfo,
            VkMemoryPropertyFlags memoryType);

    /**
     * \brief Imports an image
     *
     * \param [in] createInfo Image create info
     * \param [in] image Vulkan image to wrap
     * \param [in] memoryType Memory type flags
     * \returns The image object
     */
    Rc<DxvkImage> importImage(
      const DxvkImageCreateInfo&  createInfo,
            VkImage               image,
            VkMemoryPropertyFlags memoryType);

    /**
     * \brief Retrieves stat counters
     * 
     * Can be used by the HUD to display some
     * internal information, such as memory
     * usage, draw calls, etc.
     */
    DxvkStatCounters getStatCounters();

    /**
     * \brief Queries memory statistics
     *
     * \param [in] heap Memory heap index
     * \returns Memory usage for this heap
     */
    DxvkMemoryStats getMemoryStats(uint32_t heap);

    /**
     * \brief Queries detailed memory allocation statistics
     *
     * Expensive, should be used with caution.
     * \param [out] stats Allocation statistics
     * \returns Shared allocation cache stats
     */
    DxvkSharedAllocationCacheStats getMemoryAllocationStats(DxvkMemoryAllocationStats& stats);

    /**
     * \brief Queries descriptor properties
     *
     * And null descriptors.
     */
    const DxvkDescriptorProperties& getDescriptorProperties() {
      return m_objects.descriptors();
    }

    /**
     * \brief Queries sampler statistics
     * \returns Sampler stats
     */
    DxvkSamplerStats getSamplerStats() {
      return m_objects.samplerPool().getStats();
    }

    /**
     * \brief Queries sampler descriptor set
     * \returns Global sampler set and layout
     */
    DxvkSamplerDescriptorSet getSamplerDescriptorSet() {
      return m_objects.samplerPool().getDescriptorSetInfo();
    }

    /**
     * \brief Queries sampler descriptor set
     * \returns Global sampler set and layout
     */
    DxvkDescriptorHeapBindingInfo getSamplerDescriptorHeap() {
      return m_objects.samplerPool().getDescriptorHeapInfo();
    }

    /**
     * \brief Retreves current frame ID
     * \returns Current frame ID
     */
    uint32_t getCurrentFrameId() const;
    
    /**
     * \brief Notifies adapter about memory allocation changes
     *
     * \param [in] heap Memory heap index
     * \param [in] allocated Allocated size delta
     * \param [in] used Used size delta
     */
    void notifyMemoryStats(
            uint32_t            heap,
            int64_t             allocated,
            int64_t             used) {
      m_adapter->notifyMemoryStats(heap, allocated, used);
    }

    /**
     * \brief Registers a shader
     * \param [in] shader Newly compiled shader
     */
    void registerShader(
      const Rc<DxvkShader>&         shader);
    
    /**
     * \brief Prioritizes compilation of a given shader
     * \param [in] shader Shader to start compiling
     */
    void requestCompileShader(
      const Rc<DxvkShader>&         shader);

    /**
     * \brief Creates latency tracker for a presenter
     *
     * The specicfic implementation and parameters used
     * depend on user configuration.
     * \param [in] presenter Presenter instance
     */
    Rc<DxvkLatencyTracker> createLatencyTracker(
      const Rc<Presenter>&            presenter);

    /**
     * \brief Presents a swap chain image
     * 
     * Invokes the presenter's \c presentImage method on
     * the submission thread. The status of this operation
     * can be retrieved with \ref waitForSubmission.
     * \param [in] presenter The presenter
     * \param [in] tracker Latency tracker
     * \param [in] frameId Frame ID
     * \param [out] status Present status
     */
    void presentImage(
      const Rc<Presenter>&            presenter,
      const Rc<DxvkLatencyTracker>&   tracker,
            uint64_t                  frameId,
            DxvkSubmitStatus*         status);
    
    /**
     * \brief Submits a command list
     * 
     * Submits the given command list to the device using
     * the given set of optional synchronization primitives.
     * \param [in] commandList The command list to submit
     * \param [in] tracker Latency tracker
     * \param [in] frameId Frame ID
     * \param [out] status Submission feedback
     */
    void submitCommandList(
      const Rc<DxvkCommandList>&      commandList,
      const Rc<DxvkLatencyTracker>&   tracker,
            uint64_t                  frameId,
            DxvkSubmitStatus*         status);

    /**
     * \brief Locks submission queue
     * 
     * Since Vulkan queues are only meant to be accessed
     * from one thread at a time, external libraries need
     * to lock the queue before submitting command buffers.
     */
    void lockSubmission() {
      m_submissionQueue.synchronize();
      m_submissionQueue.lockDeviceQueue();
    }
    
    /**
     * \brief Unlocks submission queue
     * 
     * Releases the Vulkan queues again so that DXVK
     * itself can use them for submissions again.
     */
    void unlockSubmission() {
      m_submissionQueue.unlockDeviceQueue();
    }

    /**
     * \brief Increments a given stat counter
     *
     * \param [in] counter Stat counter to increment
     * \param [in] value Increment value
     */
    void addStatCtr(DxvkStatCounter counter, uint64_t value) {
      std::lock_guard<sync::Spinlock> lock(m_statLock);
      m_statCounters.addCtr(counter, value);
    }

    /**
     * \brief Waits for a given submission
     * 
     * \param [in,out] status Submission status
     * \returns Result of the submission
     */
    VkResult waitForSubmission(DxvkSubmitStatus* status);

    /**
     * \brief Waits for a fence to become signaled
     *
     * Treats the fence wait as a GPU sync point, which can
     * be useful for device statistics. Should only be used
     * if rendering is stalled because of this wait.
     * \param [in] fence Fence to wait on
     * \param [in] value Fence value
     */
    void waitForFence(sync::Fence& fence, uint64_t value);

    /**
     * \brief Waits for resource to become idle
     *
     * \param [in] resource Resource to wait for
     * \param [in] access Access mode to check
     */
    void waitForResource(const DxvkPagedResource& resource, DxvkAccess access);
    
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

    Rc<DxvkInstance>            m_instance;
    Rc<DxvkAdapter>             m_adapter;
    Rc<vk::DeviceFn>            m_vkd;
    D3DKMT_HANDLE               m_kmtLocal = 0;

    DxvkDebugFlags              m_debugFlags;
    DxvkDeviceQueueSet          m_queues;

    DxvkDeviceFeatures          m_features;
    DxvkDeviceInfo              m_properties;

    DxvkShaderOptions           m_shaderOptions;

    DxvkDevicePerfHints         m_perfHints;
    DxvkObjects                 m_objects;

    sync::Spinlock              m_statLock;
    DxvkStatCounters            m_statCounters;

    DxvkRecycler<DxvkCommandList, 16> m_recycledCommandLists;

    DxvkSubmissionQueue         m_submissionQueue;

    Rc<DxvkShaderCache>         m_shaderCache;

    DxvkDevicePerfHints getPerfHints();

    void recycleCommandList(
      const Rc<DxvkCommandList>& cmdList);

    void determineShaderOptions();

  };
  
}
