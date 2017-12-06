#include <dxvk_framebuffer.h>
#include <dxvk_instance.h>
#include <dxvk_main.h>
#include <dxvk_surface.h>

#include <cstring>
#include <fstream>

#include <windows.h>
#include <windowsx.h>

namespace dxvk {
  Logger Logger::s_instance("dxvk-triangle.log");
}

using namespace dxvk;

const uint32_t vsCode[] = {
	0x07230203,0x00010000,0x00080001,0x00000024,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0007000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000d,0x0000001b,0x00030003,
	0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00060005,0x0000000b,
	0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x0000000b,0x00000000,0x505f6c67,
	0x7469736f,0x006e6f69,0x00070006,0x0000000b,0x00000001,0x505f6c67,0x746e696f,0x657a6953,
	0x00000000,0x00070006,0x0000000b,0x00000002,0x435f6c67,0x4470696c,0x61747369,0x0065636e,
	0x00070006,0x0000000b,0x00000003,0x435f6c67,0x446c6c75,0x61747369,0x0065636e,0x00030005,
	0x0000000d,0x00000000,0x00060005,0x0000001b,0x565f6c67,0x65747265,0x646e4978,0x00007865,
	0x00050005,0x0000001e,0x65646e69,0x6c626178,0x00000065,0x00050048,0x0000000b,0x00000000,
	0x0000000b,0x00000000,0x00050048,0x0000000b,0x00000001,0x0000000b,0x00000001,0x00050048,
	0x0000000b,0x00000002,0x0000000b,0x00000003,0x00050048,0x0000000b,0x00000003,0x0000000b,
	0x00000004,0x00030047,0x0000000b,0x00000002,0x00040047,0x0000001b,0x0000000b,0x0000002a,
	0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,
	0x00040017,0x00000007,0x00000006,0x00000004,0x00040015,0x00000008,0x00000020,0x00000000,
	0x0004002b,0x00000008,0x00000009,0x00000001,0x0004001c,0x0000000a,0x00000006,0x00000009,
	0x0006001e,0x0000000b,0x00000007,0x00000006,0x0000000a,0x0000000a,0x00040020,0x0000000c,
	0x00000003,0x0000000b,0x0004003b,0x0000000c,0x0000000d,0x00000003,0x00040015,0x0000000e,
	0x00000020,0x00000001,0x0004002b,0x0000000e,0x0000000f,0x00000000,0x0004002b,0x00000008,
	0x00000010,0x00000003,0x0004001c,0x00000011,0x00000007,0x00000010,0x0004002b,0x00000006,
	0x00000012,0x00000000,0x0004002b,0x00000006,0x00000013,0x3f000000,0x0004002b,0x00000006,
	0x00000014,0x3f800000,0x0007002c,0x00000007,0x00000015,0x00000012,0x00000013,0x00000012,
	0x00000014,0x0004002b,0x00000006,0x00000016,0xbf000000,0x0007002c,0x00000007,0x00000017,
	0x00000013,0x00000016,0x00000012,0x00000014,0x0007002c,0x00000007,0x00000018,0x00000016,
	0x00000016,0x00000012,0x00000014,0x0006002c,0x00000011,0x00000019,0x00000015,0x00000017,
	0x00000018,0x00040020,0x0000001a,0x00000001,0x0000000e,0x0004003b,0x0000001a,0x0000001b,
	0x00000001,0x00040020,0x0000001d,0x00000007,0x00000011,0x00040020,0x0000001f,0x00000007,
	0x00000007,0x00040020,0x00000022,0x00000003,0x00000007,0x00050036,0x00000002,0x00000004,
	0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003b,0x0000001d,0x0000001e,0x00000007,
	0x0004003d,0x0000000e,0x0000001c,0x0000001b,0x0003003e,0x0000001e,0x00000019,0x00050041,
	0x0000001f,0x00000020,0x0000001e,0x0000001c,0x0004003d,0x00000007,0x00000021,0x00000020,
	0x00050041,0x00000022,0x00000023,0x0000000d,0x0000000f,0x0003003e,0x00000023,0x00000021,
	0x000100fd,0x00010038
};

const uint32_t fsCode[] = {
	0x07230203,0x00010000,0x00080001,0x0000000c,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0006000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x00030010,0x00000004,
	0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,0x00000000,
	0x00040005,0x00000009,0x6f6c6f63,0x00000072,0x00040047,0x00000009,0x0000001e,0x00000000,
	0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,
	0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,
	0x0004003b,0x00000008,0x00000009,0x00000003,0x0004002b,0x00000006,0x0000000a,0x3f800000,
	0x0007002c,0x00000007,0x0000000b,0x0000000a,0x0000000a,0x0000000a,0x0000000a,0x00050036,
	0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0003003e,0x00000009,
	0x0000000b,0x000100fd,0x00010038
};

class TriangleApp {
  
public:
  
