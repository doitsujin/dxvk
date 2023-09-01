#include "dxgi_surface.h"

#include "../wsi/wsi_window.h"

namespace dxvk {

  DxgiSurfaceFactory::DxgiSurfaceFactory(PFN_vkGetInstanceProcAddr vulkanLoaderProc, HWND hWnd)
  : m_vkGetInstanceProcAddr(vulkanLoaderProc), m_window(hWnd) {

  }


  DxgiSurfaceFactory::~DxgiSurfaceFactory() {

  }


  HRESULT STDMETHODCALLTYPE DxgiSurfaceFactory::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIVkSurfaceFactory)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDXGIVkSurfaceFactory), riid)) {
      Logger::warn("DxgiSurfaceFactory::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  VkResult STDMETHODCALLTYPE DxgiSurfaceFactory::CreateSurface(
          VkInstance                Instance,
          VkPhysicalDevice          Adapter,
          VkSurfaceKHR*             pSurface) {
    return wsi::createSurface(m_window, m_vkGetInstanceProcAddr, Instance, pSurface);
  }
  
}