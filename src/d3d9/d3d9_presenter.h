#pragma once

#include "d3d9_include.h"
#include "d3d9_format.h"
#include "d3d9_device.h"
#include "d3d9_common_texture.h"

namespace dxvk {

  class D3D9Presenter : public RcObject {

  public:

    D3D9Presenter(
      Direct3DDevice9Ex*  parent,
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

    std::vector<Rc<Direct3DCommonTexture9>>& getBuffers() {
      return m_buffers;
    }

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

    VkFormat makeSrgb(VkFormat format);

    void createRenderTargetViews();

    Direct3DDevice9Ex* m_parent;
    Rc<DxvkDevice>     m_device;

    HWND m_window;

    Rc<vk::Presenter> m_presenter;

    std::vector<Rc<Direct3DCommonTexture9>> m_buffers;

    D3D9Format m_format;

  };

}