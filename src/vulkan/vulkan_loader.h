#pragma once

#include "../util/rc/util_rc.h"
#include "../util/rc/util_rc_ptr.h"

#define VK_USE_PLATFORM_WIN32_KHR 1
#include <vulkan/vulkan.h>

#define VULKAN_FN(name) \
  ::PFN_ ## name name = reinterpret_cast<::PFN_ ## name>(sym(#name))

using PFN_wine_vkAcquireKeyedMutex = VkResult (VKAPI_PTR *)(VkDevice device, VkDeviceMemory memory, uint64_t key, uint32_t timeout_ms);
using PFN_wine_vkReleaseKeyedMutex = VkResult (VKAPI_PTR *)(VkDevice device, VkDeviceMemory memory, uint64_t key);

namespace dxvk::vk {

  /**
   * \brief Vulkan library loader
   * 
   * Dynamically loads the vulkan-1 library and
   * provides methods to load Vulkan functions that
   * can be called before creating a instance.
   */
  struct LibraryLoader : public RcObject {
    LibraryLoader();
    LibraryLoader(PFN_vkGetInstanceProcAddr loaderProc);
    ~LibraryLoader();
    PFN_vkVoidFunction sym(VkInstance instance, const char* name) const;
    PFN_vkVoidFunction sym(const char* name) const;
    PFN_vkGetInstanceProcAddr getLoaderProc() const { return m_getInstanceProcAddr; }
    bool               valid() const;
  protected:
    HMODULE                   m_library             = nullptr;
    PFN_vkGetInstanceProcAddr m_getInstanceProcAddr = nullptr;
  };
  
  
  /**
   * \brief Vulkan instance loader
   * 
   * Loads Vulkan functions that can be
   * called for a specific instance.
   */
  struct InstanceLoader : public RcObject {
    InstanceLoader(const Rc<LibraryLoader>& library, bool owned, VkInstance instance);
    PFN_vkVoidFunction sym(const char* name) const;
    PFN_vkGetInstanceProcAddr getLoaderProc() const { return m_library->getLoaderProc(); }
    VkInstance instance() const { return m_instance; }
  protected:
    Rc<LibraryLoader> m_library;
    const VkInstance  m_instance;
    const bool        m_owned;
  };
  
  
  /**
   * \brief Vulkan device loader
   * 
   * Loads Vulkan functions for a
   * specific device.
   */
  struct DeviceLoader : public RcObject {
    DeviceLoader(const Rc<InstanceLoader>& library, bool owned, VkDevice device);
    PFN_vkVoidFunction sym(const char* name) const;
    VkDevice device() const { return m_device; }
  protected:
    Rc<InstanceLoader>            m_library;
    const PFN_vkGetDeviceProcAddr m_getDeviceProcAddr;
    const VkDevice                m_device;
    const bool                    m_owned;
  };
  
  
  /**
   * \brief Vulkan library functions
   * 
   * Vulkan functions that are called before
   * creating an actual Vulkan instance.
   */
  struct LibraryFn : LibraryLoader {
    LibraryFn();
    LibraryFn(PFN_vkGetInstanceProcAddr loaderProc);
    ~LibraryFn();
    
    VULKAN_FN(vkCreateInstance);
    VULKAN_FN(vkEnumerateInstanceLayerProperties);
    VULKAN_FN(vkEnumerateInstanceExtensionProperties);
  };
  
  
  /**
   * \brief Vulkan instance functions
   * 
   * Vulkan functions for a given instance that
   * are independent of any Vulkan devices.
   */
  struct InstanceFn : InstanceLoader {
    InstanceFn(const Rc<LibraryLoader>& library, bool owned, VkInstance instance);
    ~InstanceFn();
    
