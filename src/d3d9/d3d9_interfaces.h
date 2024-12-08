#pragma once

#include "d3d9_include.h"
#include "../vulkan/vulkan_loader.h"

/**
 * \brief D3D9 interface for Vulkan interop
 *
 * Provides access to the instance and physical device
 * handles for the given D3D9 interface and adapter ordinals.
 */
MIDL_INTERFACE("3461a81b-ce41-485b-b6b5-fcf08ba6a6bd")
ID3D9VkInteropInterface : public IUnknown {
  /**
   * \brief Queries Vulkan handles used by DXVK
   * 
   * \param [out] pInstance The Vulkan instance
   */
  virtual void STDMETHODCALLTYPE GetInstanceHandle(
          VkInstance*           pInstance) = 0;

  /**
   * \brief Queries Vulkan handles used by DXVK
   * 
   * \param [in] Adapter Adapter ordinal
   * \param [out] pInstance The Vulkan instance
   */
  virtual void STDMETHODCALLTYPE GetPhysicalDeviceHandle(
          UINT                  Adapter,
          VkPhysicalDevice*     pPhysicalDevice) = 0;
};

/**
 * \brief D3D9 interface for Vulkan interop - extended
 *
 * Provides access to the instance extension lists
 * and everything provided by ID3D9VkInteropInterface
 */
MIDL_INTERFACE("d6589ed4-7a37-4096-bac2-223b25ae31d2")
ID3D9VkInteropInterface1 : public ID3D9VkInteropInterface {
  /**
   * \brief Gets a list of enabled instance extensions
   * 
   * \param [out] pExtensionCount Number of extensions
   * \param [out] ppExtensions List of extension names
   * \returns D3DERR_MOREDATA if the list was truncated
   */
  virtual HRESULT STDMETHODCALLTYPE GetInstanceExtensions(
          UINT*                       pExtensionCount,
    const char**                      ppExtensions) = 0;
};

/**
 * \brief D3D9 texture interface for Vulkan interop
 * 
 * Provides access to the backing image of a
 * D3D9 texture, surface, or volume.
 */
MIDL_INTERFACE("d56344f5-8d35-46fd-806d-94c351b472c1")
ID3D9VkInteropTexture : public IUnknown {
  /**
   * \brief Retrieves Vulkan image info
   * 
   * Retrieves both the image handle as well as the image's
   * properties. Any of the given pointers may be \c nullptr.
   * 
   * If \c pInfo is not \c nullptr, the following rules apply:
   * - \c pInfo->sType \e must be \c VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
   * - \c pInfo->pNext \e must be \c nullptr or point to a supported
   *   extension-specific structure (currently none)
   * - \c pInfo->queueFamilyIndexCount must be the length of the
   *   \c pInfo->pQueueFamilyIndices array, in \c uint32_t units.
   * - \c pInfo->pQueueFamilyIndices must point to a pre-allocated
   *   array of \c uint32_t of size \c pInfo->pQueueFamilyIndices.
   * 
   * \note As of now, the sharing mode will always be
   *       \c VK_SHARING_MODE_EXCLUSIVE and no queue
   *       family indices will be written to the array.
   * 
   * After the call, the structure pointed to by \c pInfo can
   * be used to create an image with identical properties.
   * 
   * If \c pLayout is not \c nullptr, it will receive the
   * layout that the image will be in after flushing any
   * outstanding commands on the device.
   * \param [out] pHandle The image handle
   * \param [out] pLayout Image layout
   * \param [out] pInfo Image properties
   * \returns \c S_OK on success, or \c D3DERR_INVALIDCALL
   */
  virtual HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(
          VkImage*              pHandle,
          VkImageLayout*        pLayout,
          VkImageCreateInfo*    pInfo) = 0;
};


/**
 * \brief D3D9 image description
 */
