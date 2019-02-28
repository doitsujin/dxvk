#include "d3d9_presenter.h"

namespace dxvk {

  D3D9Presenter::D3D9Presenter(
    Direct3DDevice9Ex*  parent,
    HWND                window,
    D3D9Format          format,
    uint32_t            width,
    uint32_t            height,
    uint32_t            bufferCount,
    bool                vsync)
    : m_window{ window }
    , m_parent{ parent }
    , m_device{ parent->GetDXVKDevice() } {
    createPresenter(
      format,
      width,
      height,
      bufferCount,
      vsync);
  }

  void D3D9Presenter::recreateSwapChain(
    D3D9Format          format,
    uint32_t            width,
    uint32_t            height,
    uint32_t            bufferCount,
    bool                vsync) {
    vk::PresenterDesc presenterDesc;
    presenterDesc.imageExtent = { width, height };
    presenterDesc.imageCount = pickImageCount(bufferCount);
    presenterDesc.numFormats = pickFormats(format, presenterDesc.formats);
    presenterDesc.numPresentModes = pickPresentModes(vsync, presenterDesc.presentModes);

    if (m_presenter->recreateSwapChain(presenterDesc) != VK_SUCCESS)
      throw DxvkError("D3D9Presenter: Failed to recreate swap chain");

    createRenderTargetViews();
  }

  void D3D9Presenter::present() {

  }

  VkFormat D3D9Presenter::makeSrgb(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_SRGB;
    default: return format; // TODO: make this srgb-ness more correct.
    }
  }

  uint32_t D3D9Presenter::pickFormats(
    D3D9Format          format,
    VkSurfaceFormatKHR* dstFormats) {
    uint32_t n = 0;

    switch (format) {
      default:
        Logger::warn(str::format("D3D9Presenter: Unexpected format: ", format));

      case D3D9Format::A8B8G8R8:
      case D3D9Format::X8B8G8R8: {
        dstFormats[n++] = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        dstFormats[n++] = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      } break;

      case D3D9Format::A2R10G10B10:
      case D3D9Format::A2B10G10R10: {
        dstFormats[n++] = { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        dstFormats[n++] = { VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      } break;

      case D3D9Format::X1R5G5B5:
      case D3D9Format::A1R5G5B5: {
        dstFormats[n++] = { VK_FORMAT_B5G5R5A1_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        dstFormats[n++] = { VK_FORMAT_R5G5B5A1_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        dstFormats[n++] = { VK_FORMAT_A1R5G5B5_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      }

      case D3D9Format::R5G6B5: {
        dstFormats[n++] = { VK_FORMAT_B5G6R5_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        dstFormats[n++] = { VK_FORMAT_R5G6B5_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      }
    }

    return n;
  }

  uint32_t D3D9Presenter::pickPresentModes(
    bool                vsync,
    VkPresentModeKHR*   dstModes) {
    uint32_t n = 0;

    if (vsync) {
      dstModes[n++] = VK_PRESENT_MODE_FIFO_KHR;
    }
    else {
      dstModes[n++] = VK_PRESENT_MODE_IMMEDIATE_KHR;
      dstModes[n++] = VK_PRESENT_MODE_MAILBOX_KHR;
      dstModes[n++] = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    }

    return n;
  }

  uint32_t D3D9Presenter::pickImageCount(
    uint32_t            preferred) {
    return preferred;
  }

  void D3D9Presenter::createPresenter(
    D3D9Format          format,
    uint32_t            width,
    uint32_t            height,
    uint32_t            bufferCount,
    bool                vsync) {
    m_format = format;

    DxvkDeviceQueue graphicsQueue = m_device->graphicsQueue();

    vk::PresenterDevice presenterDevice;
    presenterDevice.queueFamily = graphicsQueue.queueFamily;
    presenterDevice.queue = graphicsQueue.queueHandle;
    presenterDevice.adapter = m_device->adapter()->handle();

    vk::PresenterDesc presenterDesc;
    presenterDesc.imageExtent = { width, height };
    presenterDesc.imageCount = pickImageCount(bufferCount);
    presenterDesc.numFormats = pickFormats(format, presenterDesc.formats);
    presenterDesc.numPresentModes = pickPresentModes(false, presenterDesc.presentModes);

    m_presenter = new vk::Presenter(m_window,
      m_device->adapter()->vki(),
      m_device->vkd(),
      presenterDevice,
      presenterDesc);

    createRenderTargetViews();
  }

  void D3D9Presenter::createRenderTargetViews() {
    vk::PresenterInfo info = m_presenter->info();

    m_buffers.clear();
    m_buffers.resize(info.imageCount);

    DxvkImageCreateInfo imageInfo;
    imageInfo.type = VK_IMAGE_TYPE_2D;
    imageInfo.format = info.format.format;
    imageInfo.flags = 0;
    imageInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.extent = { info.imageExtent.width, info.imageExtent.height, 1 };
    imageInfo.numLayers = 1;
    imageInfo.mipLevels = 1;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.stages = 0;
    imageInfo.access = 0;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = info.format.format;
    viewInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    viewInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel = 0;
    viewInfo.numLevels = 1;
    viewInfo.minLayer = 0;
    viewInfo.numLayers = 1;

    for (uint32_t i = 0; i < info.imageCount; i++) {
      VkImage imageHandle = m_presenter->getImage(i).image;

      Rc<DxvkImage> image = new DxvkImage(
        m_device->vkd(), imageInfo, imageHandle);

      Rc<DxvkImageView> view = new DxvkImageView(
        m_device->vkd(), image, viewInfo);

      viewInfo.format = makeSrgb(info.format.format);

      Rc<DxvkImageView> viewSrgb = new DxvkImageView(
        m_device->vkd(), image, viewInfo);

      D3D9TextureDesc desc;
      desc.Depth = 1;
      desc.Discard = FALSE;
      desc.Format = m_format;
      desc.Height = image->info().extent.height;
      desc.Lockable = TRUE;
      desc.MipLevels = 1;
      desc.MultiSample = D3DMULTISAMPLE_NONE; // TODO: Multisample on presenter/swapchain
      desc.MultisampleQuality = 0;
      desc.Pool = D3DPOOL_DEFAULT;
      desc.Type = D3DRTYPE_SURFACE;
      desc.Usage = D3DUSAGE_RENDERTARGET;
      desc.Width = image->info().extent.width;

      m_buffers[i] = new Direct3DCommonTexture9(
        m_parent,
        image,
        view,
        viewSrgb,
        &desc);
    }
  }

}