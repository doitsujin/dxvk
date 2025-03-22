#pragma once

#include "../util/com/com_object.h"

#include "../vulkan/vulkan_loader.h"

#include "dxgi_interfaces.h"

namespace dxvk {

  /**
   * \brief Surface factory
   *
   * Provides a way to transparently create a
   * Vulkan surface for a given platform window.
   */
  class DxgiSurfaceFactory : public ComObject<IDXGIVkSurfaceFactory> {

  public:

    DxgiSurfaceFactory(
            PFN_vkGetInstanceProcAddr vulkanLoaderProc,
            HWND                      hWnd);

    ~DxgiSurfaceFactory();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject);

    VkResult STDMETHODCALLTYPE CreateSurface(
            VkInstance                Instance,
            VkPhysicalDevice          Adapter,
            VkSurfaceKHR*             pSurface);

  private:

    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr = nullptr;
    HWND                      m_window = nullptr;
    bool                      m_ownsWindow = false;

    HWND CreateDummyWindow();

    void DestroyDummyWindow();

  };
  
}