struct D3D9VkExtImageDesc {
  D3DRESOURCETYPE     Type;               // Can be SURFACE, TEXTURE, CUBETEXTURE, VOLUMETEXTURE
  UINT                Width;
  UINT                Height;
  UINT                Depth;              // Can be > 1 for VOLUMETEXTURE
  UINT                MipLevels;          // Can be > 1 for TEXTURE, CUBETEXTURE, VOLUMETEXTURE
  DWORD               Usage;
  D3DFORMAT           Format;
  D3DPOOL             Pool;
  D3DMULTISAMPLE_TYPE MultiSample;        // Must be NONE unless Type is SURFACE
  DWORD               MultiSampleQuality;
  bool                Discard;            // Depth stencils only
  bool                IsAttachmentOnly;   // If false, then VK_IMAGE_USAGE_SAMPLED_BIT will be added
  bool                IsLockable;
  VkImageUsageFlags   ImageUsage;         // Additional image usage flags
};

/**
 * \brief D3D9 device interface for Vulkan interop
 *
 * Provides access to the device and instance handles
 * as well as the queue that is used for rendering.
 */
MIDL_INTERFACE("2eaa4b89-0107-4bdb-87f7-0f541c493ce0")
ID3D9VkInteropDevice : public IUnknown {
  /**
   * \brief Queries Vulkan handles used by DXVK
   * 
   * \param [out] pInstance The Vulkan instance
   * \param [out] pPhysDev The physical device
   * \param [out] pDevide The device handle
   */
  virtual void STDMETHODCALLTYPE GetVulkanHandles(
          VkInstance*           pInstance,
          VkPhysicalDevice*     pPhysDev,
          VkDevice*             pDevice) = 0;

  /**
   * \brief Queries the rendering queue used by DXVK
   * 
   * \param [out] pQueue The Vulkan queue handle
   * \param [out] pQueueIndex Queue index
   * \param [out] pQueueFamilyIndex Queue family index
   */
  virtual void STDMETHODCALLTYPE GetSubmissionQueue(
          VkQueue*              pQueue,
          uint32_t*             pQueueIndex,
          uint32_t*             pQueueFamilyIndex) = 0;

  /**
   * \brief Transitions a Texture to a given layout
   * 
   * Executes an explicit image layout transition on the
   * D3D device. Note that the image subresources \e must
   * be transitioned back to its original layout before
   * using it again from D3D9.
   * Synchronization is left up to the caller.
   * This function merely emits a call to transition the
   * texture on the DXVK internal command stream.
   * \param [in] pTexture The image to transform
   * \param [in] pSubresources Subresources to transform
   * \param [in] OldLayout Current image layout
   * \param [in] NewLayout Desired image layout
   */
  virtual void STDMETHODCALLTYPE TransitionTextureLayout(
          ID3D9VkInteropTexture*    pTexture,
    const VkImageSubresourceRange*  pSubresources,
          VkImageLayout             OldLayout,
          VkImageLayout             NewLayout) = 0;

  /**
   * \brief Flushes outstanding D3D rendering commands
   * 
   * Must be called before submitting Vulkan commands
   * to the rendering queue if those commands use the
   * backing resource of a D3D9 object.
   */
  virtual void STDMETHODCALLTYPE FlushRenderingCommands() = 0;
  
  /**
   * \brief Locks submission queue
   * 
   * Should be called immediately before submitting
   * Vulkan commands to the rendering queue in order
   * to prevent DXVK from using the queue.
   * 
   * While the submission queue is locked, no D3D9
   * methods must be called from the locking thread,
   * or otherwise a deadlock might occur.
   */
  virtual void STDMETHODCALLTYPE LockSubmissionQueue() = 0;
  
  /**
   * \brief Releases submission queue
   * 
   * Should be called immediately after submitting
   * Vulkan commands to the rendering queue in order
   * to allow DXVK to submit new commands.
   */
  virtual void STDMETHODCALLTYPE ReleaseSubmissionQueue() = 0;

  /**
   * \brief Locks the device
   * 
   * Can be called to ensure no D3D9 device methods
   * can be executed until UnlockDevice has been called.
   * 
   * This will do nothing if the D3DCREATE_MULTITHREADED
   * is not set.
   */
  virtual void STDMETHODCALLTYPE LockDevice() = 0;
  
  /**
   * \brief Unlocks the device
   * 
   * Must only be called after a call to LockDevice.
   */
  virtual void STDMETHODCALLTYPE UnlockDevice() = 0;

  /**
   * \brief Wait for a resource to finish being used
   * 
   * Waits for the GPU resource associated with the
   * resource to finish being used by the GPU.
   * 
   * Valid D3DLOCK flags:
   *  - D3DLOCK_READONLY:  Only waits for writes
   *  - D3DLOCK_DONOTWAIT: Does not wait for the resource (may flush)
   * 
   * \param [in] pResource Resource to be waited upon
   * \param [in] MapFlags D3DLOCK flags
   * \returns true if the resource is ready to use,
   *          false if the resource is till in use
   */
  virtual bool STDMETHODCALLTYPE WaitForResource(
          IDirect3DResource9*  pResource,
          DWORD                MapFlags) = 0;

  /**
   * \brief Creates a custom image/surface/texture
   * 
   * \param [in] desc Image description
   * \param [out, retval] ppResult Pointer to a resource of the D3DRESOURCETYPE given by desc.Type
   * \returns D3D_OK, D3DERR_INVALIDCALL, or D3DERR_OUTOFVIDEOMEMORY
   */
  virtual HRESULT STDMETHODCALLTYPE CreateImage(
          const D3D9VkExtImageDesc* desc,
          IDirect3DResource9**      ppResult) = 0;
};