  TriangleApp(HINSTANCE instance, HWND window)
  : m_dxvkInstance    (new DxvkInstance()),
    m_dxvkAdapter     (m_dxvkInstance->enumAdapters().at(0)),
    m_dxvkDevice      (m_dxvkAdapter->createDevice(getDeviceFeatures())),
    m_dxvkSurface     (m_dxvkAdapter->createSurface(instance, window)),
    m_dxvkSwapchain   (m_dxvkDevice->createSwapchain(m_dxvkSurface,
      DxvkSwapchainProperties {
        VkSurfaceFormatKHR { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        VK_PRESENT_MODE_FIFO_KHR,
        VkExtent2D { 640, 480 },
      })),
    m_dxvkContext     (m_dxvkDevice->createContext()) {
    
    m_dxvkContext->setInputAssemblyState(
      new DxvkInputAssemblyState(
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE));
    m_dxvkContext->setInputLayout(
      new DxvkInputLayout(0, nullptr, 0, nullptr));
    m_dxvkContext->setRasterizerState(
      new DxvkRasterizerState(
        VK_FALSE, VK_FALSE,
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE, 0.0f, 0.0f, 0.0f,
        1.0f));
    m_dxvkContext->setMultisampleState(
      new DxvkMultisampleState(
        VK_SAMPLE_COUNT_1_BIT, 0xFFFFFFFF,
        VK_FALSE, VK_FALSE, VK_FALSE, 1.0f));
    m_dxvkContext->setDepthStencilState(
      new DxvkDepthStencilState(
        VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE,
        VK_COMPARE_OP_ALWAYS,
        VkStencilOpState(),
        VkStencilOpState(),
        0.0f, 1.0f));
    m_dxvkContext->setBlendState(
      new DxvkBlendState(
        VK_FALSE, VK_LOGIC_OP_COPY,
        0, nullptr));
    
    m_dxvkVertexShader = m_dxvkDevice->createShader(
      VK_SHADER_STAGE_VERTEX_BIT,
      SpirvCodeBuffer(_countof(vsCode), vsCode));
    m_dxvkFragmentShader = m_dxvkDevice->createShader(
      VK_SHADER_STAGE_FRAGMENT_BIT,
      SpirvCodeBuffer(_countof(fsCode), fsCode));
    
    m_dxvkBindingLayout = m_dxvkDevice->createBindingLayout(0, nullptr);
    
    m_dxvkPipeline = m_dxvkDevice->createGraphicsPipeline(m_dxvkBindingLayout,
      m_dxvkVertexShader, nullptr, nullptr, nullptr, m_dxvkFragmentShader);
    
    m_dxvkContext->bindGraphicsPipeline(m_dxvkPipeline);
  }
  
  ~TriangleApp() {
    
  }
  
  void run() {
    auto sync1 = m_dxvkDevice->createSemaphore();
    auto sync2 = m_dxvkDevice->createSemaphore();
    
    auto fb = m_dxvkSwapchain->getFramebuffer(sync1);
    auto fbSize = fb->size();
    
    m_dxvkContext->beginRecording(
      m_dxvkDevice->createCommandList());
    m_dxvkContext->bindFramebuffer(fb);
    
    VkViewport viewport;
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = 640.0f;
    viewport.height   = 480.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor;
    scissor.offset.x      = 0;
    scissor.offset.y      = 0;
    scissor.extent.width  = 640;
    scissor.extent.height = 480;
    
    m_dxvkContext->setViewports(1, &viewport, &scissor);
    
    VkClearAttachment clearAttachment;
    clearAttachment.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    clearAttachment.colorAttachment = 0;
    clearAttachment.clearValue.color.float32[0] = 0.0f;
    clearAttachment.clearValue.color.float32[1] = 0.0f;
    clearAttachment.clearValue.color.float32[2] = 0.0f;
    clearAttachment.clearValue.color.float32[3] = 1.0f;
    
    VkClearRect clearArea;
    clearArea.rect           = VkRect2D { { 0, 0 }, fbSize.width, fbSize.height };
    clearArea.baseArrayLayer = 0;
    clearArea.layerCount     = fbSize.layers;
    
    m_dxvkContext->clearRenderTarget(
      clearAttachment,
      clearArea);
    m_dxvkContext->draw(3, 1, 0, 0);
    
    auto fence = m_dxvkDevice->submitCommandList(
      m_dxvkContext->endRecording(), sync1, sync2);
    m_dxvkSwapchain->present(sync2);
    m_dxvkDevice->waitForIdle();
  }
  
  
  VkPhysicalDeviceFeatures getDeviceFeatures() const {
    VkPhysicalDeviceFeatures features;
    std::memset(&features, 0, sizeof(features));
    return features;
  }
  
private:
  
  Rc<DxvkInstance>    m_dxvkInstance;
  Rc<DxvkAdapter>     m_dxvkAdapter;
  Rc<DxvkDevice>      m_dxvkDevice;
  Rc<DxvkSurface>     m_dxvkSurface;
  Rc<DxvkSwapchain>   m_dxvkSwapchain;
  Rc<DxvkContext>     m_dxvkContext;
  
  Rc<DxvkShader>            m_dxvkVertexShader;
  Rc<DxvkShader>            m_dxvkFragmentShader;
  Rc<DxvkBindingLayout>     m_dxvkBindingLayout;
  Rc<DxvkGraphicsPipeline>  m_dxvkPipeline;
  
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
    640, 480,
    nullptr,
    nullptr,
    hInstance,
    nullptr);
  ShowWindow(hWnd, nCmdShow);

  MSG msg;
  
  TriangleApp app(hInstance, hWnd);
  
  try {
    while (true) {
      if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        if (msg.message == WM_QUIT)
          return msg.wParam;
      } else {
        app.run();
      }
    }
  } catch (const dxvk::DxvkError& e) {
    Logger::err(e.message());
    return msg.wParam;
  }
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}
