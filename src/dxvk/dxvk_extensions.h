#pragma once

#include <set>
#include <vector>

#include "dxvk_include.h"

namespace dxvk {
  
  /**
   * \brief Vulkan extension mode
   * 
   * Defines whether an extension is
   * optional, required, or disabled.
   */
  enum class DxvkExtMode {
    Disabled,
    Optional,
    Required,
  };


  /**
   * \brief Vulkan extension info
   * 
   * Stores information for a single extension.
   * The renderer can use this information to
   * find out which extensions are enabled.
   */
  class DxvkExt {

  public:

    DxvkExt(
      const char*       pName,
            DxvkExtMode mode)
    : m_name(pName), m_mode(mode) { }

    /**
     * \brief Extension name
     * \returns Extension name
     */
    const char* name() const {
      return m_name;
    }

    /**
     * \brief Extension mode
     * \returns Extension mode
     */
    DxvkExtMode mode() const {
      return m_mode;
    }

    /**
     * \brief Checks whether the extension is enabled
     * 
     * If an extension is enabled, the features
     * provided by the extension can be used.
     * \returns \c true if the extension is enabled
     */
    operator bool () const {
      return m_enabled;
    }

    /**
     * \brief Changes extension mode
     * 
     * In some cases, it may be useful to change the
     * default mode dynamically after initialization.
     */
    void setMode(DxvkExtMode mode) {
      m_mode = mode;
    }

    /**
     * \brief Enables the extension
     */
    void enable() {
      m_enabled = true;
    }

  private:

    const char* m_name    = nullptr;
    DxvkExtMode m_mode    = DxvkExtMode::Disabled;
    bool        m_enabled = false;

  };


  /**
   * \brief Vulkan name list
   * 
   * A simple \c vector wrapper that can
   * be used to build a list of layer and
   * extension names.
   */
  class DxvkNameList {

  public:

    /**
     * \brief Adds a name
     * \param [in] pName The name
     */
    void add(const char* pName) {
      m_names.push_back(pName);
    }

    /**
     * \brief Number of names
     * \returns Name count
     */
    uint32_t count() const {
      return m_names.size();
    }

    /**
     * \brief Name list
     * \returns Name list
     */
    const char* const* names() const {
      return m_names.data();
    }

    /**
     * \brief Retrieves a single name
     * 
     * \param [in] index Name index
     * \returns The given name
     */
    const char* name(uint32_t index) const {
      return m_names.at(index);
    }

  private:

    std::vector<const char*> m_names;

  };


  /**
   * \brief Vulkan extension set
   * 
   * Stores a set of extensions or layers
   * supported by the Vulkan implementation.
   */
  class DxvkNameSet {

  public:

    DxvkNameSet();
    ~DxvkNameSet();

    /**
     * \brief Adds a name to the set
     * \param [in] pName Extension name
     */
    void add(
      const char*             pName);
    
    /**
     * \brief Merges two name sets
     * 
     * Adds all names from the given name set to
     * this name set, avoiding duplicate entries.
     * \param [in] names Name set to merge
     */
    void merge(
      const DxvkNameSet&      names);

    /**
     * \brief Checks whether an extension is supported
     * 
     * \param [in] pName Extension name
     * \returns \c true if the extension is supported
     */
    bool supports(
      const char*             pName) const;
    
    /**
     * \brief Enables requested extensions
     * 
     * Walks over a set of extensions and enables all
     * extensions that are supported and not disabled.
     * This also checks whether all required extensions
     * could be enabled, and returns \c false otherwise.
     * \param [in] numExtensions Number of extensions
     * \param [in] ppExtensions List of extensions
     * \param [out] nameSet Extension name set
     * \returns \c true on success
     */
    bool enableExtensions(
            uint32_t          numExtensions,
            DxvkExt**         ppExtensions,
            DxvkNameSet&      nameSet) const;
    
    /**
     * \brief Creates name list from name set
     * 
     * Adds all names contained in the name set
     * to a name list, which can then be passed
     * to Vulkan functions.
     * \returns Name list
     */
    DxvkNameList toNameList() const;

    /**
     * \brief Enumerates instance layers
     * 
     * \param [in] vkl Vulkan library functions
     * \returns Set of available instance layers
     */
    static DxvkNameSet enumInstanceLayers(
      const Rc<vk::LibraryFn>&  vkl);
    
    /**
     * \brief Enumerates instance extensions
     * 
     * \param [in] vkl Vulkan library functions
     * \returns Set of available instance extensions
     */
    static DxvkNameSet enumInstanceExtensions(
      const Rc<vk::LibraryFn>&  vkl);
    
    /**
     * \brief Enumerates device extensions
     * 
     * \param [in] vki Vulkan instance functions
     * \param [in] device The device to query
     * \returns Set of available device extensions
     */
    static DxvkNameSet enumDeviceExtensions(
      const Rc<vk::InstanceFn>& vki,
            VkPhysicalDevice    device);

  private:

    std::set<std::string> m_names;

  };

  /**
   * \brief Device extensions
   * 
   * Lists all Vulkan extensions that are potentially
   * used by DXVK if supported by the implementation.
   */
  struct DxvkDeviceExtensions {
    DxvkExt extShaderViewportIndexLayer     = { VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,      DxvkExtMode::Optional };
    DxvkExt extVertexAttributeDivisor       = { VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME,         DxvkExtMode::Optional };
    DxvkExt khrDedicatedAllocation          = { VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,             DxvkExtMode::Required };
    DxvkExt khrDescriptorUpdateTemplate     = { VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,       DxvkExtMode::Required };
    DxvkExt khrGetMemoryRequirements2       = { VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,        DxvkExtMode::Required };
    DxvkExt khrImageFormatList              = { VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,                DxvkExtMode::Required };
    DxvkExt khrMaintenance1                 = { VK_KHR_MAINTENANCE1_EXTENSION_NAME,                     DxvkExtMode::Required };
    DxvkExt khrMaintenance2                 = { VK_KHR_MAINTENANCE2_EXTENSION_NAME,                     DxvkExtMode::Required };
    DxvkExt khrSamplerMirrorClampToEdge     = { VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME,     DxvkExtMode::Optional };
    DxvkExt khrShaderDrawParameters         = { VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,           DxvkExtMode::Required };
    DxvkExt khrSwapchain                    = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,                        DxvkExtMode::Required };
  };
  
  /**
   * \brief Instance extensions
   * 
   * Lists all Vulkan extensions that are potentially
   * used by DXVK if supported by the implementation.
   */
  struct DxvkInstanceExtensions {
    DxvkExt khrGetPhysicalDeviceProperties2 = { VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, DxvkExtMode::Required };
    DxvkExt khrSurface                      = { VK_KHR_SURFACE_EXTENSION_NAME,                          DxvkExtMode::Required };
    DxvkExt khrWin32Surface                 = { VK_KHR_WIN32_SURFACE_EXTENSION_NAME,                    DxvkExtMode::Required };
  };
  
}