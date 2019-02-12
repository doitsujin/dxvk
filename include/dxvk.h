#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include <windows.h>
#include <d3d11.h>
#include <d3d10_1.h>
#include <dxgi1_2.h>

#include "../vulkan/vulkan_loader.h"

extern "C" {

  typedef void (*PFN_dxvk_thread_proc)(void *data);

  typedef void* (*PFN_dxvk_create_thread)(PFN_dxvk_thread_proc proc, void *data);
  typedef void (*PFN_dxvk_join_thread)(void *thread);
  typedef void (*PFN_dxvk_detach_thread)(void *thread);
  typedef VkResult (*PFN_dxvk_create_vulkan_surface)(void* window, VkSurfaceKHR *surface);

  typedef struct tag_dxvk_native_info
  {
      PFN_dxvk_create_thread          pfn_create_thread;
      PFN_dxvk_join_thread            pfn_join_thread;
      PFN_dxvk_detach_thread          pfn_detach_thread;
      PFN_dxvk_create_vulkan_surface  pfn_create_vulkan_surface;
  } dxvk_native_info;

  HRESULT dxvk_native_create_d3d11_device(
    dxvk_native_info            native_info,
    IDXGIAdapter*               pAdapter,
    D3D_DRIVER_TYPE             DriverType,
    HMODULE                     Software,
    UINT                        Flags,
    const D3D_FEATURE_LEVEL*    pFeatureLevels,
    UINT                        FeatureLevels,
    UINT                        SDKVersion,
    ID3D11Device**              ppDevice,
    D3D_FEATURE_LEVEL*          pFeatureLevel,
    ID3D11DeviceContext**       ppImmediateContext
  );

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

}