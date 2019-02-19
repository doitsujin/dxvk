#ifndef DXVK_H
#define DXVK_H

#ifndef DXVK_NO_WINDOWS_H
  #ifdef __GNUC__
    #pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
  #endif
  #include <windows.h>
  #include <d3d11.h>
  #include <d3d10_1.h>
  #include <dxgi1_2.h>
#endif

#ifndef DXVK_NO_INTERFACE_H
#include "../dxgi/dxgi_interfaces.h"
#endif

#ifndef DXVK_NO_VULKAN_H
  #include <vulkan/vulkan.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

  typedef void (*PFN_dxvk_thread_proc)(void *data);

  typedef void* (*PFN_dxvk_create_thread)(PFN_dxvk_thread_proc proc, void *data);
  typedef int (*PFN_dxvk_join_thread)(void *thread);
  typedef void (*PFN_dxvk_detach_thread)(void *thread);
  typedef VkResult (*PFN_dxvk_create_vulkan_surface)(VkInstance instance, void* window, VkSurfaceKHR *surface);
  typedef IDXGISwapChain1* (*PFN_dxvk_create_dxgi_swapchain)(IDXGIVkSwapChain *presenter, 
                                                              IDXGIFactory *pFactory, 
                                                              HWND hwnd, 
                                                              const DXGI_SWAP_CHAIN_DESC1 *pDesc, 
                                                              const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc);

  typedef struct tag_dxvk_native_info
  {
      PFN_dxvk_create_thread          pfn_create_thread;
      PFN_dxvk_join_thread            pfn_join_thread;
      PFN_dxvk_detach_thread          pfn_detach_thread;
      PFN_vkGetInstanceProcAddr       pfn_vkGetInstanceProcAddr;
      PFN_dxvk_create_vulkan_surface  pfn_create_vulkan_surface;
      PFN_dxvk_create_dxgi_swapchain  pfn_create_dxgi_swapchain;
  } dxvk_native_info;

  HRESULT dxvk_native_create_d3d11_device(
    dxvk_native_info            native_info,
    IDXGIFactory*               pFactory,
    IDXGIAdapter*               pAdapter,
    UINT                        Flags,
    const D3D_FEATURE_LEVEL*    pFeatureLevels,
    UINT                        FeatureLevels,
    ID3D11Device**              ppDevice
  );

  typedef HRESULT (*PFN_dxvk_native_create_d3d11_device)(dxvk_native_info,IDXGIFactory*,IDXGIAdapter*,UINT,const D3D_FEATURE_LEVEL*,UINT,ID3D11Device**);

  HRESULT dxvk_native_create_d3d10_device(
    dxvk_native_info        native_info,
    IDXGIAdapter*           pAdapter,
    D3D10_DRIVER_TYPE       DriverType,
    HMODULE                 Software,
    UINT                    Flags,
    D3D10_FEATURE_LEVEL1    HardwareLevel,
    UINT                    SDKVersion,
    ID3D10Device1**         ppDevice
  );

  extern dxvk_native_info g_native_info;

#ifdef __cplusplus
}
#endif

#endif