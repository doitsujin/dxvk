#include "dxgi_surface.h"

#include "../wsi/wsi_window.h"

namespace dxvk {

  DxgiSurfaceFactory::DxgiSurfaceFactory(PFN_vkGetInstanceProcAddr vulkanLoaderProc, HWND hWnd)
  : m_vkGetInstanceProcAddr(vulkanLoaderProc), m_window(hWnd), m_ownsWindow(!hWnd) {
    if (!m_window)
      m_window = CreateDummyWindow();
  }


  DxgiSurfaceFactory::~DxgiSurfaceFactory() {
    if (m_ownsWindow)
      DestroyDummyWindow();
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


  HWND DxgiSurfaceFactory::CreateDummyWindow() {
#ifdef _WIN32
    static std::atomic<bool> s_wndClassRegistered = { false };

    HINSTANCE hInstance = ::GetModuleHandle(nullptr);

    if (!s_wndClassRegistered.load(std::memory_order_acquire)) {
      WNDCLASSEXW wndClass = { };
      wndClass.cbSize = sizeof(wndClass);
      wndClass.style = CS_HREDRAW | CS_VREDRAW;
      wndClass.lpfnWndProc = &::DefWindowProcW;
      wndClass.hInstance = hInstance;
      wndClass.lpszClassName = L"DXVKDUMMYWNDCLASS";

      ATOM atom = ::RegisterClassExW(&wndClass);

      if (!atom)
        Logger::warn("DxgiSurfaceFactory: Failed to register dummy window class");

      s_wndClassRegistered.store(!!atom, std::memory_order_release);
    }

    HWND hWnd = ::CreateWindowW(L"DXVKDUMMYWNDCLASS", L"DXVKDUMMYWINDOW",
      WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
      Logger::err("DxgiSurfaceFactory: Failed to create dummy window");

    return hWnd;
#else
    return nullptr;
#endif
  }


  void DxgiSurfaceFactory::DestroyDummyWindow() {
#ifdef _WIN32
    DestroyWindow(m_window);
#endif
  }

}