    VULKAN_FN(vkCreateDevice);
    VULKAN_FN(vkDestroyInstance);
    VULKAN_FN(vkEnumerateDeviceExtensionProperties);
    VULKAN_FN(vkEnumeratePhysicalDevices);
    VULKAN_FN(vkGetPhysicalDeviceExternalSemaphoreProperties);
    VULKAN_FN(vkGetPhysicalDeviceFeatures);
    VULKAN_FN(vkGetPhysicalDeviceFeatures2);
    VULKAN_FN(vkGetPhysicalDeviceFormatProperties);
    VULKAN_FN(vkGetPhysicalDeviceFormatProperties2);
    VULKAN_FN(vkGetPhysicalDeviceProperties2);
    VULKAN_FN(vkGetPhysicalDeviceImageFormatProperties);
    VULKAN_FN(vkGetPhysicalDeviceImageFormatProperties2);
    VULKAN_FN(vkGetPhysicalDeviceMemoryProperties);
    VULKAN_FN(vkGetPhysicalDeviceMemoryProperties2);
    VULKAN_FN(vkGetPhysicalDeviceProperties);
    VULKAN_FN(vkGetPhysicalDeviceQueueFamilyProperties);
    VULKAN_FN(vkGetPhysicalDeviceQueueFamilyProperties2);
    VULKAN_FN(vkGetPhysicalDeviceSparseImageFormatProperties);
    VULKAN_FN(vkGetPhysicalDeviceSparseImageFormatProperties2);

    #ifdef VK_KHR_get_surface_capabilities2
    VULKAN_FN(vkGetPhysicalDeviceSurfaceCapabilities2KHR);
    VULKAN_FN(vkGetPhysicalDeviceSurfaceFormats2KHR);
    #endif
    
    #ifdef VK_KHR_surface
    #ifdef VK_USE_PLATFORM_XCB_KHR
    VULKAN_FN(vkCreateXcbSurfaceKHR);
    VULKAN_FN(vkGetPhysicalDeviceXcbPresentationSupportKHR);
    #endif
    
    #ifdef VK_USE_PLATFORM_XLIB_KHR
    VULKAN_FN(vkCreateXlibSurfaceKHR);
    VULKAN_FN(vkGetPhysicalDeviceXlibPresentationSupportKHR);
    #endif
    
    #ifdef VK_USE_PLATFORM_WAYLAND_KHR
    VULKAN_FN(vkCreateWaylandSurfaceKHR);
    VULKAN_FN(vkGetPhysicalDeviceWaylandPresentationSupportKHR);
    #endif
    
    #ifdef VK_USE_PLATFORM_WIN32_KHR
    VULKAN_FN(vkCreateWin32SurfaceKHR);
    VULKAN_FN(vkGetPhysicalDeviceWin32PresentationSupportKHR);
    #endif
    
    VULKAN_FN(vkDestroySurfaceKHR);
    
    VULKAN_FN(vkGetPhysicalDeviceSurfaceSupportKHR);
    VULKAN_FN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    VULKAN_FN(vkGetPhysicalDeviceSurfaceFormatsKHR);
    VULKAN_FN(vkGetPhysicalDeviceSurfacePresentModesKHR);
    #endif
    
    #ifdef VK_EXT_debug_utils
    VULKAN_FN(vkCmdBeginDebugUtilsLabelEXT);
    VULKAN_FN(vkCmdEndDebugUtilsLabelEXT);
    VULKAN_FN(vkCmdInsertDebugUtilsLabelEXT);
    VULKAN_FN(vkCreateDebugUtilsMessengerEXT);
    VULKAN_FN(vkDestroyDebugUtilsMessengerEXT);
    VULKAN_FN(vkSubmitDebugUtilsMessageEXT);
    #endif

    #ifdef VK_EXT_full_screen_exclusive
    VULKAN_FN(vkGetPhysicalDeviceSurfacePresentModes2EXT);
    #endif

    #ifdef VK_EXT_swapchain_maintenance1
    VULKAN_FN(vkReleaseSwapchainImagesEXT);
    #endif
  };
  
  
  /**
   * \brief Vulkan device functions
   * 
   * Vulkan functions for a specific Vulkan device.
   * This ensures that no slow dispatch code is executed.
   */
  struct DeviceFn : DeviceLoader {
    DeviceFn(const Rc<InstanceLoader>& library, bool owned, VkDevice device);
    ~DeviceFn();
    
