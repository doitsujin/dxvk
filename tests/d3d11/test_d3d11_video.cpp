#include <d3d11_1.h>

#include <windows.h>
#include <windowsx.h>

#include <cmath>
#include <fstream>
#include <vector>

#include "../test_utils.h"

using namespace dxvk;

class VideoApp {
  
public:
  
  VideoApp(HINSTANCE instance, HWND window)
  : m_window(window) {
    // Create base D3D11 device and swap chain
    DXGI_SWAP_CHAIN_DESC swapchainDesc = { };
    swapchainDesc.BufferDesc.Width = m_windowSizeX;
    swapchainDesc.BufferDesc.Height = m_windowSizeY;
    swapchainDesc.BufferDesc.RefreshRate = { 0, 0 };
    swapchainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapchainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapchainDesc.BufferCount = 2;
    swapchainDesc.SampleDesc = { 1, 0 };
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.OutputWindow = m_window;
    swapchainDesc.Windowed = true;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapchainDesc.Flags = 0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr,
      D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
      D3D11_SDK_VERSION, &swapchainDesc, &m_swapchain,
      &m_device, nullptr, &m_context);

    if (FAILED(hr)) {
      std::cerr << "Failed to initialize D3D11 device and swap chain" << std::endl;
      return;
    }

    if (FAILED(hr = m_device->QueryInterface(IID_PPV_ARGS(&m_vdevice)))) {
      std::cerr << "Failed to query D3D11 video device" << std::endl;
      return;
    }

    if (FAILED(hr = m_context->QueryInterface(IID_PPV_ARGS(&m_vcontext)))) {
      std::cerr << "Failed to query D3D11 video context" << std::endl;
      return;
    }

    if (FAILED(hr = m_swapchain->ResizeTarget(&swapchainDesc.BufferDesc))) {
      std::cerr << "Failed to resize target" << std::endl;
      return;
    }

    if (FAILED(hr = m_swapchain->GetBuffer(0, IID_PPV_ARGS(&m_swapImage)))) {
      std::cerr << "Failed to query swap chain image" << std::endl;
      return;
    }

    if (FAILED(hr = m_device->CreateRenderTargetView(m_swapImage.ptr(), nullptr, &m_swapImageView))) {
      std::cerr << "Failed to create render target view" << std::endl;
      return;
    }

    // Create video processor instance
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC videoEnumDesc = { };
    videoEnumDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    videoEnumDesc.InputFrameRate = { 60, 1 };
    videoEnumDesc.InputWidth = 128;
    videoEnumDesc.InputHeight = 128;
    videoEnumDesc.OutputFrameRate = { 60, 1 };
    videoEnumDesc.OutputWidth = 256;
    videoEnumDesc.OutputHeight = 256;
    videoEnumDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
    
    if (FAILED(hr = m_vdevice->CreateVideoProcessorEnumerator(&videoEnumDesc, &m_venum))) {
      std::cerr << "Failed to create D3D11 video processor enumerator" << std::endl;
      return;
    }

    if (FAILED(hr = m_vdevice->CreateVideoProcessor(m_venum.ptr(), 0, &m_vprocessor))) {
      std::cerr << "Failed to create D3D11 video processor" << std::endl;
      return;
    }

