#pragma once

#include <functional>
#include <optional>

#include "dxvk_device_info.h"
#include "dxvk_extensions.h"
#include "dxvk_include.h"
#include "dxvk_format.h"

namespace dxvk {
  
  class DxvkDevice;
  class DxvkInstance;

  using DxvkQueueCallback = std::function<void (bool)>;

  /**
   * \brief GPU vendors
   * Based on PCIe IDs.
   */
  enum class DxvkGpuVendor : uint16_t {
    Amd    = 0x1002,
    Nvidia = 0x10de,
    Intel  = 0x8086,
  };

  /**
   * \brief Adapter memory hfeap info
   * 
   * Stores info about a heap, and the amount
   * of memory allocated from it by the app.
   */
  struct DxvkAdapterMemoryHeapInfo {
    VkMemoryHeapFlags heapFlags;
    VkDeviceSize heapSize;
    VkDeviceSize memoryBudget;
    VkDeviceSize memoryAllocated;
  };

  /**
   * \brief Adapter memory info
   * 
   * Stores properties and allocation
   * info of each available heap.
   */
  struct DxvkAdapterMemoryInfo {
    uint32_t                  heapCount;
    DxvkAdapterMemoryHeapInfo heaps[VK_MAX_MEMORY_HEAPS];
  };

  /**
   * \brief Retrieves queue indices
   */
  struct DxvkAdapterQueueIndices {
    uint32_t graphics;
    uint32_t transfer;
    uint32_t sparse;
  };
  
  /**
   * \brief Device import info
   */
  struct DxvkDeviceImportInfo {
    VkDevice device;
    VkQueue queue;
    uint32_t queueFamily;
    uint32_t extensionCount;
    const char** extensionNames;
    const VkPhysicalDeviceFeatures2* features;
    DxvkQueueCallback queueCallback;
  };

  /**
   * \brief DXVK adapter
   * 
   * Corresponds to a physical device in Vulkan. Provides
   * all kinds of information about the device itself and
   * the supported feature set.
   */
  class DxvkAdapter : public RcObject {
    
  public:
    
    DxvkAdapter(
      const Rc<vk::InstanceFn>& vki,
            VkPhysicalDevice    handle);
    ~DxvkAdapter();
    
    /**
     * \brief Vulkan instance functions
     * \returns Vulkan instance functions
     */
    Rc<vk::InstanceFn> vki() const {
      return m_vki;
    }
    
    /**
     * \brief Physical device handle
     * \returns The adapter handle
     */
    VkPhysicalDevice handle() const {
      return m_handle;
    }
    
    /**
     * \brief Physical device properties
     * 
     * Returns a read-only reference to the core
     * properties of the Vulkan physical device.
     * \returns Physical device core properties
     */
    const VkPhysicalDeviceProperties& deviceProperties() const {
      return m_deviceInfo.core.properties;
    }

    /**
     * \brief Device info
     * 
     * Returns a read-only reference to the full
     * device info structure, including extended
     * properties.
     * \returns Device info struct
     */
    const DxvkDeviceInfo& devicePropertiesExt() const {
      return m_deviceInfo;
    }
    
    /**
     * \brief Supportred device features
     * 
     * Queries the supported device features.
     * \returns Device features
     */
    const DxvkDeviceFeatures& features() const {
      return m_deviceFeatures;
    }
    
    /**
     * \brief Retrieves memory heap info
     * 
     * Returns properties of all available memory heaps,
     * both device-local and non-local heaps, and the
     * amount of memory allocated from those heaps by
     * logical devices.
     * \returns Memory heap info
     */
    DxvkAdapterMemoryInfo getMemoryHeapInfo() const;
    
    /**
     * \brief Memory properties
     * 
     * Queries the memory types and memory heaps of
     * the device. This is useful for memory allocators.
     * \returns Device memory properties
     */
    VkPhysicalDeviceMemoryProperties memoryProperties() const;

    /**
     * \brief Queries format feature support
     *
     * \param [in] format Format to query
     * \returns Format feature bits
     */
    DxvkFormatFeatures getFormatFeatures(
            VkFormat                  format) const;

    /**
     * \brief Queries format limits
     *
     * \param [in] query Format query info
     * \returns Format limits if the given image is supported
     */
    std::optional<DxvkFormatLimits> getFormatLimits(
      const DxvkFormatQuery&          query) const;

    /**
     * \brief Retrieves queue family indices
     * \returns Indices for all queue families
     */
    DxvkAdapterQueueIndices findQueueFamilies() const;
    
    /**
     * \brief Tests whether all required features are supported
     * 
     * \param [in] features Required device features
     * \returns \c true if all features are supported
     */
    bool checkFeatureSupport(
      const DxvkDeviceFeatures& required) const;
    
