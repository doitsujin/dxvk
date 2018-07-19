#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  // Forward declarations
  class DxvkExtension;
  class DxvkExtensionList;
  
  /**
   * \brief Extension type
   */
  enum class DxvkExtensionType {
    Optional, ///< Nothing will happen if not supported
    Desired,  ///< A warning will be issued if not supported
    Required, ///< Device creation will fail if not supported
  };
  
  /**
   * \brief Vulkan extension list
   * 
   * Convenience class to manage a set of extensions
   * which can be either required or optional.
   */
  class DxvkExtensionList : public RcObject {
    friend class DxvkExtension;
  public:
    
    DxvkExtensionList();
    ~DxvkExtensionList();
    
    DxvkExtensionList             (const DxvkExtensionList&) = delete;
    DxvkExtensionList& operator = (const DxvkExtensionList&) = delete;
    
    /**
     * \brief Enables Vulkan extensions
     * 
     * Marks all extension in the list as enabled.
     * \param [in] extensions Supported extensions
     */
    void enableExtensions(
      const vk::NameSet& extensions);
    
    /**
     * \brief Checks extension support status
     * 
     * Checks whether all required extensions are present
     * and logs the name of any unsupported extension.
     * \returns \c true if required extensions are present
     */
    bool checkSupportStatus();
    
    /**
     * \brief Creates a list of enabled extensions
     * 
     * The resulting list can be fed into the Vulkan
     * structs for device and instance creation.
     * \returns Names of enabled Vulkan extensions
     */
    vk::NameSet getEnabledExtensionNames() const;
    
  private:
    
    std::vector<DxvkExtension*> m_extensions;
    
    void registerExtension(DxvkExtension* extension);
    
  };
  
  /**
   * \brief Extension class
   * 
   * Stores the name, type and support
   * status of a single Vulkan extension.
   */
  class DxvkExtension {
    friend class DxvkExtensionList;
  public:
    
    DxvkExtension(
            DxvkExtensionList*  parent,
      const char*               name,
            DxvkExtensionType   type);
    
    DxvkExtension             (const DxvkExtension&) = delete;
    DxvkExtension& operator = (const DxvkExtension&) = delete;
    
    /**
     * \brief Extension name
     * \returns Extension name
     */
    const char* name() const {
      return m_name;
    }
    
    /**
     * \brief Extension type
     * \returns Extension type
     */
    DxvkExtensionType type() const {
      return m_type;
    }
    
    /**
     * \brief Extension support status
     * \returns \c true if supported
     */
    bool enabled() const {
      return m_enabled;
    }
    
  private:
    
    const char*       m_name;
    DxvkExtensionType m_type;
    bool              m_enabled;
    
    void setEnabled(bool enabled) {
      m_enabled = enabled;
    }
    
  };
  
  /**
   * \brief Device extensions
   * 
   * Lists all Vulkan extensions that are potentially
   * used by DXVK if supported by the implementation.
   */
  struct DxvkDeviceExtensions : public DxvkExtensionList {
    DxvkExtension extShaderViewportIndexLayer     = { this, VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,      DxvkExtensionType::Desired  };
    DxvkExtension extVertexAttributeDivisor       = { this, VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,         DxvkExtensionType::Desired  };
    DxvkExtension khrDedicatedAllocation          = { this, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,             DxvkExtensionType::Required };
    DxvkExtension khrDescriptorUpdateTemplate     = { this, VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,       DxvkExtensionType::Required };
    DxvkExtension khrGetMemoryRequirements2       = { this, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,        DxvkExtensionType::Required };
    DxvkExtension khrImageFormatList              = { this, VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,                DxvkExtensionType::Required };
    DxvkExtension khrMaintenance1                 = { this, VK_KHR_MAINTENANCE1_EXTENSION_NAME,                     DxvkExtensionType::Required };
    DxvkExtension khrMaintenance2                 = { this, VK_KHR_MAINTENANCE2_EXTENSION_NAME,                     DxvkExtensionType::Required };
    DxvkExtension khrSamplerMirrorClampToEdge     = { this, VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,     DxvkExtensionType::Desired  };
    DxvkExtension khrShaderDrawParameters         = { this, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,           DxvkExtensionType::Required };
    DxvkExtension khrSwapchain                    = { this, VK_KHR_SWAPCHAIN_EXTENSION_NAME,                        DxvkExtensionType::Required };
  };
  
  /**
   * \brief Instance extensions
   * 
   * Lists all Vulkan extensions that are potentially
   * used by DXVK if supported by the implementation.
   */
  struct DxvkInstanceExtensions : public DxvkExtensionList {
    DxvkExtension khrGetPhysicalDeviceProperties2 = { this, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, DxvkExtensionType::Required };
    DxvkExtension khrSurface                      = { this, VK_KHR_SURFACE_EXTENSION_NAME,                          DxvkExtensionType::Required };
    DxvkExtension khrWin32Surface                 = { this, VK_KHR_WIN32_SURFACE_EXTENSION_NAME,                    DxvkExtensionType::Required };
  };
  
}