    // Video output image and view
    D3D11_TEXTURE2D_DESC textureDesc = { };
    textureDesc.Width = 256;
    textureDesc.Height = 256;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc = { 1, 0 };
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    if (FAILED(hr = m_device->CreateTexture2D(&textureDesc, nullptr, &m_videoOutput))) {
      std::cerr << "Failed to create D3D11 video output image" << std::endl;
      return;
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc = { };
    outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outputDesc.Texture2D.MipSlice = 0;

    if (FAILED(hr = m_vdevice->CreateVideoProcessorOutputView(m_videoOutput.ptr(), m_venum.ptr(), &outputDesc, &m_videoOutputView))) {
      std::cerr << "Failed to create D3D11 video output view" << std::endl;
      return;
    }

    if (FAILED(hr = m_device->CreateRenderTargetView(m_videoOutput.ptr(), nullptr, &m_videoOutputRtv))) {
      std::cerr << "Failed to create video render target view" << std::endl;
      return;
    }

    // RGBA input image and view
    textureDesc.Width = 128;
    textureDesc.Height = 128;
    textureDesc.BindFlags = 0;

    size_t pixelCount = textureDesc.Width * textureDesc.Height;

    size_t rowSizeRgba = textureDesc.Width * 4;
    size_t rowSizeNv12 = textureDesc.Width;
    size_t rowSizeYuy2 = textureDesc.Width * 2;
    size_t imageSizeRgba = textureDesc.Height * rowSizeRgba;
    size_t imageSizeNv12 = pixelCount + pixelCount / 2;
    size_t imageSizeYuy2 = textureDesc.Height * rowSizeYuy2;

    std::vector<uint8_t> srcData(pixelCount * 3);
    std::vector<uint8_t> imgDataRgba(imageSizeRgba);
    std::vector<uint8_t> imgDataNv12(imageSizeNv12);
    std::vector<uint8_t> imgDataYuy2(imageSizeYuy2);
    std::ifstream ifile("video_image.raw", std::ios::binary);

    if (!ifile || !ifile.read(reinterpret_cast<char*>(srcData.data()), srcData.size())) {
      std::cerr << "Failed to read image file" << std::endl;
      return;
    }

    for (size_t i = 0; i < pixelCount; i++) {
      imgDataRgba[4 * i + 0] = srcData[3 * i + 0];
      imgDataRgba[4 * i + 1] = srcData[3 * i + 1];
      imgDataRgba[4 * i + 2] = srcData[3 * i + 2];
      imgDataRgba[4 * i + 3] = 0xFF;

      imgDataNv12[i] = y_coeff(&srcData[3 * i], 0.299000f, 0.587000f, 0.114000f);

      imgDataYuy2[2 * i + 0] = y_coeff(&srcData[3 * i], 0.299000f, 0.587000f, 0.114000f);
      imgDataYuy2[2 * i + 1] = i % 2
        ? c_coeff(&srcData[3 * i], -0.168736f, -0.331264f,  0.500000f)
        : c_coeff(&srcData[3 * i],  0.500000f, -0.418688f, -0.081312f);
    }

    for (size_t y = 0; y < textureDesc.Height / 2; y++) {
      for (size_t x = 0; x < textureDesc.Width / 2; x++) {
        size_t p = textureDesc.Width * (2 * y) + 2 * x;
        size_t i = pixelCount + textureDesc.Width * y + 2 * x;
        imgDataNv12[i + 0] = c_coeff(&srcData[3 * p],  0.500000f, -0.418688f, -0.081312f);
        imgDataNv12[i + 1] = c_coeff(&srcData[3 * p], -0.168736f, -0.331264f,  0.500000f);
      }
    }

    D3D11_SUBRESOURCE_DATA subresourceData = { };
    subresourceData.pSysMem = imgDataRgba.data();
    subresourceData.SysMemPitch = rowSizeRgba;
    subresourceData.SysMemSlicePitch = rowSizeRgba * textureDesc.Height;

    if (FAILED(hr = m_device->CreateTexture2D(&textureDesc, &subresourceData, &m_videoInput))) {
      std::cerr << "Failed to create D3D11 video input image" << std::endl;
      return;
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc = { };
    inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inputDesc.Texture2D.MipSlice = 0;

    if (FAILED(hr = m_vdevice->CreateVideoProcessorInputView(m_videoInput.ptr(), m_venum.ptr(), &inputDesc, &m_videoInputView))) {
      std::cerr << "Failed to create D3D11 video input view" << std::endl;
      return;
    }

    // NV12 input image and view
    textureDesc.Format = DXGI_FORMAT_NV12;
    textureDesc.BindFlags = 0;

    subresourceData.pSysMem = imgDataNv12.data();
    subresourceData.SysMemPitch = rowSizeNv12;
    subresourceData.SysMemSlicePitch = rowSizeNv12 * textureDesc.Height;

    if (SUCCEEDED(hr = m_device->CreateTexture2D(&textureDesc, nullptr, &m_videoInputNv12))) {
      if (FAILED(hr = m_vdevice->CreateVideoProcessorInputView(m_videoInputNv12.ptr(), m_venum.ptr(), &inputDesc, &m_videoInputViewNv12))) {
        std::cerr << "Failed to create D3D11 video input view for NV12" << std::endl;
        return;
      }
    } else {
      std::cerr << "NV12 not supported" << std::endl;
    }

    textureDesc.Usage = D3D11_USAGE_STAGING;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;

    if (SUCCEEDED(hr = m_device->CreateTexture2D(&textureDesc, nullptr, &m_videoInputNv12Host))) {
      D3D11_MAPPED_SUBRESOURCE mr = { };
      m_context->Map(m_videoInputNv12Host.ptr(), 0, D3D11_MAP_WRITE, D3D11_MAP_FLAG_DO_NOT_WAIT, &mr);
      memcpy(mr.pData, imgDataNv12.data(), imgDataNv12.size());
      m_context->Unmap(m_videoInputNv12Host.ptr(), 0);
      D3D11_BOX box = { 0, 0, 0, 128, 128, 1 };
      m_context->CopySubresourceRegion(m_videoInputNv12.ptr(), 0, 0, 0, 0, m_videoInputNv12Host.ptr(), 0, &box);
    }

    // YUY2 input image and view
    textureDesc.Format = DXGI_FORMAT_YUY2;
    textureDesc.BindFlags = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.CPUAccessFlags = 0;

    subresourceData.pSysMem = imgDataYuy2.data();
    subresourceData.SysMemPitch = rowSizeYuy2;
    subresourceData.SysMemSlicePitch = imageSizeYuy2;

    if (SUCCEEDED(hr = m_device->CreateTexture2D(&textureDesc, &subresourceData, &m_videoInputYuy2))) {
      if (FAILED(hr = m_vdevice->CreateVideoProcessorInputView(m_videoInputYuy2.ptr(), m_venum.ptr(), &inputDesc, &m_videoInputViewYuy2))) {
        std::cerr << "Failed to create D3D11 video input view for YUY2" << std::endl;
        return;
      }
    } else {
      std::cerr << "YUY2 not supported" << std::endl;
    }

    m_initialized = true;
  }
  
  
  ~VideoApp() {

  }
  
  
  void run() {
    this->adjustBackBuffer();

    float color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    m_context->ClearRenderTargetView(m_swapImageView.ptr(), color);

    // Full range RGB output color space
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE csOut = { };
    csOut.Usage     = 0; // Present
    csOut.RGB_Range = 0; // Full range
    csOut.Nominal_Range = 1; // Full range

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE csIn = { };
    csIn.Usage     = 0; // Present
    csIn.RGB_Range = 0; // Full range
    csIn.Nominal_Range = 1; // Full range
    csIn.YCbCr_Matrix = 0; // BT.601

    m_vcontext->VideoProcessorSetStreamAutoProcessingMode(m_vprocessor.ptr(), 0, false);
    m_vcontext->VideoProcessorSetOutputColorSpace(m_vprocessor.ptr(), &csOut);
    m_vcontext->VideoProcessorSetStreamColorSpace(m_vprocessor.ptr(), 0, &csIn);
    blit(m_videoInputView.ptr(), 32, 32);
    blit(m_videoInputViewNv12.ptr(), 32, 320);
    blit(m_videoInputViewYuy2.ptr(), 32, 608);

    csIn.RGB_Range = 1; // Limited range
    csIn.Nominal_Range = 0; // Limited range
    m_vcontext->VideoProcessorSetStreamColorSpace(m_vprocessor.ptr(), 0, &csIn);
    blit(m_videoInputView.ptr(), 320, 32);
    blit(m_videoInputViewNv12.ptr(), 320, 320);
    blit(m_videoInputViewYuy2.ptr(), 320, 608);

    // Limited range RGB output color space
    csOut.RGB_Range = 1;
    csOut.Nominal_Range = 0;
    m_vcontext->VideoProcessorSetOutputColorSpace(m_vprocessor.ptr(), &csOut);

    csIn.RGB_Range = 0; // Full range
    csIn.Nominal_Range = 1; // Full range
    m_vcontext->VideoProcessorSetStreamColorSpace(m_vprocessor.ptr(), 0, &csIn);
    blit(m_videoInputView.ptr(), 608, 32);
    blit(m_videoInputViewNv12.ptr(), 608, 320);
    blit(m_videoInputViewYuy2.ptr(), 608, 608);

    csIn.RGB_Range = 1; // Limited range
    csIn.Nominal_Range = 0; // Limited range
    m_vcontext->VideoProcessorSetStreamColorSpace(m_vprocessor.ptr(), 0, &csIn);
    blit(m_videoInputView.ptr(), 896, 32);
    blit(m_videoInputViewNv12.ptr(), 896, 320);
    blit(m_videoInputViewYuy2.ptr(), 896, 608);

    m_swapchain->Present(1, 0);
  }
  

  void blit(ID3D11VideoProcessorInputView* pView, uint32_t x, uint32_t y) {
    if (!pView)
      return;

    D3D11_VIDEO_PROCESSOR_STREAM stream = { };
    stream.Enable = true;
    stream.pInputSurface = pView;

    D3D11_BOX box;
    box.left = 0;
    box.top = 0;
    box.front = 0;
    box.right = 256;
    box.bottom = 256;
    box.back = 1;

    FLOAT red[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    m_context->ClearRenderTargetView(m_videoOutputRtv.ptr(), red);
    m_vcontext->VideoProcessorBlt(m_vprocessor.ptr(), m_videoOutputView.ptr(), 0, 1, &stream);
    m_context->CopySubresourceRegion(m_swapImage.ptr(), 0, x, y, 0, m_videoOutput.ptr(), 0, &box);
  }

  
  void adjustBackBuffer() {
    RECT windowRect = { };
    GetClientRect(m_window, &windowRect);

    if (uint32_t(windowRect.right - windowRect.left) != m_windowSizeX
     || uint32_t(windowRect.bottom - windowRect.top) != m_windowSizeY) {
      m_windowSizeX = windowRect.right - windowRect.left;
      m_windowSizeY = windowRect.bottom - windowRect.top;

      m_swapImage = nullptr;
      m_swapImageView = nullptr;

      HRESULT hr = m_swapchain->ResizeBuffers(0,
        m_windowSizeX, m_windowSizeY, DXGI_FORMAT_UNKNOWN, 0);

      if (FAILED(hr)) {
        std::cerr << "Failed to resize swap chain buffer" << std::endl;
        return;
      }

      if (FAILED(hr = m_swapchain->GetBuffer(0, IID_PPV_ARGS(&m_swapImage)))) {
        std::cerr << "Failed to query swap chain image" << std::endl;
        return;
      }

      if (FAILED(hr = m_device->CreateRenderTargetView(m_swapImage.ptr(), nullptr, &m_swapImageView))) {
        std::cerr << "Failed to create render target view" << std::endl;
        return;
      }
    }
  }

  operator bool () const {
    return m_initialized;
  }
    
private:
  
  HWND                                m_window;
  uint32_t                            m_windowSizeX = 1280;
  uint32_t                            m_windowSizeY = 720;

  Com<IDXGISwapChain>                 m_swapchain;
  Com<ID3D11Device>                   m_device;
  Com<ID3D11DeviceContext>            m_context;
  Com<ID3D11VideoDevice>              m_vdevice;
  Com<ID3D11VideoContext>             m_vcontext;
  Com<ID3D11VideoProcessorEnumerator> m_venum;
  Com<ID3D11VideoProcessor>           m_vprocessor;
  Com<ID3D11Texture2D>                m_swapImage;
  Com<ID3D11RenderTargetView>         m_swapImageView;
  Com<ID3D11Texture2D>                m_videoOutput;
  Com<ID3D11VideoProcessorOutputView> m_videoOutputView;
  Com<ID3D11RenderTargetView>         m_videoOutputRtv;
  Com<ID3D11Texture2D>                m_videoInput;
  Com<ID3D11VideoProcessorInputView>  m_videoInputView;
  Com<ID3D11Texture2D>                m_videoInputNv12;
  Com<ID3D11Texture2D>                m_videoInputNv12Host;
  Com<ID3D11Texture2D>                m_videoInputYuy2;
  Com<ID3D11VideoProcessorInputView>  m_videoInputViewNv12;
  Com<ID3D11VideoProcessorInputView>  m_videoInputViewYuy2;

  bool                                m_initialized = false;

  static inline uint8_t y_coeff(const uint8_t* rgb, float r, float g, float b) {
    float x = (rgb[0] * r + rgb[1] * g + rgb[2] * b) / 255.0f;
    return 16 + uint8_t(std::roundf(219.0f * std::clamp(x, 0.0f, 1.0f)));
  }

  static inline uint8_t c_coeff(const uint8_t* rgb, float r, float g, float b) {
    float x = ((rgb[0] * r + rgb[1] * g + rgb[2] * b) / 255.0f) + 0.5f;
    return uint8_t(std::roundf(255.0f * std::clamp(x, 0.0f, 1.0f)));
  }

};

LRESULT CALLBACK WindowProc(HWND hWnd,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  HWND hWnd;
  WNDCLASSEXW wc;
  ZeroMemory(&wc, sizeof(WNDCLASSEX));
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
  wc.lpszClassName = L"WindowClass1";
  RegisterClassExW(&wc);

  hWnd = CreateWindowExW(0,
    L"WindowClass1",
    L"Our First Windowed Program",
    WS_OVERLAPPEDWINDOW,
    300, 300,
    1280, 720,
    nullptr,
    nullptr,
    hInstance,
    nullptr);
  ShowWindow(hWnd, nCmdShow);

  MSG msg;
  VideoApp app(hInstance, hWnd);
  
  while (app) {
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      
      if (msg.message == WM_QUIT)
        return msg.wParam;
    } else {
      app.run();
    }
  }

  return 0;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}
