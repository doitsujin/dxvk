#pragma once

#include "dxvk_include.h"

namespace dxvk {

  /**
   * \brief Device info
   * 
   * Stores core properties and a bunch of extension-specific
   * properties, if the respective extensions are available.
   * Structures for unsupported extensions will be undefined,
   * so before using them, check whether they are supported.
   */
  struct DxvkDeviceInfo {
    VkPhysicalDeviceProperties2                               core;
    VkPhysicalDeviceVulkan11Properties                        vk11;
    VkPhysicalDeviceVulkan12Properties                        vk12;
    VkPhysicalDeviceVulkan13Properties                        vk13;
    VkPhysicalDeviceConservativeRasterizationPropertiesEXT    extConservativeRasterization;
    VkPhysicalDeviceCustomBorderColorPropertiesEXT            extCustomBorderColor;
    VkPhysicalDeviceExtendedDynamicState3PropertiesEXT        extExtendedDynamicState3;
    VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT      extGraphicsPipelineLibrary;
    VkPhysicalDeviceLineRasterizationPropertiesEXT            extLineRasterization;
    VkPhysicalDeviceRobustness2PropertiesEXT                  extRobustness2;
    VkPhysicalDeviceTransformFeedbackPropertiesEXT            extTransformFeedback;
    VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT       extVertexAttributeDivisor;
    VkPhysicalDeviceMaintenance5PropertiesKHR                 khrMaintenance5;
  };


  /**
   * \brief Device features
   * 
   * Stores core features and extension-specific features.
   * If the respective extensions are not available, the
   * extended features will be marked as unsupported.
   */
  struct DxvkDeviceFeatures {
    VkPhysicalDeviceFeatures2                                 core;
    VkPhysicalDeviceVulkan11Features                          vk11;
    VkPhysicalDeviceVulkan12Features                          vk12;
    VkPhysicalDeviceVulkan13Features                          vk13;
    VkBool32                                                  amdShaderFragmentMask;
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT   extAttachmentFeedbackLoopLayout;
    VkBool32                                                  extConservativeRasterization;
    VkPhysicalDeviceCustomBorderColorFeaturesEXT              extCustomBorderColor;
    VkPhysicalDeviceDepthClipEnableFeaturesEXT                extDepthClipEnable;
    VkPhysicalDeviceDepthBiasControlFeaturesEXT               extDepthBiasControl;
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT          extExtendedDynamicState3;
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT        extFragmentShaderInterlock;
    VkBool32                                                  extFullScreenExclusive;
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT        extGraphicsPipelineLibrary;
    VkBool32                                                  extHdrMetadata;
    VkPhysicalDeviceLineRasterizationFeaturesEXT              extLineRasterization;
    VkBool32                                                  extMemoryBudget;
    VkPhysicalDeviceMemoryPriorityFeaturesEXT                 extMemoryPriority;
    VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT             extNonSeamlessCubeMap;
    VkPhysicalDeviceRobustness2FeaturesEXT                    extRobustness2;
    VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT         extShaderModuleIdentifier;
    VkBool32                                                  extShaderStencilExport;
    VkBool32                                                  extSwapchainColorSpace;
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT          extSwapchainMaintenance1;
    VkPhysicalDeviceTransformFeedbackFeaturesEXT              extTransformFeedback;
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT         extVertexAttributeDivisor;
    VkBool32                                                  khrExternalMemoryWin32;
    VkBool32                                                  khrExternalSemaphoreWin32;
    VkPhysicalDeviceMaintenance5FeaturesKHR                   khrMaintenance5;
    VkPhysicalDevicePresentIdFeaturesKHR                      khrPresentId;
    VkPhysicalDevicePresentWaitFeaturesKHR                    khrPresentWait;
    VkPhysicalDeviceRawAccessChainsFeaturesNV                 nvRawAccessChains;
    VkBool32                                                  nvxBinaryImport;
    VkBool32                                                  nvxImageViewHandle;
    VkBool32                                                  khrWin32KeyedMutex;
  };

}