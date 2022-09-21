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

#ifdef _MSC_VER
struct __declspec(uuid("3461a81b-ce41-485b-b6b5-fcf08ba6a6bd")) ID3D9VkInteropInterface;
#else
__CRT_UUID_DECL(ID3D9VkInteropInterface,   0x3461a81b,0xce41,0x485b,0xb6,0xb5,0xfc,0xf0,0x8b,0xa6,0xa6,0xbd);
#endif
