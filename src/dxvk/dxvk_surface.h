#pragma once

#include "dxvk_adapter.h"

namespace dxvk {
  
  /**
   * \brief DXVK surface
   * 
   * Vulkan representation of a drawable window surface. This
   * class provides methods to query the current dimension of
   * the surface as well as format support queries.
   */
  class DxvkSurface : public RcObject {
    
  public:
    
    DxvkSurface(
      const Rc<DxvkAdapter>&    adapter,
            HINSTANCE           instance,
            HWND                window);
    ~DxvkSurface();
    
    /**
     * \brief Vulkan surface handle
     * \returns The surface handle
     */
    VkSurfaceKHR handle() const {
      return m_handle;
    }
    
    /**
     * \brief Queries surface capabilities
     * 
     * Retrieves up-to-date information about the surface,
     * such as the bounds of the swapchain images.
     * \returns Current surface properties
     */
    VkSurfaceCapabilitiesKHR getSurfaceCapabilities() const;
    
    /**
     * \brief Picks a suitable surface format
     * 
     * \param [in] preferredCount Number of formats to probe
     * \param [in] preferred Preferred surface formats
     * \returns The actual surface format
     */
    VkSurfaceFormatKHR pickSurfaceFormat(
            uint32_t            preferredCount,
      const VkSurfaceFormatKHR* preferred) const;
    
    /**
     * \brief Picks a supported present mode
     * 
     * \param [in] preferredCount Number of modes to probe
     * \param [in] preferred Preferred present modes
     * \returns The actual present mode
     */
    VkPresentModeKHR pickPresentMode(
            uint32_t            preferredCount,
      const VkPresentModeKHR*   preferred) const;
    
    /**
     * \brief Picks a suitable image count for a swap chain
     * 
     * \param [in] caps Surface capabilities
     * \param [in] mode The present mode
     * \param [in] preferred Preferred image count
     * \returns Suitable image count
     */
    uint32_t pickImageCount(
      const VkSurfaceCapabilitiesKHR& caps,
            VkPresentModeKHR          mode,
            uint32_t                  preferred) const;
    
    /**
     * \brief Picks a suitable image size for a swap chain
     * 
     * \param [in] caps Surface capabilities
     * \param [in] preferred Preferred image size
     * \returns Selected image size
     */
    VkExtent2D pickImageExtent(
      const VkSurfaceCapabilitiesKHR& caps,
            VkExtent2D                preferred) const;
    
  private:
    
    Rc<DxvkAdapter>    m_adapter;
    Rc<vk::InstanceFn> m_vki;
    
    VkSurfaceKHR m_handle;
    
    std::vector<VkSurfaceFormatKHR> m_surfaceFormats;
    std::vector<VkPresentModeKHR>   m_presentModes;
    
    VkSurfaceKHR createSurface(HINSTANCE instance, HWND window);
    
    std::vector<VkSurfaceFormatKHR> getSurfaceFormats();
    std::vector<VkPresentModeKHR> getPresentModes();
    
  };
  
}