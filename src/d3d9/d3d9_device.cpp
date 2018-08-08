#include "d3d9_device.h"

#include "d3d9_format.h"

namespace dxvk {
  D3D9Device::~D3D9Device() = default;

  D3D9Device::D3D9Device(D3D9Adapter& adapter, HWND hFocusWindow, D3DPRESENT_PARAMETERS& pp)
    : m_adapter(adapter) {
    // Get a handle to the DXGI adapter.
    auto dxgiAdapter = m_adapter.GetAdapter();

    // We're supposed to use the device window for the back buffer,
    // or fall back to the focus window otherwise.
    const auto window = pp.hDeviceWindow ? pp.hDeviceWindow : hFocusWindow;

    // TODO: use the focus window for something.
    // It is currently ignored.

    // Back buffer width and height.
    UINT width = pp.BackBufferWidth, height = pp.BackBufferWidth;

    // If either dimension is 0, we use the window to determine the dimensions.
    if (!width || !height) {
      RECT r{};

      GetClientRect(window, &r);

      width = r.right - r.left;
      height = r.bottom - r.top;
    }

    DXGI_RATIONAL refreshRate { 60, 1 };

    if (pp.Windowed || pp.FullScreen_RefreshRateInHz == 0) {
      // TODO: In windowed mode we must use the desktop's refresh rate.
    } else {
      refreshRate.Numerator = pp.FullScreen_RefreshRateInHz;
    }

    // TODO: store PresentationInterval to use it when calling swapchain->Present

    const DXGI_MODE_DESC mode {
      pp.BackBufferWidth,
      pp.BackBufferHeight,
      refreshRate,
      BackBufferFormatToDXGIFormat(pp.BackBufferFormat),
      DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
      DXGI_MODE_SCALING_UNSPECIFIED,
    };

    // TODO: support multisampling
    const DXGI_SAMPLE_DESC samples {
      1, // pp.MultiSampleType (from 0 to 16)
      0, // Quality: pp.MultiSampleQuality
    };
    const auto usage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;

    const auto backBufferCount = std::min(pp.BackBufferCount, 1u);

    // TODO: DXVK only supports this swap effect, for now.
    const auto swapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const DXGI_SWAP_CHAIN_DESC scDesc {
      mode,
      samples,
      usage,
      backBufferCount,
      window,
      pp.Windowed,
      swapEffect,
      0,
    };

    Com<IDXGISwapChain> swapChain;

    const auto result = D3D11CreateDeviceAndSwapChain(dxgiAdapter,
      D3D_DRIVER_TYPE_UNKNOWN,
      nullptr,
      0,
      // TODO: determine which feature level we actually need.
      nullptr,
      0,
      D3D11_SDK_VERSION,
      &scDesc,
      &swapChain,
      &m_device,
      nullptr,
      &m_ctx
    );

    if (FAILED(result)) {
      Logger::err(str::format("D3D11CreateDeviceAndSwapChain failed: ", result));
      throw DxvkError("Failed to create D3D9 device");
    }

    if (pp.EnableAutoDepthStencil) {
      // TODO: create depth/stencil pp.AutoDepthStencilFormat
    }
  }
}
