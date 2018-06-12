#pragma once

#include <set>
#include <string>
#include <vector>

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
     * \brief Merges two name sets
     * \param [in] other Name set to merge
     */
    void merge(const NameSet& other);
    
    /**
     * \brief Checks whether an extension or layer is supported
     * 
     * \param [in] name The layer or extension name
     * \returns \c true if the entity is supported
     */
    bool contains(const std::string& name) const;
    
    /**
     * \brief Enumerates instance extensions
     * 
     * \param [in] vkl Vulkan library functions
     * \returns Available instance extensions
     */
    static NameSet enumerateInstanceExtensions(
      const LibraryFn&        vkl);
    
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
    
    /**
     * \brief Generates a name list
     * 
     * The pointers to the names will have the same
     * lifetime as the name set, and may be invalidated
     * by modifications made to the name set.
     * \returns Name list
     */
    NameList getNameList() const;
    
  private:
    
    std::set<std::string> m_names;
    
  };
  
}
