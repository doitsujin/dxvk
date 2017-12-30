#pragma once

#include <dxvk_device.h>
#include <dxvk_surface.h>
#include <dxvk_swapchain.h>

#include "dxgi_include.h"

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
      const Rc<DxvkDevice>&          device,
            HWND                     window);
    
    ~DxgiPresenter();
      
    /**
     * \brief Initializes back buffer image
     * \param [in] image Back buffer image
     */
    void initBackBuffer(
      const Rc<DxvkImage>& image);
    
    /**
     * \brief Renders back buffer to the screen
     */
    void presentImage();
    
    /**
     * \brief Sets new back buffer
     * 
     * Recreates internal structures when
     * the back buffer image was replaced.
     * \param [in] image Back buffer image
     */
    void updateBackBuffer(
      const Rc<DxvkImage>& image);
    
    /**
     * \brief Recreats Vulkan swap chain
     * 
     * Only actually recreates the swap chain object
     * if any of the properties have changed. If no
     * properties have changed, this is a no-op.
     * \param [in] options New swap chain options
     */
    void recreateSwapchain(
      const DxvkSwapchainProperties& options);
    
    /**
     * \brief Picks a surface format based on a DXGI format
     * 
     * This will return a supported format that, if possible,
     * has properties similar to those of the DXGI format.
     * \param [in] fmt The DXGI format
     * \returns The Vulkan format
     */
    VkSurfaceFormatKHR pickSurfaceFormat(DXGI_FORMAT fmt) const;
    
    /**
     * \brief Picks a supported present mode
     * 
     * \param [in] preferred Preferred present mode
     * \returns An actually supported present mode
     */
    VkPresentModeKHR pickPresentMode(VkPresentModeKHR preferred) const;
    
  private:
    
    enum BindingIds : uint32_t {
      Sampler = 0,
      Texture = 1,
    };
    
    Rc<DxvkDevice>      m_device;
    Rc<DxvkContext>     m_context;
    
    Rc<DxvkSurface>     m_surface;
    Rc<DxvkSwapchain>   m_swapchain;
    
    Rc<DxvkSampler>     m_samplerFitting;
    Rc<DxvkSampler>     m_samplerScaling;
    
    Rc<DxvkImage>       m_backBuffer;
    Rc<DxvkImage>       m_backBufferResolve;
    Rc<DxvkImageView>   m_backBufferView;
    
    DxvkSwapchainProperties m_options;
    
    Rc<DxvkShader> createVertexShader();
    Rc<DxvkShader> createFragmentShader();
    
  };
  
}
