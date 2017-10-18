#include <dxvk_framebuffer.h>
#include <dxvk_instance.h>
#include <dxvk_main.h>
#include <dxvk_surface.h>

#include <cstring>
#include <fstream>

#include <windows.h>
#include <windowsx.h>

using namespace dxvk;

class TriangleApp {
  
public:
  
  TriangleApp(HINSTANCE instance, HWND window)
  : m_dxvkInstance    (new DxvkInstance()),
    m_dxvkAdapter     (m_dxvkInstance->enumAdapters().at(0)),
    m_dxvkDevice      (m_dxvkAdapter->createDevice()),
    m_dxvkSurface     (m_dxvkAdapter->createSurface(instance, window)),
    m_dxvkSwapchain   (m_dxvkDevice->createSwapchain(m_dxvkSurface,
      DxvkSwapchainProperties {
        VkSurfaceFormatKHR { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        VK_PRESENT_MODE_FIFO_KHR,
        VkExtent2D { 640, 480 },
      })),
    m_dxvkContext     (m_dxvkDevice->createContext()),
    m_dxvkCommandList (m_dxvkDevice->createCommandList()) {
    
    DxvkBufferCreateInfo bufferInfo;
    bufferInfo.size   = sizeof(m_testData);
    bufferInfo.usage  = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.stages = VK_PIPELINE_STAGE_HOST_BIT
                      | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    bufferInfo.access = VK_ACCESS_HOST_WRITE_BIT
                      | VK_ACCESS_HOST_READ_BIT
                      | VK_ACCESS_SHADER_WRITE_BIT
                      | VK_ACCESS_SHADER_READ_BIT;
    
    m_testBuffer = m_dxvkDevice->createBuffer(bufferInfo,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    for (size_t i = 0; i < 64; i++)
      m_testData[i] = static_cast<int>(i);
    std::memcpy(m_testBuffer->mapPtr(),
      m_testData, sizeof(m_testData));
    
    DxvkResourceSlot computeBufferSlot;
    computeBufferSlot.mode.set(
      DxvkResourceModeBit::Read,
      DxvkResourceModeBit::Write);
    computeBufferSlot.type = DxvkResourceType::StorageBuffer;
    computeBufferSlot.slot = 0;
    
    SpirvCodeBuffer code(std::ifstream("comp.spv", std::ios::binary));
    code.store(std::ofstream("comp.2.spv", std::ios::binary));
    
    m_compShader = new DxvkShader(
      VK_SHADER_STAGE_COMPUTE_BIT, std::move(code),
      1, &computeBufferSlot);
  }
  
  ~TriangleApp() {
    
  }
  
  void run() {
    auto sync1 = m_dxvkDevice->createSemaphore();
    auto sync2 = m_dxvkDevice->createSemaphore();
    
    auto fb = m_dxvkSwapchain->getFramebuffer(sync1);
    auto fbSize = fb->size();
    
    m_dxvkContext->beginRecording(m_dxvkCommandList);
    m_dxvkContext->bindFramebuffer(fb);
    
    VkClearAttachment clearAttachment;
    clearAttachment.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    clearAttachment.colorAttachment = 0;
    clearAttachment.clearValue.color.float32[0] = 1.0f;
    clearAttachment.clearValue.color.float32[1] = 1.0f;
    clearAttachment.clearValue.color.float32[2] = 1.0f;
    clearAttachment.clearValue.color.float32[3] = 1.0f;
    
    VkClearRect clearArea;
    clearArea.rect           = VkRect2D { { 0, 0 }, fbSize.width, fbSize.height };
    clearArea.baseArrayLayer = 0;
    clearArea.layerCount     = fbSize.layers;
    
    m_dxvkContext->clearRenderTarget(
      clearAttachment,
      clearArea);
    m_dxvkContext->bindShader(
      VK_SHADER_STAGE_COMPUTE_BIT,
      m_compShader);
    m_dxvkContext->bindStorageBuffer(
      VK_SHADER_STAGE_COMPUTE_BIT, 0,
      m_testBuffer, 0, sizeof(m_testData));
    m_dxvkContext->dispatch(1, 1, 1);
    m_dxvkContext->endRecording();
    
    auto fence = m_dxvkDevice->submitCommandList(
      m_dxvkCommandList, sync1, sync2);
    m_dxvkSwapchain->present(sync2);
    m_dxvkDevice->waitForIdle();
    
    std::memcpy(m_testData, m_testBuffer->mapPtr(), sizeof(m_testData));
    std::cout << m_testData[0] << std::endl;
  }
  
private:
  
  Rc<DxvkInstance>    m_dxvkInstance;
  Rc<DxvkAdapter>     m_dxvkAdapter;
  Rc<DxvkDevice>      m_dxvkDevice;
  Rc<DxvkSurface>     m_dxvkSurface;
  Rc<DxvkSwapchain>   m_dxvkSwapchain;
  Rc<DxvkContext>     m_dxvkContext;
  Rc<DxvkCommandList> m_dxvkCommandList;
  
  Rc<DxvkBuffer>      m_testBuffer;
  Rc<DxvkShader>      m_compShader;
  Rc<DxvkShader>      m_vertShader;
  Rc<DxvkShader>      m_fragShader;
  
  int m_testData[64];
  
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
