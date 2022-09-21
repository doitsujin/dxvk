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
 * \brief D3D9 texture interface for Vulkan interop
 * 
 * Provides access to the backing resource of a
 * D3D9 texture.
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

#ifdef _MSC_VER
struct __declspec(uuid("3461a81b-ce41-485b-b6b5-fcf08ba6a6bd")) ID3D9VkInteropInterface;
struct __declspec(uuid("d56344f5-8d35-46fd-806d-94c351b472c1")) ID3D9VkInteropTexture;
#else
__CRT_UUID_DECL(ID3D9VkInteropInterface,   0x3461a81b,0xce41,0x485b,0xb6,0xb5,0xfc,0xf0,0x8b,0xa6,0xa6,0xbd);
__CRT_UUID_DECL(ID3D9VkInteropTexture,     0xd56344f5,0x8d35,0x46fd,0x80,0x6d,0x94,0xc3,0x51,0xb4,0x72,0xc1);
#endif
