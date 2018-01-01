// *** THIS FILE IS GENERATED - DO NOT EDIT ***
// See loader_extension_generator.py for modifications

/*
 * Copyright (c) 2015-2017 The Khronos Group Inc.
 * Copyright (c) 2015-2017 Valve Corporation
 * Copyright (c) 2015-2017 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Mark Lobodzinski <mark@lunarg.com>
 * Author: Mark Young <marky@lunarg.com>
 */

#pragma once

typedef PFN_vkVoidFunction (VKAPI_PTR *PFN_GetPhysicalDeviceProcAddr)(VkInstance instance, const char* pName);

// Instance function pointer dispatch table
typedef struct VkLayerInstanceDispatchTable_ {
    // Manually add in GetPhysicalDeviceProcAddr entry
    PFN_GetPhysicalDeviceProcAddr GetPhysicalDeviceProcAddr;

    // ---- Core 1_0 commands
    PFN_vkCreateInstance CreateInstance;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures;
    PFN_vkGetPhysicalDeviceFormatProperties GetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceImageFormatProperties GetPhysicalDeviceImageFormatProperties;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
    PFN_vkCreateDevice CreateDevice;
    PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties;
    PFN_vkEnumerateInstanceLayerProperties EnumerateInstanceLayerProperties;
    PFN_vkEnumerateDeviceLayerProperties EnumerateDeviceLayerProperties;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties GetPhysicalDeviceSparseImageFormatProperties;

    // ---- VK_KHR_surface extension commands
    PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;

    // ---- VK_KHR_display extension commands
    PFN_vkGetPhysicalDeviceDisplayPropertiesKHR GetPhysicalDeviceDisplayPropertiesKHR;
    PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR GetPhysicalDeviceDisplayPlanePropertiesKHR;
    PFN_vkGetDisplayPlaneSupportedDisplaysKHR GetDisplayPlaneSupportedDisplaysKHR;
    PFN_vkGetDisplayModePropertiesKHR GetDisplayModePropertiesKHR;
    PFN_vkCreateDisplayModeKHR CreateDisplayModeKHR;
    PFN_vkGetDisplayPlaneCapabilitiesKHR GetDisplayPlaneCapabilitiesKHR;
    PFN_vkCreateDisplayPlaneSurfaceKHR CreateDisplayPlaneSurfaceKHR;

    // ---- VK_KHR_xlib_surface extension commands
#ifdef VK_USE_PLATFORM_XLIB_KHR
    PFN_vkCreateXlibSurfaceKHR CreateXlibSurfaceKHR;
#endif // VK_USE_PLATFORM_XLIB_KHR
#ifdef VK_USE_PLATFORM_XLIB_KHR
    PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR GetPhysicalDeviceXlibPresentationSupportKHR;
#endif // VK_USE_PLATFORM_XLIB_KHR

    // ---- VK_KHR_xcb_surface extension commands
#ifdef VK_USE_PLATFORM_XCB_KHR
    PFN_vkCreateXcbSurfaceKHR CreateXcbSurfaceKHR;
#endif // VK_USE_PLATFORM_XCB_KHR
#ifdef VK_USE_PLATFORM_XCB_KHR
    PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR GetPhysicalDeviceXcbPresentationSupportKHR;
#endif // VK_USE_PLATFORM_XCB_KHR

    // ---- VK_KHR_wayland_surface extension commands
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    PFN_vkCreateWaylandSurfaceKHR CreateWaylandSurfaceKHR;
#endif // VK_USE_PLATFORM_WAYLAND_KHR
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR GetPhysicalDeviceWaylandPresentationSupportKHR;
#endif // VK_USE_PLATFORM_WAYLAND_KHR

    // ---- VK_KHR_mir_surface extension commands
#ifdef VK_USE_PLATFORM_MIR_KHR
    PFN_vkCreateMirSurfaceKHR CreateMirSurfaceKHR;
#endif // VK_USE_PLATFORM_MIR_KHR
#ifdef VK_USE_PLATFORM_MIR_KHR
    PFN_vkGetPhysicalDeviceMirPresentationSupportKHR GetPhysicalDeviceMirPresentationSupportKHR;
#endif // VK_USE_PLATFORM_MIR_KHR

    // ---- VK_KHR_android_surface extension commands
#ifdef VK_USE_PLATFORM_ANDROID_KHR
    PFN_vkCreateAndroidSurfaceKHR CreateAndroidSurfaceKHR;
#endif // VK_USE_PLATFORM_ANDROID_KHR

    // ---- VK_KHR_win32_surface extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkCreateWin32SurfaceKHR CreateWin32SurfaceKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR GetPhysicalDeviceWin32PresentationSupportKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR

    // ---- VK_KHR_get_physical_device_properties2 extension commands
    PFN_vkGetPhysicalDeviceFeatures2KHR GetPhysicalDeviceFeatures2KHR;
    PFN_vkGetPhysicalDeviceProperties2KHR GetPhysicalDeviceProperties2KHR;
    PFN_vkGetPhysicalDeviceFormatProperties2KHR GetPhysicalDeviceFormatProperties2KHR;
    PFN_vkGetPhysicalDeviceImageFormatProperties2KHR GetPhysicalDeviceImageFormatProperties2KHR;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR GetPhysicalDeviceQueueFamilyProperties2KHR;
    PFN_vkGetPhysicalDeviceMemoryProperties2KHR GetPhysicalDeviceMemoryProperties2KHR;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR GetPhysicalDeviceSparseImageFormatProperties2KHR;

    // ---- VK_KHR_external_memory_capabilities extension commands
    PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR GetPhysicalDeviceExternalBufferPropertiesKHR;

    // ---- VK_KHR_external_semaphore_capabilities extension commands
    PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR GetPhysicalDeviceExternalSemaphorePropertiesKHR;

    // ---- VK_KHR_external_fence_capabilities extension commands
    PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR GetPhysicalDeviceExternalFencePropertiesKHR;

    // ---- VK_KHR_get_surface_capabilities2 extension commands
    PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR GetPhysicalDeviceSurfaceCapabilities2KHR;
    PFN_vkGetPhysicalDeviceSurfaceFormats2KHR GetPhysicalDeviceSurfaceFormats2KHR;

    // ---- VK_EXT_debug_report extension commands
    PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallbackEXT;
    PFN_vkDebugReportMessageEXT DebugReportMessageEXT;

    // ---- VK_NV_external_memory_capabilities extension commands
    PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV GetPhysicalDeviceExternalImageFormatPropertiesNV;

    // ---- VK_KHX_device_group extension commands
    PFN_vkGetPhysicalDevicePresentRectanglesKHX GetPhysicalDevicePresentRectanglesKHX;

    // ---- VK_NN_vi_surface extension commands
#ifdef VK_USE_PLATFORM_VI_NN
    PFN_vkCreateViSurfaceNN CreateViSurfaceNN;
#endif // VK_USE_PLATFORM_VI_NN

    // ---- VK_KHX_device_group_creation extension commands
    PFN_vkEnumeratePhysicalDeviceGroupsKHX EnumeratePhysicalDeviceGroupsKHX;

    // ---- VK_NVX_device_generated_commands extension commands
    PFN_vkGetPhysicalDeviceGeneratedCommandsPropertiesNVX GetPhysicalDeviceGeneratedCommandsPropertiesNVX;

    // ---- VK_EXT_direct_mode_display extension commands
    PFN_vkReleaseDisplayEXT ReleaseDisplayEXT;

    // ---- VK_EXT_acquire_xlib_display extension commands
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
    PFN_vkAcquireXlibDisplayEXT AcquireXlibDisplayEXT;
#endif // VK_USE_PLATFORM_XLIB_XRANDR_EXT
#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
    PFN_vkGetRandROutputDisplayEXT GetRandROutputDisplayEXT;
#endif // VK_USE_PLATFORM_XLIB_XRANDR_EXT

    // ---- VK_EXT_display_surface_counter extension commands
    PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT GetPhysicalDeviceSurfaceCapabilities2EXT;

    // ---- VK_MVK_ios_surface extension commands
#ifdef VK_USE_PLATFORM_IOS_MVK
    PFN_vkCreateIOSSurfaceMVK CreateIOSSurfaceMVK;
#endif // VK_USE_PLATFORM_IOS_MVK

    // ---- VK_MVK_macos_surface extension commands
#ifdef VK_USE_PLATFORM_MACOS_MVK
    PFN_vkCreateMacOSSurfaceMVK CreateMacOSSurfaceMVK;
#endif // VK_USE_PLATFORM_MACOS_MVK

    // ---- VK_EXT_sample_locations extension commands
    PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT GetPhysicalDeviceMultisamplePropertiesEXT;
} VkLayerInstanceDispatchTable;

