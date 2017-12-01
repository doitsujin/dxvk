#pragma once

#include <dxvk_device.h>
#include <dxvk_surface.h>
#include <dxvk_swapchain.h>

#include "../spirv/spirv_module.h"

namespace dxvk {
  
  /**
   * \brief DXGI presenter
   * 
   * Renders the back buffer from the
   * \ref DxgiSwapChain to the Vulkan
   * swap chain.
   */
  class DxgiPresenter : public RcObject {
    
  public:
    
    DxgiPresenter(
      const Rc<DxvkDevice>& device,
            HWND            window,
            UINT            bufferWidth,
            UINT            bufferHeight);
    
    ~DxgiPresenter();
    
    /**
     * \brief Renders image to the screen
     * \param [in] view Source image view
     */
    void presentImage(
      const Rc<DxvkImageView>& view);
    
  private:
    
    Rc<DxvkDevice>      m_device;
    Rc<DxvkContext>     m_context;
    
    Rc<DxvkSurface>     m_surface;
    Rc<DxvkSwapchain>   m_swapchain;
    
    Rc<DxvkSemaphore>   m_acquireSync;
    Rc<DxvkSemaphore>   m_presentSync;
    
    Rc<DxvkShader> createVertexShader();
    Rc<DxvkShader> createFragmentShader();
    
  };
  
}
