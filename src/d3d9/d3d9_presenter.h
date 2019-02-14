#pragma once

#include "d3d9_include.h"
#include "d3d9_format.h"

#include "../dxvk/dxvk_device.h"

namespace dxvk {

  class D3D9Presenter : public RcObject {

  public:

    D3D9Presenter(
      Rc<DxvkDevice>      device,
      HWND                window,
      D3D9Format          format,
      uint32_t            width,
      uint32_t            height,
      uint32_t            bufferCount,
      bool                vsync);

    void recreateSwapChain(
      D3D9Format          format,
      uint32_t            width,
      uint32_t            height,
      uint32_t            bufferCount,
      bool                vsync);

    inline HWND window() {
      return m_window;
    }

    void present();

  private:

    uint32_t pickFormats(
      D3D9Format          format,
      VkSurfaceFormatKHR* dstFormats);

    uint32_t pickPresentModes(
      bool                vsync,
      VkPresentModeKHR*   dstModes);

    uint32_t pickImageCount(
      uint32_t            preferred);

    void createPresenter(
      D3D9Format          format,
      uint32_t            width,
      uint32_t            height,
      uint32_t            bufferCount,
      bool                vsync);

    void createRenderTargetViews();

    Rc<DxvkDevice> m_device;

    HWND m_window;

    Rc<vk::Presenter> m_presenter;

    std::vector<Rc<DxvkImageView>> m_imageViews;

  };

}