// Device function pointer dispatch table
typedef struct VkLayerDispatchTable_ {

    // ---- Core 1_0 commands
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
    PFN_vkDestroyDevice DestroyDevice;
    PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkQueueWaitIdle QueueWaitIdle;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    PFN_vkAllocateMemory AllocateMemory;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkMapMemory MapMemory;
    PFN_vkUnmapMemory UnmapMemory;
    PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges;
    PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges;
    PFN_vkGetDeviceMemoryCommitment GetDeviceMemoryCommitment;
    PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkBindImageMemory BindImageMemory;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkGetImageSparseMemoryRequirements GetImageSparseMemoryRequirements;
    PFN_vkQueueBindSparse QueueBindSparse;
    PFN_vkCreateFence CreateFence;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkResetFences ResetFences;
    PFN_vkGetFenceStatus GetFenceStatus;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkCreateEvent CreateEvent;
    PFN_vkDestroyEvent DestroyEvent;
    PFN_vkGetEventStatus GetEventStatus;
    PFN_vkSetEvent SetEvent;
    PFN_vkResetEvent ResetEvent;
    PFN_vkCreateQueryPool CreateQueryPool;
    PFN_vkDestroyQueryPool DestroyQueryPool;
    PFN_vkGetQueryPoolResults GetQueryPoolResults;
    PFN_vkCreateBuffer CreateBuffer;
    PFN_vkDestroyBuffer DestroyBuffer;
    PFN_vkCreateBufferView CreateBufferView;
    PFN_vkDestroyBufferView DestroyBufferView;
    PFN_vkCreateImage CreateImage;
    PFN_vkDestroyImage DestroyImage;
    PFN_vkGetImageSubresourceLayout GetImageSubresourceLayout;
    PFN_vkCreateImageView CreateImageView;
    PFN_vkDestroyImageView DestroyImageView;
    PFN_vkCreateShaderModule CreateShaderModule;
    PFN_vkDestroyShaderModule DestroyShaderModule;
    PFN_vkCreatePipelineCache CreatePipelineCache;
    PFN_vkDestroyPipelineCache DestroyPipelineCache;
    PFN_vkGetPipelineCacheData GetPipelineCacheData;
    PFN_vkMergePipelineCaches MergePipelineCaches;
    PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
    PFN_vkCreateComputePipelines CreateComputePipelines;
    PFN_vkDestroyPipeline DestroyPipeline;
    PFN_vkCreatePipelineLayout CreatePipelineLayout;
    PFN_vkDestroyPipelineLayout DestroyPipelineLayout;
    PFN_vkCreateSampler CreateSampler;
    PFN_vkDestroySampler DestroySampler;
    PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout;
    PFN_vkCreateDescriptorPool CreateDescriptorPool;
    PFN_vkDestroyDescriptorPool DestroyDescriptorPool;
    PFN_vkResetDescriptorPool ResetDescriptorPool;
    PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
    PFN_vkFreeDescriptorSets FreeDescriptorSets;
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
    PFN_vkCreateFramebuffer CreateFramebuffer;
    PFN_vkDestroyFramebuffer DestroyFramebuffer;
    PFN_vkCreateRenderPass CreateRenderPass;
    PFN_vkDestroyRenderPass DestroyRenderPass;
    PFN_vkGetRenderAreaGranularity GetRenderAreaGranularity;
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkResetCommandPool ResetCommandPool;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
    PFN_vkFreeCommandBuffers FreeCommandBuffers;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkResetCommandBuffer ResetCommandBuffer;
    PFN_vkCmdBindPipeline CmdBindPipeline;
    PFN_vkCmdSetViewport CmdSetViewport;
    PFN_vkCmdSetScissor CmdSetScissor;
    PFN_vkCmdSetLineWidth CmdSetLineWidth;
    PFN_vkCmdSetDepthBias CmdSetDepthBias;
    PFN_vkCmdSetBlendConstants CmdSetBlendConstants;
    PFN_vkCmdSetDepthBounds CmdSetDepthBounds;
    PFN_vkCmdSetStencilCompareMask CmdSetStencilCompareMask;
    PFN_vkCmdSetStencilWriteMask CmdSetStencilWriteMask;
    PFN_vkCmdSetStencilReference CmdSetStencilReference;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
    PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer;
    PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
    PFN_vkCmdDraw CmdDraw;
    PFN_vkCmdDrawIndexed CmdDrawIndexed;
    PFN_vkCmdDrawIndirect CmdDrawIndirect;
    PFN_vkCmdDrawIndexedIndirect CmdDrawIndexedIndirect;
    PFN_vkCmdDispatch CmdDispatch;
    PFN_vkCmdDispatchIndirect CmdDispatchIndirect;
    PFN_vkCmdCopyBuffer CmdCopyBuffer;
    PFN_vkCmdCopyImage CmdCopyImage;
    PFN_vkCmdBlitImage CmdBlitImage;
    PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage;
    PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer;
    PFN_vkCmdUpdateBuffer CmdUpdateBuffer;
    PFN_vkCmdFillBuffer CmdFillBuffer;
    PFN_vkCmdClearColorImage CmdClearColorImage;
    PFN_vkCmdClearDepthStencilImage CmdClearDepthStencilImage;
    PFN_vkCmdClearAttachments CmdClearAttachments;
    PFN_vkCmdResolveImage CmdResolveImage;
    PFN_vkCmdSetEvent CmdSetEvent;
    PFN_vkCmdResetEvent CmdResetEvent;
    PFN_vkCmdWaitEvents CmdWaitEvents;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
    PFN_vkCmdBeginQuery CmdBeginQuery;
    PFN_vkCmdEndQuery CmdEndQuery;
    PFN_vkCmdResetQueryPool CmdResetQueryPool;
    PFN_vkCmdWriteTimestamp CmdWriteTimestamp;
    PFN_vkCmdCopyQueryPoolResults CmdCopyQueryPoolResults;
    PFN_vkCmdPushConstants CmdPushConstants;
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
    PFN_vkCmdNextSubpass CmdNextSubpass;
    PFN_vkCmdEndRenderPass CmdEndRenderPass;
    PFN_vkCmdExecuteCommands CmdExecuteCommands;

    // ---- VK_KHR_swapchain extension commands
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
    PFN_vkQueuePresentKHR QueuePresentKHR;

    // ---- VK_KHR_display_swapchain extension commands
    PFN_vkCreateSharedSwapchainsKHR CreateSharedSwapchainsKHR;

    // ---- VK_KHR_maintenance1 extension commands
    PFN_vkTrimCommandPoolKHR TrimCommandPoolKHR;

    // ---- VK_KHR_external_memory_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetMemoryWin32HandleKHR GetMemoryWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetMemoryWin32HandlePropertiesKHR GetMemoryWin32HandlePropertiesKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR

    // ---- VK_KHR_external_memory_fd extension commands
    PFN_vkGetMemoryFdKHR GetMemoryFdKHR;
    PFN_vkGetMemoryFdPropertiesKHR GetMemoryFdPropertiesKHR;

    // ---- VK_KHR_external_semaphore_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkImportSemaphoreWin32HandleKHR ImportSemaphoreWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetSemaphoreWin32HandleKHR GetSemaphoreWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR

    // ---- VK_KHR_external_semaphore_fd extension commands
    PFN_vkImportSemaphoreFdKHR ImportSemaphoreFdKHR;
    PFN_vkGetSemaphoreFdKHR GetSemaphoreFdKHR;

    // ---- VK_KHR_push_descriptor extension commands
    PFN_vkCmdPushDescriptorSetKHR CmdPushDescriptorSetKHR;

    // ---- VK_KHR_descriptor_update_template extension commands
    PFN_vkCreateDescriptorUpdateTemplateKHR CreateDescriptorUpdateTemplateKHR;
    PFN_vkDestroyDescriptorUpdateTemplateKHR DestroyDescriptorUpdateTemplateKHR;
    PFN_vkUpdateDescriptorSetWithTemplateKHR UpdateDescriptorSetWithTemplateKHR;
    PFN_vkCmdPushDescriptorSetWithTemplateKHR CmdPushDescriptorSetWithTemplateKHR;

    // ---- VK_KHR_shared_presentable_image extension commands
    PFN_vkGetSwapchainStatusKHR GetSwapchainStatusKHR;

    // ---- VK_KHR_external_fence_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkImportFenceWin32HandleKHR ImportFenceWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetFenceWin32HandleKHR GetFenceWin32HandleKHR;
#endif // VK_USE_PLATFORM_WIN32_KHR

    // ---- VK_KHR_external_fence_fd extension commands
    PFN_vkImportFenceFdKHR ImportFenceFdKHR;
    PFN_vkGetFenceFdKHR GetFenceFdKHR;

    // ---- VK_KHR_get_memory_requirements2 extension commands
    PFN_vkGetImageMemoryRequirements2KHR GetImageMemoryRequirements2KHR;
    PFN_vkGetBufferMemoryRequirements2KHR GetBufferMemoryRequirements2KHR;
    PFN_vkGetImageSparseMemoryRequirements2KHR GetImageSparseMemoryRequirements2KHR;

    // ---- VK_KHR_sampler_ycbcr_conversion extension commands
    PFN_vkCreateSamplerYcbcrConversionKHR CreateSamplerYcbcrConversionKHR;
    PFN_vkDestroySamplerYcbcrConversionKHR DestroySamplerYcbcrConversionKHR;

    // ---- VK_KHR_bind_memory2 extension commands
    PFN_vkBindBufferMemory2KHR BindBufferMemory2KHR;
    PFN_vkBindImageMemory2KHR BindImageMemory2KHR;

    // ---- VK_EXT_debug_marker extension commands
    PFN_vkDebugMarkerSetObjectTagEXT DebugMarkerSetObjectTagEXT;
    PFN_vkDebugMarkerSetObjectNameEXT DebugMarkerSetObjectNameEXT;
    PFN_vkCmdDebugMarkerBeginEXT CmdDebugMarkerBeginEXT;
    PFN_vkCmdDebugMarkerEndEXT CmdDebugMarkerEndEXT;
    PFN_vkCmdDebugMarkerInsertEXT CmdDebugMarkerInsertEXT;

    // ---- VK_AMD_draw_indirect_count extension commands
    PFN_vkCmdDrawIndirectCountAMD CmdDrawIndirectCountAMD;
    PFN_vkCmdDrawIndexedIndirectCountAMD CmdDrawIndexedIndirectCountAMD;

    // ---- VK_NV_external_memory_win32 extension commands
#ifdef VK_USE_PLATFORM_WIN32_KHR
    PFN_vkGetMemoryWin32HandleNV GetMemoryWin32HandleNV;
#endif // VK_USE_PLATFORM_WIN32_KHR

    // ---- VK_KHX_device_group extension commands
    PFN_vkGetDeviceGroupPeerMemoryFeaturesKHX GetDeviceGroupPeerMemoryFeaturesKHX;
    PFN_vkCmdSetDeviceMaskKHX CmdSetDeviceMaskKHX;
    PFN_vkCmdDispatchBaseKHX CmdDispatchBaseKHX;
    PFN_vkGetDeviceGroupPresentCapabilitiesKHX GetDeviceGroupPresentCapabilitiesKHX;
    PFN_vkGetDeviceGroupSurfacePresentModesKHX GetDeviceGroupSurfacePresentModesKHX;
    PFN_vkAcquireNextImage2KHX AcquireNextImage2KHX;

    // ---- VK_NVX_device_generated_commands extension commands
    PFN_vkCmdProcessCommandsNVX CmdProcessCommandsNVX;
    PFN_vkCmdReserveSpaceForCommandsNVX CmdReserveSpaceForCommandsNVX;
    PFN_vkCreateIndirectCommandsLayoutNVX CreateIndirectCommandsLayoutNVX;
    PFN_vkDestroyIndirectCommandsLayoutNVX DestroyIndirectCommandsLayoutNVX;
    PFN_vkCreateObjectTableNVX CreateObjectTableNVX;
    PFN_vkDestroyObjectTableNVX DestroyObjectTableNVX;
    PFN_vkRegisterObjectsNVX RegisterObjectsNVX;
    PFN_vkUnregisterObjectsNVX UnregisterObjectsNVX;

    // ---- VK_NV_clip_space_w_scaling extension commands
    PFN_vkCmdSetViewportWScalingNV CmdSetViewportWScalingNV;

    // ---- VK_EXT_display_control extension commands
    PFN_vkDisplayPowerControlEXT DisplayPowerControlEXT;
    PFN_vkRegisterDeviceEventEXT RegisterDeviceEventEXT;
    PFN_vkRegisterDisplayEventEXT RegisterDisplayEventEXT;
    PFN_vkGetSwapchainCounterEXT GetSwapchainCounterEXT;

    // ---- VK_GOOGLE_display_timing extension commands
    PFN_vkGetRefreshCycleDurationGOOGLE GetRefreshCycleDurationGOOGLE;
    PFN_vkGetPastPresentationTimingGOOGLE GetPastPresentationTimingGOOGLE;

    // ---- VK_EXT_discard_rectangles extension commands
    PFN_vkCmdSetDiscardRectangleEXT CmdSetDiscardRectangleEXT;

    // ---- VK_EXT_hdr_metadata extension commands
    PFN_vkSetHdrMetadataEXT SetHdrMetadataEXT;

    // ---- VK_EXT_sample_locations extension commands
    PFN_vkCmdSetSampleLocationsEXT CmdSetSampleLocationsEXT;

    // ---- VK_EXT_validation_cache extension commands
    PFN_vkCreateValidationCacheEXT CreateValidationCacheEXT;
    PFN_vkDestroyValidationCacheEXT DestroyValidationCacheEXT;
    PFN_vkMergeValidationCachesEXT MergeValidationCachesEXT;
    PFN_vkGetValidationCacheDataEXT GetValidationCacheDataEXT;
} VkLayerDispatchTable;


