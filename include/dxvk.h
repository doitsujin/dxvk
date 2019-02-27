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

#ifndef DXVK_NO_VULKAN_H
  #include <vulkan/vulkan.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

  typedef VkResult (*PFN_dxvk_create_vulkan_surface)(VkInstance instance, void* window, VkSurfaceKHR *surface);
  typedef struct tag_dxvk_native_info
  {
      PFN_vkGetInstanceProcAddr       pfn_vkGetInstanceProcAddr;
      PFN_dxvk_create_vulkan_surface  pfn_create_vulkan_surface;
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
    IDXGIFactory*           pFactory,
    IDXGIAdapter*           pAdapter,
    UINT                    Flags,
    D3D_FEATURE_LEVEL       FeatureLevel,
    ID3D10Device**         ppDevice
  );

  typedef HRESULT (*PFN_dxvk_native_create_d3d10_device)(dxvk_native_info,IDXGIFactory*,IDXGIAdapter*,UINT,D3D_FEATURE_LEVEL,ID3D10Device**);

  extern dxvk_native_info g_native_info;

#ifdef __cplusplus
}
#endif

#endif