    /**
     * \brief Enables extensions for this adapter
     *
     * When creating a device, all extensions that
     * are added using this method will be enabled
     * in addition to the ones required by DXVK.
     * This is used for OpenVR support.
     */
    void enableExtensions(
      const DxvkNameSet&        extensions);
    
    /**
     * \brief Creates a DXVK device
     * 
     * Creates a logical device for this adapter.
     * \param [in] instance Parent instance
     * \param [in] enabledFeatures Device features
     * \returns Device handle
     */
    Rc<DxvkDevice> createDevice(
      const Rc<DxvkInstance>&   instance,
            DxvkDeviceFeatures  enabledFeatures);
    
    /**
     * \brief Imports a foreign device
     * 
     * \param [in] instance Parent instance
     * \param [in] args Device import info
     * \returns Device handle
     */
    Rc<DxvkDevice> importDevice(
      const Rc<DxvkInstance>&   instance,
      const DxvkDeviceImportInfo& args);
    
    /**
     * \brief Registers heap memory allocation
     * 
     * Updates memory alloc info accordingly.
     * \param [in] heap Memory heap index
     * \param [in] bytes Allocation size
     */
    void notifyMemoryAlloc(
            uint32_t            heap,
            int64_t             bytes);
    
    /**
     * \brief Registers memory suballocation
     * 
     * Updates memory alloc info accordingly.
     * \param [in] heap Memory heap index
     * \param [in] bytes Allocation size
     */
    void notifyMemoryUse(
            uint32_t            heap,
            int64_t             bytes);
    
    /**
     * \brief Tests if the driver matches certain criteria
     *
     * \param [in] driver Driver ID
     * \param [in] minVer Match versions starting with this one
     * \param [in] maxVer Match versions lower than this one
     * \returns \c True if the driver matches these criteria
     */
    bool matchesDriver(
            VkDriverIdKHR       driver,
            uint32_t            minVer,
            uint32_t            maxVer) const;
    
    /**
     * \brief Logs DXVK adapter info
     * 
     * May be useful for bug reports
     * and general troubleshooting.
     */
    void logAdapterInfo() const;
    
    /**
     * \brief Checks whether this is a UMA system
     *
     * Basically tests whether all heaps are device-local.
     * Can be used for various optimizations in client APIs.
     * \returns \c true if the system has unified memory.
     */
    bool isUnifiedMemoryArchitecture() const;

    /**
     * \brief Registers a relationship with another GPU
     *
     * Used for display enumeration purposes.
     * \param [in] dgpu Dedicated GPU adatper
     */
    void linkToDGPU(Rc<DxvkAdapter> dgpu) {
        dgpu->m_linkedIGPUAdapter = this;
        m_linkedToDGPU = true;
    }

    /**
     * \brief Retrieves linked integrated GPU
     * \returns Integrated GPU adapter
     */
    Rc<DxvkAdapter> linkedIGPUAdapter() const {
        return m_linkedIGPUAdapter;
    }

    /**
     * \brief Checks whether the GPU is linked
     * \returns \c true if the GPU is linked
     */
    bool isLinkedToDGPU() const {
        return m_linkedToDGPU;
    }

  private:
    
    Rc<vk::InstanceFn>  m_vki;
    VkPhysicalDevice    m_handle;

    DxvkNameSet         m_extraExtensions;
    DxvkNameSet         m_deviceExtensions;
    DxvkDeviceInfo      m_deviceInfo;
    DxvkDeviceFeatures  m_deviceFeatures;

    bool                m_hasMemoryBudget;

    Rc<DxvkAdapter>     m_linkedIGPUAdapter;
    bool                m_linkedToDGPU = false;

    std::vector<VkQueueFamilyProperties> m_queueFamilies;

    std::array<std::atomic<uint64_t>, VK_MAX_MEMORY_HEAPS> m_memoryAllocated = { };
    std::array<std::atomic<uint64_t>, VK_MAX_MEMORY_HEAPS> m_memoryUsed = { };

    void queryExtensions();
    void queryDeviceInfo();
    void queryDeviceFeatures();
    void queryDeviceQueues();

    uint32_t findQueueFamily(
            VkQueueFlags          mask,
            VkQueueFlags          flags) const;
    
    std::vector<DxvkExt*> getExtensionList(
            DxvkDeviceExtensions&   devExtensions);

    static void initFeatureChain(
            DxvkDeviceFeatures&   enabledFeatures,
      const DxvkDeviceExtensions& devExtensions,
      const DxvkInstanceExtensions& insExtensions);

    static void logNameList(const DxvkNameList& names);
    static void logFeatures(const DxvkDeviceFeatures& features);
    static void logQueueFamilies(const DxvkAdapterQueueIndices& queues);
    
  };
  
}