/**
 * \brief D3D9 current output metadata
 */
struct D3D9VkExtOutputMetadata {
  float RedPrimary[2];
  float GreenPrimary[2];
  float BluePrimary[2];
  float WhitePoint[2];
  float MinLuminance;
  float MaxLuminance;
  float MaxFullFrameLuminance;
};

/**
 * \brief D3D9 extended swapchain
 */
MIDL_INTERFACE("13776e93-4aa9-430a-a4ec-fe9e281181d5")
ID3D9VkExtSwapchain : public IUnknown {
  virtual BOOL STDMETHODCALLTYPE CheckColorSpaceSupport(
          VkColorSpaceKHR           ColorSpace) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetColorSpace(
          VkColorSpaceKHR           ColorSpace) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetHDRMetaData(
    const VkHdrMetadataEXT          *pHDRMetadata) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetCurrentOutputDesc(
          D3D9VkExtOutputMetadata   *pOutputDesc) = 0;

  virtual void STDMETHODCALLTYPE UnlockAdditionalFormats() = 0;
};

#ifndef _MSC_VER
__CRT_UUID_DECL(ID3D9VkInteropInterface,   0x3461a81b,0xce41,0x485b,0xb6,0xb5,0xfc,0xf0,0x8b,0xa6,0xa6,0xbd);
__CRT_UUID_DECL(ID3D9VkInteropInterface1,  0xd6589ed4,0x7a37,0x4096,0xba,0xc2,0x22,0x3b,0x25,0xae,0x31,0xd2);
__CRT_UUID_DECL(ID3D9VkInteropTexture,     0xd56344f5,0x8d35,0x46fd,0x80,0x6d,0x94,0xc3,0x51,0xb4,0x72,0xc1);
__CRT_UUID_DECL(ID3D9VkInteropDevice,      0x2eaa4b89,0x0107,0x4bdb,0x87,0xf7,0x0f,0x54,0x1c,0x49,0x3c,0xe0);
__CRT_UUID_DECL(ID3D9VkExtSwapchain,       0x13776e93,0x4aa9,0x430a,0xa4,0xec,0xfe,0x9e,0x28,0x11,0x81,0xd5);
#endif
