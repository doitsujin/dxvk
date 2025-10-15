#pragma once

#include <functional>
#include <optional>

#include "dxvk_device_info.h"
#include "dxvk_extension_provider.h"
#include "dxvk_include.h"
#include "dxvk_format.h"

#include "../util/util_gdi.h"

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
   * \brief Adapter memory statistics
   *
   * Periodically updated by the devices using this adapter.
   */
  struct DxvkAdapterMemoryStats {
    std::atomic<uint64_t> allocated = { 0u };
    std::atomic<uint64_t> used = { 0u };
  };


  /**
   * \brief Device import info
   */
  struct DxvkDeviceImportInfo {
    VkDevice          device          = VK_NULL_HANDLE;
    VkQueue           queue           = VK_NULL_HANDLE;
    uint32_t          queueFamily     = VK_QUEUE_FAMILY_IGNORED;
    uint32_t          extensionCount  = 0u;
    const char**      extensionNames  = nullptr;
    const VkPhysicalDeviceFeatures2* features = nullptr;
    DxvkQueueCallback queueCallback   = { };
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
            DxvkInstance&       instance,
            VkPhysicalDevice    handle);
    ~DxvkAdapter();
    
    /**
     * \brief Vulkan instance functions
     * \returns Vulkan instance functions
     */
    Rc<vk::InstanceFn> vki() const;
    
    /**
     * \brief Physical device handle
     * \returns The adapter handle
     */
    VkPhysicalDevice handle() const {
      return m_handle;
    }
    
    /**
     * \brief D3DKMT adapter local handle
     * \returns The adapter D3DKMT local handle
     * \returns \c 0 if there's no matching D3DKMT adapter
     */
    D3DKMT_HANDLE kmtLocal() const {
      return m_kmtLocal;
    }
    
    /**
     * \brief Physical device properties
     * 
     * Returns a read-only reference to the core
     * properties of the Vulkan physical device.
     * \returns Physical device core properties
     */
    const DxvkDeviceInfo& deviceProperties() const {
      return m_capabilities.getProperties();
    }
    
    /**
     * \brief Supportred device features
     * 
     * Queries the supported device features.
     * \returns Device features
     */
    const DxvkDeviceFeatures& features() const {
      return m_capabilities.getFeatures();
    }
    
    /**
     * \brief Memory properties
     *
     * Queries the memory types and memory heaps of
     * the device. This is useful for memory allocators.
     * \returns Device memory properties
     */
    const VkPhysicalDeviceMemoryProperties& memoryProperties() const {
      return m_capabilities.getMemoryInfo().core.memoryProperties;
    }

    /**
     * \brief Checks whether the adapter is usable for DXVK
     *
     * \param [out] error Detailed error message on error
     * \returns \c true if the adapter supports required features
     */
    bool isCompatible(std::string& error);

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
      const DxvkExtensionList&  extensions);
    
    /**
     * \brief Creates a DXVK device
     * 
     * Creates a logical device for this adapter.
     * \returns Device handle
     */
    Rc<DxvkDevice> createDevice();
    
    /**
     * \brief Imports a foreign device
     * 
     * \param [in] args Device import info
     * \returns Device handle
     */
    Rc<DxvkDevice> importDevice(
      const DxvkDeviceImportInfo& args);
    
    /**
     * \brief Registers heap memory allocation
     * 
     * Updates memory alloc info accordingly.
     * \param [in] heap Memory heap index
     * \param [in] allocated Allocated size delta
     * \param [in] used Used size delta
     */
    void notifyMemoryStats(
            uint32_t            heap,
            int64_t             allocated,
            int64_t             used);
    
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
            Version             minVer,
            Version             maxVer) const;

    /**
     * \brief Tests if the driver matches certain criteria
     *
     * \param [in] driver Driver ID
     * \returns \c True if the driver matches these criteria
     */
    bool matchesDriver(
            VkDriverIdKHR       driver) const;
    
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
    
    DxvkInstance*           m_instance  = nullptr;
    VkPhysicalDevice        m_handle    = VK_NULL_HANDLE;
    D3DKMT_HANDLE           m_kmtLocal = 0;

    DxvkDeviceCapabilities  m_capabilities;

    std::vector<VkExtensionProperties> m_extraExtensions;

    Rc<DxvkAdapter>     m_linkedIGPUAdapter;
    bool                m_linkedToDGPU = false;

    std::array<DxvkAdapterMemoryStats, VK_MAX_MEMORY_HEAPS> m_memoryStats = { };

  };
  
}
