#pragma once

#include <string>
#include <vector>
#include <unordered_set>

#include "dxvk_vulkan_loader.h"

#include "../../util/util_error.h"
#include "../../util/util_string.h"

namespace dxvk::vk {
  
  /**
   * \brief Name list
   * 
   * Stores a list of extension or layer names.
   * Note that only name constants may be added
   * to a name list. Adding dynamically allocated
   * strings will result in udefined behaviour.
   */
  class NameList {
    
  public:
    
    void add(const char* name) {
      m_list.push_back(name);
    }
    
    auto name(size_t i) const {
      return m_list[i];
    }
    
    auto names() const { return m_list.data(); }
    auto count() const { return m_list.size(); }
    
  private:
    
    std::vector<const char*> m_list;
    
  };
  
  
  /**
   * \brief Name set
   * 
   * Stores a set of supported layers or extensions and
   * provides methods to query their support status.
   */
  class NameSet {
    
  public:
    
    /**
     * \brief Adds an extension to the set
     * \param [in] name The extension to add
     */
    void add(const std::string& name);
    
    /**
     * \brief Checks whether an extension or layer is supported
     * 
     * \param [in] name The layer or extension name
     * \returns \c true if the entity is supported
     */
    bool contains(const std::string& name) const;
    
    /**
     * \brief Enumerates instance layers
     * 
     * \param [in] vkl Vulkan library functions
     * \returns Available instance layers
     */
    static NameSet enumerateInstanceLayers(
      const LibraryFn&        vkl);
    
    /**
     * \brief Enumerates instance extensions
     * 
     * \param [in] vkl Vulkan library functions
     * \param [in] layers Enabled instance layers
     * \returns Available instance extensions
     */
    static NameSet enumerateInstanceExtensions(
      const LibraryFn&        vkl,
      const NameList&         layers);
    
    /**
     * \brief Enumerates device extensions
     * 
     * \param [in] vki Vulkan instance functions
     * \param [in] device The physical device
     * \returns Available device extensions
     */
    static NameSet enumerateDeviceExtensions(
      const InstanceFn&       vki,
            VkPhysicalDevice  device);
    
  private:
    
    std::unordered_set<std::string> m_names;
    
    void addInstanceLayerExtensions(
      const LibraryFn&        vkl,
      const char*             layer);
    
  };
  
}