    VULKAN_FN(vkDestroyDevice);
    VULKAN_FN(vkGetDeviceQueue);
    VULKAN_FN(vkQueueSubmit);
    VULKAN_FN(vkQueueSubmit2);
    VULKAN_FN(vkQueueWaitIdle);
    VULKAN_FN(vkDeviceWaitIdle);
    VULKAN_FN(vkAllocateMemory);
    VULKAN_FN(vkFreeMemory);
    VULKAN_FN(vkMapMemory);
    VULKAN_FN(vkUnmapMemory);
    VULKAN_FN(vkFlushMappedMemoryRanges);
    VULKAN_FN(vkInvalidateMappedMemoryRanges);
    VULKAN_FN(vkGetDeviceMemoryCommitment);
    VULKAN_FN(vkBindBufferMemory);
    VULKAN_FN(vkBindImageMemory);
    VULKAN_FN(vkGetBufferMemoryRequirements);
    VULKAN_FN(vkGetBufferMemoryRequirements2);
    VULKAN_FN(vkGetImageMemoryRequirements);
    VULKAN_FN(vkGetImageMemoryRequirements2);
    VULKAN_FN(vkGetImageSparseMemoryRequirements);
    VULKAN_FN(vkGetImageSparseMemoryRequirements2);
    VULKAN_FN(vkQueueBindSparse);
    VULKAN_FN(vkCreateFence);
    VULKAN_FN(vkDestroyFence);
    VULKAN_FN(vkResetFences);
    VULKAN_FN(vkGetFenceStatus);
    VULKAN_FN(vkWaitForFences);
    VULKAN_FN(vkCreateSemaphore);
    VULKAN_FN(vkDestroySemaphore);
    VULKAN_FN(vkCreateEvent);
    VULKAN_FN(vkDestroyEvent);
    VULKAN_FN(vkGetEventStatus);
    VULKAN_FN(vkSetEvent);
    VULKAN_FN(vkResetEvent);
    VULKAN_FN(vkCreateQueryPool);
    VULKAN_FN(vkDestroyQueryPool);
    VULKAN_FN(vkGetQueryPoolResults);
    VULKAN_FN(vkCreateBuffer);
    VULKAN_FN(vkDestroyBuffer);
    VULKAN_FN(vkCreateBufferView);
    VULKAN_FN(vkDestroyBufferView);
    VULKAN_FN(vkCreateImage);
    VULKAN_FN(vkDestroyImage);
    VULKAN_FN(vkGetImageSubresourceLayout);
    VULKAN_FN(vkCreateImageView);
    VULKAN_FN(vkDestroyImageView);
    VULKAN_FN(vkCreateShaderModule);
    VULKAN_FN(vkDestroyShaderModule);
    VULKAN_FN(vkCreatePipelineCache);
    VULKAN_FN(vkDestroyPipelineCache);
    VULKAN_FN(vkGetPipelineCacheData);
    VULKAN_FN(vkMergePipelineCaches);
    VULKAN_FN(vkCreateGraphicsPipelines);
    VULKAN_FN(vkCreateComputePipelines);
    VULKAN_FN(vkDestroyPipeline);
    VULKAN_FN(vkCreatePipelineLayout);
    VULKAN_FN(vkDestroyPipelineLayout);
    VULKAN_FN(vkCreateSampler);
    VULKAN_FN(vkDestroySampler);
    VULKAN_FN(vkCreateDescriptorSetLayout);
    VULKAN_FN(vkDestroyDescriptorSetLayout);
    VULKAN_FN(vkCreateDescriptorPool);
    VULKAN_FN(vkDestroyDescriptorPool);
    VULKAN_FN(vkResetDescriptorPool);
    VULKAN_FN(vkAllocateDescriptorSets);
    VULKAN_FN(vkFreeDescriptorSets);
    VULKAN_FN(vkUpdateDescriptorSets);
    VULKAN_FN(vkCreateFramebuffer);
    VULKAN_FN(vkDestroyFramebuffer);
    VULKAN_FN(vkCreateRenderPass);
    VULKAN_FN(vkCreateRenderPass2);
    VULKAN_FN(vkDestroyRenderPass);
    VULKAN_FN(vkGetRenderAreaGranularity);
    VULKAN_FN(vkCreateCommandPool);
    VULKAN_FN(vkDestroyCommandPool);
    VULKAN_FN(vkResetCommandPool);
    VULKAN_FN(vkAllocateCommandBuffers);
    VULKAN_FN(vkFreeCommandBuffers);
    VULKAN_FN(vkBeginCommandBuffer);
    VULKAN_FN(vkEndCommandBuffer);
    VULKAN_FN(vkResetCommandBuffer);
    VULKAN_FN(vkCreateDescriptorUpdateTemplate);
    VULKAN_FN(vkDestroyDescriptorUpdateTemplate);
    VULKAN_FN(vkUpdateDescriptorSetWithTemplate);
    VULKAN_FN(vkResetQueryPool);
    VULKAN_FN(vkGetBufferDeviceAddress);
    VULKAN_FN(vkGetSemaphoreCounterValue);
    VULKAN_FN(vkSignalSemaphore);
    VULKAN_FN(vkWaitSemaphores);
    VULKAN_FN(vkCmdBindPipeline);
    VULKAN_FN(vkCmdSetViewport);
    VULKAN_FN(vkCmdSetScissor);
    VULKAN_FN(vkCmdSetLineWidth);
    VULKAN_FN(vkCmdSetDepthBias);
    VULKAN_FN(vkCmdSetDepthBias2EXT);
    VULKAN_FN(vkCmdSetBlendConstants);
    VULKAN_FN(vkCmdSetDepthBounds);
    VULKAN_FN(vkCmdSetStencilCompareMask);
    VULKAN_FN(vkCmdSetStencilWriteMask);
    VULKAN_FN(vkCmdSetStencilReference);
    VULKAN_FN(vkCmdBindVertexBuffers2);
    VULKAN_FN(vkCmdSetCullMode);
    VULKAN_FN(vkCmdSetDepthBoundsTestEnable);
    VULKAN_FN(vkCmdSetDepthCompareOp);
    VULKAN_FN(vkCmdSetDepthTestEnable);
    VULKAN_FN(vkCmdSetDepthWriteEnable);
    VULKAN_FN(vkCmdSetFrontFace);
    VULKAN_FN(vkCmdSetPrimitiveTopology);
    VULKAN_FN(vkCmdSetScissorWithCount);
    VULKAN_FN(vkCmdSetStencilOp);
    VULKAN_FN(vkCmdSetStencilTestEnable);
    VULKAN_FN(vkCmdSetViewportWithCount);
    VULKAN_FN(vkCmdSetRasterizerDiscardEnable);
    VULKAN_FN(vkCmdSetDepthBiasEnable);
    VULKAN_FN(vkCmdSetPrimitiveRestartEnable);
    VULKAN_FN(vkCmdBindDescriptorSets);
    VULKAN_FN(vkCmdBindIndexBuffer);
    VULKAN_FN(vkCmdBindVertexBuffers);
    VULKAN_FN(vkCmdDraw);
    VULKAN_FN(vkCmdDrawIndexed);
    VULKAN_FN(vkCmdDrawIndirect);
    VULKAN_FN(vkCmdDrawIndirectCount);
    VULKAN_FN(vkCmdDrawIndexedIndirect);
    VULKAN_FN(vkCmdDrawIndexedIndirectCount);
    VULKAN_FN(vkCmdDispatch);
    VULKAN_FN(vkCmdDispatchIndirect);
    VULKAN_FN(vkCmdCopyBuffer);
    VULKAN_FN(vkCmdCopyBuffer2);
    VULKAN_FN(vkCmdCopyImage);
    VULKAN_FN(vkCmdCopyImage2);
    VULKAN_FN(vkCmdBlitImage);
    VULKAN_FN(vkCmdBlitImage2);
    VULKAN_FN(vkCmdCopyBufferToImage);
    VULKAN_FN(vkCmdCopyBufferToImage2);
    VULKAN_FN(vkCmdCopyImageToBuffer);
    VULKAN_FN(vkCmdCopyImageToBuffer2);
    VULKAN_FN(vkCmdUpdateBuffer);
    VULKAN_FN(vkCmdFillBuffer);
    VULKAN_FN(vkCmdClearColorImage);
    VULKAN_FN(vkCmdClearDepthStencilImage);
    VULKAN_FN(vkCmdClearAttachments);
    VULKAN_FN(vkCmdResolveImage);
    VULKAN_FN(vkCmdResolveImage2);
    VULKAN_FN(vkCmdSetEvent);
    VULKAN_FN(vkCmdSetEvent2);
    VULKAN_FN(vkCmdResetEvent);
    VULKAN_FN(vkCmdResetEvent2);
    VULKAN_FN(vkCmdWaitEvents);
    VULKAN_FN(vkCmdWaitEvents2);
    VULKAN_FN(vkCmdPipelineBarrier);
    VULKAN_FN(vkCmdPipelineBarrier2);
    VULKAN_FN(vkCmdBeginQuery);
    VULKAN_FN(vkCmdEndQuery);
    VULKAN_FN(vkCmdResetQueryPool);
    VULKAN_FN(vkCmdWriteTimestamp);
    VULKAN_FN(vkCmdWriteTimestamp2);
    VULKAN_FN(vkCmdCopyQueryPoolResults);
    VULKAN_FN(vkCmdPushConstants);
    VULKAN_FN(vkCmdBeginRenderPass);
    VULKAN_FN(vkCmdBeginRenderPass2);
    VULKAN_FN(vkCmdNextSubpass);
    VULKAN_FN(vkCmdNextSubpass2);
    VULKAN_FN(vkCmdEndRenderPass);
    VULKAN_FN(vkCmdEndRenderPass2);
    VULKAN_FN(vkCmdBeginRendering);
    VULKAN_FN(vkCmdEndRendering);
    VULKAN_FN(vkCmdExecuteCommands);

