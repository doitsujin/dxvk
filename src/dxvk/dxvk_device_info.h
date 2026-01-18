#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dxvk_include.h"

#include "../util/util_version.h"

namespace dxvk {

  class DxvkInstance;

  /**
   * \brief Device info
   * 
   * Stores core properties and a bunch of extension-specific
   * properties, if the respective extensions are available.
   * Structures for unsupported extensions will be undefined,
   * so before using them, check whether they are supported.
   */
  struct DxvkDeviceInfo {
    Version                                                   driverVersion = { };
    VkPhysicalDeviceProperties2                               core                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    VkPhysicalDeviceVulkan11Properties                        vk11                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES };
    VkPhysicalDeviceVulkan12Properties                        vk12                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES };
    VkPhysicalDeviceVulkan13Properties                        vk13                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES };
    VkPhysicalDeviceConservativeRasterizationPropertiesEXT    extConservativeRasterization    = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT };
    VkPhysicalDeviceCustomBorderColorPropertiesEXT            extCustomBorderColor            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT };
    VkPhysicalDeviceDescriptorBufferPropertiesEXT             extDescriptorBuffer             = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT };
    VkPhysicalDeviceExtendedDynamicState3PropertiesEXT        extExtendedDynamicState3        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_PROPERTIES_EXT };
    VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT      extGraphicsPipelineLibrary      = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT };
    VkPhysicalDeviceLineRasterizationPropertiesEXT            extLineRasterization            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT };
    VkPhysicalDeviceMultiDrawPropertiesEXT                    extMultiDraw                    = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_PROPERTIES_EXT };
    VkPhysicalDeviceRobustness2PropertiesEXT                  extRobustness2                  = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT };
    VkPhysicalDeviceSampleLocationsPropertiesEXT              extSampleLocations              = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT };
    VkPhysicalDeviceTransformFeedbackPropertiesEXT            extTransformFeedback            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT };
    VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT       extVertexAttributeDivisor       = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT };
    VkPhysicalDeviceMaintenance5PropertiesKHR                 khrMaintenance5                 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES_KHR };
    VkPhysicalDeviceMaintenance6PropertiesKHR                 khrMaintenance6                 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_PROPERTIES_KHR };
    VkPhysicalDeviceMaintenance7PropertiesKHR                 khrMaintenance7                 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_7_PROPERTIES_KHR };
  };


  /**
   * \brief Device features
   * 
   * Stores core features and extension-specific features.
   * If the respective extensions are not available, the
   * extended features will be marked as unsupported.
   */
  struct DxvkDeviceFeatures {
    VkPhysicalDeviceFeatures2                                 core                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceVulkan11Features                          vk11                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    VkPhysicalDeviceVulkan12Features                          vk12                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };;
    VkPhysicalDeviceVulkan13Features                          vk13                            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };;
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT   extAttachmentFeedbackLoopLayout = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT };
    VkPhysicalDeviceBorderColorSwizzleFeaturesEXT             extBorderColorSwizzle           = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BORDER_COLOR_SWIZZLE_FEATURES_EXT };
    VkBool32                                                  extConservativeRasterization    = VK_FALSE;
    VkPhysicalDeviceCustomBorderColorFeaturesEXT              extCustomBorderColor            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT };
    VkPhysicalDeviceDepthClipEnableFeaturesEXT                extDepthClipEnable              = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT };
    VkPhysicalDeviceDepthBiasControlFeaturesEXT               extDepthBiasControl             = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT };
    VkPhysicalDeviceDescriptorBufferFeaturesEXT               extDescriptorBuffer             = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT          extExtendedDynamicState3        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT };
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT        extFragmentShaderInterlock      = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT };
    VkBool32                                                  extFullScreenExclusive          = VK_FALSE;
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT        extGraphicsPipelineLibrary      = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT };
    VkBool32                                                  extHdrMetadata                  = VK_FALSE;
    VkPhysicalDeviceLineRasterizationFeaturesEXT              extLineRasterization            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT };
    VkBool32                                                  extMemoryBudget                 = VK_FALSE;
    VkPhysicalDeviceMemoryPriorityFeaturesEXT                 extMemoryPriority               = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT };
    VkPhysicalDeviceMultiDrawFeaturesEXT                      extMultiDraw                    = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT };
    VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT             extNonSeamlessCubeMap           = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT };
    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT      extPageableDeviceLocalMemory    = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT };
    VkPhysicalDeviceRobustness2FeaturesEXT                    extRobustness2                  = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT };
    VkBool32                                                  extSampleLocations              = VK_FALSE;
    VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT         extShaderModuleIdentifier       = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT };
    VkBool32                                                  extShaderStencilExport          = VK_FALSE;
    VkBool32                                                  extSwapchainColorSpace          = VK_FALSE;
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT          extSwapchainMaintenance1        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT };
    VkPhysicalDeviceTransformFeedbackFeaturesEXT              extTransformFeedback            = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT };
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT         extVertexAttributeDivisor       = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES };
    VkBool32                                                  khrExternalMemoryWin32          = VK_FALSE;
    VkBool32                                                  khrExternalSemaphoreWin32       = VK_FALSE;
    VkBool32                                                  khrLoadStoreOpNone              = VK_FALSE;
    VkPhysicalDeviceMaintenance5FeaturesKHR                   khrMaintenance5                 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR };
    VkPhysicalDeviceMaintenance6FeaturesKHR                   khrMaintenance6                 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR };
    VkPhysicalDeviceMaintenance7FeaturesKHR                   khrMaintenance7                 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_7_FEATURES_KHR };
    VkBool32                                                  khrPipelineLibrary              = VK_FALSE;
    VkPhysicalDevicePresentIdFeaturesKHR                      khrPresentId                    = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR };
    VkPhysicalDevicePresentId2FeaturesKHR                     khrPresentId2                   = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_2_FEATURES_KHR };
    VkPhysicalDevicePresentWaitFeaturesKHR                    khrPresentWait                  = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR };
    VkPhysicalDevicePresentWait2FeaturesKHR                   khrPresentWait2                 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_2_FEATURES_KHR };
    VkPhysicalDeviceShaderFloatControls2FeaturesKHR           khrShaderFloatControls2         = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT_CONTROLS_2_FEATURES_KHR };
    VkBool32                                                  khrSwapchain                    = VK_FALSE;
    VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR          khrSwapchainMaintenance1        = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR };
    VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR            khrUnifiedImageLayouts          = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR };
    VkBool32                                                  khrSwapchainMutableFormat       = VK_FALSE;
    VkBool32                                                  khrWin32KeyedMutex              = VK_FALSE;
    VkBool32                                                  nvLowLatency2                   = VK_FALSE;
    VkPhysicalDeviceRawAccessChainsFeaturesNV                 nvRawAccessChains               = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAW_ACCESS_CHAINS_FEATURES_NV };
    VkBool32                                                  nvxBinaryImport                 = VK_FALSE;
    VkBool32                                                  nvxImageViewHandle              = VK_FALSE;
  };


  /**
   * \brief Device memory properties
   */
  struct DxvkDeviceMemoryInfo {
    VkPhysicalDeviceMemoryProperties2         core   = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 };
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
  };


  /**
   * \brief Vulkan extension info
   */
  struct DxvkDeviceExtensionInfo {
    VkExtensionProperties extAttachmentFeedbackLoopLayout   = vk::makeExtension(VK_EXT_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_EXTENSION_NAME);
    VkExtensionProperties extBorderColorSwizzle             = vk::makeExtension(VK_EXT_BORDER_COLOR_SWIZZLE_EXTENSION_NAME);
    VkExtensionProperties extConservativeRasterization      = vk::makeExtension(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);
    VkExtensionProperties extCustomBorderColor              = vk::makeExtension(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
    VkExtensionProperties extDepthClipEnable                = vk::makeExtension(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME);
    VkExtensionProperties extDepthBiasControl               = vk::makeExtension(VK_EXT_DEPTH_BIAS_CONTROL_EXTENSION_NAME);
    VkExtensionProperties extDescriptorBuffer               = vk::makeExtension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
    VkExtensionProperties extExtendedDynamicState3          = vk::makeExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
    VkExtensionProperties extFragmentShaderInterlock        = vk::makeExtension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME);
    VkExtensionProperties extFullScreenExclusive            = vk::makeExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
    VkExtensionProperties extGraphicsPipelineLibrary        = vk::makeExtension(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
    VkExtensionProperties extHdrMetadata                    = vk::makeExtension(VK_EXT_HDR_METADATA_EXTENSION_NAME);
    VkExtensionProperties extLineRasterization              = vk::makeExtension(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);
    VkExtensionProperties extMemoryBudget                   = vk::makeExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    VkExtensionProperties extMemoryPriority                 = vk::makeExtension(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
    VkExtensionProperties extMultiDraw                      = vk::makeExtension(VK_EXT_MULTI_DRAW_EXTENSION_NAME);
    VkExtensionProperties extNonSeamlessCubeMap             = vk::makeExtension(VK_EXT_NON_SEAMLESS_CUBE_MAP_EXTENSION_NAME);
    VkExtensionProperties extPageableDeviceLocalMemory      = vk::makeExtension(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME);
    VkExtensionProperties extRobustness2                    = vk::makeExtension(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
    VkExtensionProperties extSampleLocations                = vk::makeExtension(VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME);
    VkExtensionProperties extShaderModuleIdentifier         = vk::makeExtension(VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME);
    VkExtensionProperties extShaderStencilExport            = vk::makeExtension(VK_EXT_SHADER_STENCIL_EXPORT_EXTENSION_NAME);
    VkExtensionProperties extSwapchainColorSpace            = vk::makeExtension(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
    VkExtensionProperties extSwapchainMaintenance1          = vk::makeExtension(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    VkExtensionProperties extTransformFeedback              = vk::makeExtension(VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME);
    VkExtensionProperties extVertexAttributeDivisor         = vk::makeExtension(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);
    VkExtensionProperties khrExternalMemoryWin32            = vk::makeExtension(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
    VkExtensionProperties khrExternalSemaphoreWin32         = vk::makeExtension(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
    VkExtensionProperties khrLoadStoreOpNone                = vk::makeExtension(VK_KHR_LOAD_STORE_OP_NONE_EXTENSION_NAME);
    VkExtensionProperties khrMaintenance5                   = vk::makeExtension(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
    VkExtensionProperties khrMaintenance6                   = vk::makeExtension(VK_KHR_MAINTENANCE_6_EXTENSION_NAME);
    VkExtensionProperties khrMaintenance7                   = vk::makeExtension(VK_KHR_MAINTENANCE_7_EXTENSION_NAME);
    VkExtensionProperties khrPipelineLibrary                = vk::makeExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    VkExtensionProperties khrPresentId                      = vk::makeExtension(VK_KHR_PRESENT_ID_EXTENSION_NAME);
    VkExtensionProperties khrPresentId2                     = vk::makeExtension(VK_KHR_PRESENT_ID_2_EXTENSION_NAME);
    VkExtensionProperties khrPresentWait                    = vk::makeExtension(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
    VkExtensionProperties khrPresentWait2                   = vk::makeExtension(VK_KHR_PRESENT_WAIT_2_EXTENSION_NAME);
    VkExtensionProperties khrShaderFloatControls2           = vk::makeExtension(VK_KHR_SHADER_FLOAT_CONTROLS_2_EXTENSION_NAME);
    VkExtensionProperties khrSwapchain                      = vk::makeExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    VkExtensionProperties khrSwapchainMaintenance1          = vk::makeExtension(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    VkExtensionProperties khrSwapchainMutableFormat         = vk::makeExtension(VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME);
    VkExtensionProperties khrUnifiedImageLayouts            = vk::makeExtension(VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME);
    VkExtensionProperties khrWin32KeyedMutex                = vk::makeExtension(VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME);
    VkExtensionProperties nvLowLatency2                     = vk::makeExtension(VK_NV_LOW_LATENCY_2_EXTENSION_NAME);
    VkExtensionProperties nvRawAccessChains                 = vk::makeExtension(VK_NV_RAW_ACCESS_CHAINS_EXTENSION_NAME);
    VkExtensionProperties nvxBinaryImport                   = vk::makeExtension(VK_NVX_BINARY_IMPORT_EXTENSION_NAME);
    VkExtensionProperties nvxImageViewHandle                = vk::makeExtension(VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME);
  };


  /**
   * \brief Queue family and index
   */
  struct DxvkDeviceQueueIndex {
    uint32_t family = VK_QUEUE_FAMILY_IGNORED;
    uint32_t index  = 0u;
  };


  /**
   * \brief Queue mapping
   */
  struct DxvkDeviceQueueMapping {
    DxvkDeviceQueueIndex graphics;
    DxvkDeviceQueueIndex transfer;
    DxvkDeviceQueueIndex sparse;
  };


  /**
   * \brief Device capability info
   *
   * Stores supported extensions, features and device properties for any
   * given adapter and handles feature enablement for device creation.
   */
  class DxvkDeviceCapabilities {

  public:

    DxvkDeviceCapabilities(
      const DxvkInstance&               instance,
            VkPhysicalDevice            adapter,
      const VkDeviceCreateInfo*         deviceInfo);

    DxvkDeviceCapabilities(const DxvkDeviceCapabilities&) = delete;

    DxvkDeviceCapabilities& operator = (const DxvkDeviceCapabilities&) = delete;

    ~DxvkDeviceCapabilities();

    /**
     * \brief Queries device features
     * \returns Enabled or supported features
     */
    const auto& getFeatures() const {
      return m_featuresEnabled;
    }

    /**
     * \brief Queries device properties
     * \returns Device properties
     */
    const auto& getProperties() const {
      return m_properties;
    }

    /**
     * \brief Queries memory properties
     * \returns Device memory properties
     */
    const auto& getMemoryInfo() const {
      return m_memory;
    }

    /**
     * \brief Queries queue family mapping
     * \returns Assigned queue families and indices
     */
    DxvkDeviceQueueMapping getQueueMapping() const {
      return m_queueMapping;
    }

    /**
     * \brief Queries extensions to enable
     *
     * All returned extensions \e must be enabled when
     * using an external Vulkan device for DXVK.
     * \param [in,out] count Number of extensions
     * \param [out] extensions Extension properties
     * \returns \c true on success, \c false if the extension array is too small.
     */
    bool queryDeviceExtensions(
            uint32_t*                   count,
            VkExtensionProperties*      extensions) const;

    /**
     * \brief Queries queue create infos
     *
     * Writes an array of queues that can be used to create a Vulkan device
     * compatible with DXVK. Applications are free to add or remove queues
     * as they wish, however disabling queues may reduce performance, and at
     * least one queue \e must support both graphics and, compute operations.
     * For each written member of \c queues, if \c pQueuePriorities is non-null,
     * it must point to an array of \c queueCount floats that can be \e written.
     * As a result, this function needs to be called up to three times to query
     * queue properties.
     * \param [in,out] count Number of queues to enable
     * \param [out] queues Queue properties
     * \returns \c true on success, \c false if the queue array is too small.
     */
    bool queryDeviceQueues(
            uint32_t*                   count,
            VkDeviceQueueCreateInfo*    queues) const;

    /**
     * \brief Queries device features to enable
     *
     * Returns a blob of memory containing feature structs, led by a single
     * \c VkPhysicalDeviceFeatures2 structure at the start. The \c pNext
     * chain includes all feature structs that are both known to DXVK and
     * supported by the device. Applications can enable additional features
     * by scanning feature structs by their \c sType, and change the \c pNext
     * chain to insert feature structs that DXVK is not aware of.
     * \param [in,out] size Memory data size, in bytes
     * \param [out] data Pointer to meory blob of \c size bytes
     * \returns \c true on success, \c false if the blob at \c data is too small.
     */
    bool queryDeviceFeatures(
            size_t*                     size,
            void*                       data) const;

    /**
     * \brief Checks whether adapter supports all features
     *
     * \param [in] errorSize Size of error string
     * \param [out] error Optional pointer to descriptive error
     * \returns \c true if the device supports all required features
     *    and can be used with DXVK, \c false otherwise. If \c false,
     *    and if \c error is not null, a string describing which
     *    feature or extension is missing will be written there.
     */
    bool isSuitable(size_t errorSize, char* error);

    /**
     * \brief Logs all enabled extensions and features
     */
    void logDeviceInfo();

  private:

    struct FeatureEntry {
      VkExtensionProperties*  extensionSupported  = nullptr;
      VkExtensionProperties*  extensionEnabled    = nullptr;
      VkBool32*               featureSupported    = nullptr;
      VkBool32*               featureEnabled      = nullptr;
      VkBool32                featureRequired     = VK_FALSE;
      const char*             readableName        = nullptr;
    };

    DxvkDeviceInfo                        m_properties          = { };

    DxvkDeviceFeatures                    m_featuresSupported   = { };
    DxvkDeviceFeatures                    m_featuresEnabled     = { };

    DxvkDeviceExtensionInfo               m_extensionsSupported = { };
    DxvkDeviceExtensionInfo               m_extensionsEnabled   = { };

    DxvkDeviceMemoryInfo                  m_memory = { };

    DxvkDeviceQueueMapping                m_queueMapping = { };

    bool                                  m_hasMeshShader = false;
    bool                                  m_hasFmask = false;

    std::vector<const VkExtensionProperties*> m_extensionList;

    std::vector<VkQueueFamilyProperties2> m_queuesAvailable;
    std::vector<VkDeviceQueueCreateInfo>  m_queuesEnabled;
    std::vector<float>                    m_queuePriorities;

    void initSupportedExtensions(
      const DxvkInstance&               instance,
            VkPhysicalDevice            adapter,
      const VkDeviceCreateInfo*         deviceInfo);

    void initSupportedFeatures(
      const DxvkInstance&               instance,
            VkPhysicalDevice            adapter,
      const VkDeviceCreateInfo*         deviceInfo);

    void initDeviceProperties(
      const DxvkInstance&               instance,
            VkPhysicalDevice            adapter,
      const VkDeviceCreateInfo*         deviceInfo);

    void initQueueProperties(
      const DxvkInstance&               instance,
            VkPhysicalDevice            adapter,
      const VkDeviceCreateInfo*         deviceInfo);

    void initMemoryProperties(
      const DxvkInstance&               instance,
            VkPhysicalDevice            adapter);

    void disableUnusedFeatures(
      const DxvkInstance&               instance);

    void enableFeaturesAndExtensions();

    void enableQueues();

    void enableQueue(
            DxvkDeviceQueueIndex        queue);

    uint32_t findQueueFamily(
            VkQueueFlags                mask,
            VkQueueFlags                flags) const;

    std::optional<std::string> checkDeviceCompatibility();

    void chainFeatures(
      const DxvkDeviceExtensionInfo&    extensions,
            DxvkDeviceFeatures&         features);

    void chainProperties(
      const DxvkDeviceExtensionInfo&    extensions,
            DxvkDeviceInfo&             properties);

    std::vector<FeatureEntry> getFeatureList();

    static Version decodeDriverVersion(VkDriverId driverId, uint32_t version);

    template<typename T>
    static void copyFeature(
      const void*                       chain,
      const VkExtensionProperties*      extension,
            T*                          feature) {
      auto in = vk::scanChain(chain, feature->sType);

      if (in) {
        auto next = feature->pNext;

        *feature = *reinterpret_cast<const T*>(in);
        feature->pNext = next;
      }
    }

    static void copyFeature(
      const void*                       chain,
      const VkExtensionProperties*      extension,
            VkBool32*                   feature) {
      *feature = !extension || extension->specVersion;
    }

    template<typename T>
    static void chainFeature(
      const VkExtensionProperties*      extension,
            VkPhysicalDeviceFeatures2*  chain,
            T*                          feature) {
      if (!extension || extension->specVersion)
        feature->pNext = std::exchange(chain->pNext, feature);
    }

    static void chainFeature(
      const VkExtensionProperties*      extension,
            VkPhysicalDeviceFeatures2*  chain,
            VkBool32*                   feature) {
      if (!extension || extension->specVersion)
        *feature = VK_TRUE;
    }

    template<typename T>
    static void chainProperties(
      const VkExtensionProperties*      extension,
            VkPhysicalDeviceProperties2* chain,
            T*                          property) {
      if (!extension || extension->specVersion)
        property->pNext = std::exchange(chain->pNext, property);
    }

  };

}
