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
    VkPhysicalDeviceGraphicsPipelineLibraryPropertiesEXT      extGraphicsPipelineLibrary;
    VkPhysicalDeviceRobustness2PropertiesEXT                  extRobustness2;
    VkPhysicalDeviceTransformFeedbackPropertiesEXT            extTransformFeedback;
    VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT       extVertexAttributeDivisor;
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
    VkPhysicalDeviceAttachmentFeedbackLoopLayoutFeaturesEXT   extAttachmentFeedbackLoopLayout;
    VkPhysicalDeviceCustomBorderColorFeaturesEXT              extCustomBorderColor;
    VkPhysicalDeviceDepthClipEnableFeaturesEXT                extDepthClipEnable;
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT        extGraphicsPipelineLibrary;
    VkPhysicalDeviceMemoryPriorityFeaturesEXT                 extMemoryPriority;
    VkPhysicalDeviceNonSeamlessCubeMapFeaturesEXT             extNonSeamlessCubeMap;
    VkPhysicalDeviceRobustness2FeaturesEXT                    extRobustness2;
    VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT         extShaderModuleIdentifier;
    VkPhysicalDeviceTransformFeedbackFeaturesEXT              extTransformFeedback;
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT         extVertexAttributeDivisor;
  };

}