    #ifdef VK_KHR_swapchain
    VULKAN_FN(vkCreateSwapchainKHR);
    VULKAN_FN(vkDestroySwapchainKHR);
    VULKAN_FN(vkGetSwapchainImagesKHR);
    VULKAN_FN(vkAcquireNextImageKHR);
    VULKAN_FN(vkQueuePresentKHR);
    #endif

    #ifdef VK_EXT_conditional_rendering
    VULKAN_FN(vkCmdBeginConditionalRenderingEXT);
    VULKAN_FN(vkCmdEndConditionalRenderingEXT);
    #endif

    #ifdef VK_EXT_debug_utils
    VULKAN_FN(vkQueueBeginDebugUtilsLabelEXT);
    VULKAN_FN(vkQueueEndDebugUtilsLabelEXT);
    VULKAN_FN(vkQueueInsertDebugUtilsLabelEXT);
    VULKAN_FN(vkSetDebugUtilsObjectNameEXT);
    VULKAN_FN(vkSetDebugUtilsObjectTagEXT);
    #endif

    #ifdef VK_EXT_extended_dynamic_state3
    VULKAN_FN(vkCmdSetTessellationDomainOriginEXT);
    VULKAN_FN(vkCmdSetDepthClampEnableEXT);
    VULKAN_FN(vkCmdSetPolygonModeEXT);
    VULKAN_FN(vkCmdSetRasterizationSamplesEXT);
    VULKAN_FN(vkCmdSetSampleMaskEXT);
    VULKAN_FN(vkCmdSetAlphaToCoverageEnableEXT);
    VULKAN_FN(vkCmdSetAlphaToOneEnableEXT);
    VULKAN_FN(vkCmdSetLogicOpEnableEXT);
    VULKAN_FN(vkCmdSetColorBlendEnableEXT);
    VULKAN_FN(vkCmdSetColorBlendEquationEXT);
    VULKAN_FN(vkCmdSetColorWriteMaskEXT);
    VULKAN_FN(vkCmdSetRasterizationStreamEXT);
    VULKAN_FN(vkCmdSetConservativeRasterizationModeEXT);
    VULKAN_FN(vkCmdSetExtraPrimitiveOverestimationSizeEXT);
    VULKAN_FN(vkCmdSetDepthClipEnableEXT);
    VULKAN_FN(vkCmdSetLineRasterizationModeEXT);
    #endif

    #ifdef VK_EXT_full_screen_exclusive
    VULKAN_FN(vkAcquireFullScreenExclusiveModeEXT);
    VULKAN_FN(vkReleaseFullScreenExclusiveModeEXT);
    VULKAN_FN(vkGetDeviceGroupSurfacePresentModes2EXT);
    #endif

    #ifdef VK_EXT_hdr_metadata
    VULKAN_FN(vkSetHdrMetadataEXT);
    #endif

    #ifdef VK_EXT_shader_module_identifier
    VULKAN_FN(vkGetShaderModuleCreateInfoIdentifierEXT);
    VULKAN_FN(vkGetShaderModuleIdentifierEXT);
    #endif

    #ifdef VK_EXT_transform_feedback
    VULKAN_FN(vkCmdBindTransformFeedbackBuffersEXT);
    VULKAN_FN(vkCmdBeginTransformFeedbackEXT);
    VULKAN_FN(vkCmdEndTransformFeedbackEXT);
    VULKAN_FN(vkCmdDrawIndirectByteCountEXT);
    VULKAN_FN(vkCmdBeginQueryIndexedEXT);
    VULKAN_FN(vkCmdEndQueryIndexedEXT);
    #endif

    #ifdef VK_NVX_image_view_handle
    VULKAN_FN(vkGetImageViewHandleNVX);
    VULKAN_FN(vkGetImageViewAddressNVX);
    #endif

    #ifdef VK_NVX_binary_import
    VULKAN_FN(vkCreateCuModuleNVX);
    VULKAN_FN(vkCreateCuFunctionNVX);
    VULKAN_FN(vkDestroyCuModuleNVX);
    VULKAN_FN(vkDestroyCuFunctionNVX);
    VULKAN_FN(vkCmdCuLaunchKernelNVX);
    #endif

    #ifdef VK_KHR_external_memory_win32
    VULKAN_FN(vkGetMemoryWin32HandleKHR);
    VULKAN_FN(vkGetMemoryWin32HandlePropertiesKHR);
    #endif

    #ifdef VK_KHR_external_semaphore_win32
    VULKAN_FN(vkGetSemaphoreWin32HandleKHR);
    VULKAN_FN(vkImportSemaphoreWin32HandleKHR);
    #endif

    #ifdef VK_KHR_maintenance5
    VULKAN_FN(vkCmdBindIndexBuffer2KHR);
    VULKAN_FN(vkGetRenderingAreaGranularityKHR);
    VULKAN_FN(vkGetDeviceImageSubresourceLayoutKHR);
    VULKAN_FN(vkGetImageSubresourceLayout2KHR);
    #endif

    #ifdef VK_KHR_present_wait
    VULKAN_FN(vkWaitForPresentKHR);
    #endif

    #ifdef VK_KHR_win32_keyed_mutex
    // Wine additions to actually use this extension.
    VULKAN_FN(wine_vkAcquireKeyedMutex);
    VULKAN_FN(wine_vkReleaseKeyedMutex);
    #endif
  };
  
}
