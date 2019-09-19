#pragma once

#include <vector>

#include "dxvk_resource.h"

namespace dxvk {
  
  /**
   * \brief DXVK lifetime tracker
   * 
   * Maintains references to a set of resources. This is
   * used to guarantee that resources are not destroyed
   * or otherwise accessed in an unsafe manner until the
   * device has finished using them.
   */
  class DxvkLifetimeTracker {
    
  public:
    
    DxvkLifetimeTracker();
    ~DxvkLifetimeTracker();
    
    /**
     * \brief Adds a resource to track
     * \param [in] rc The resource to track
     */
    template<DxvkAccess Access>
    void trackResource(Rc<DxvkResource>&& rc) {
      rc->acquire(Access);
      m_resources.emplace_back(std::move(rc), Access);
    }
    
    /**
     * \brief Resets the command list
     * 
     * Called automatically by the device when
     * the command list has completed execution.
     */
    void reset();
    
  private:
    
    std::vector<std::pair<Rc<DxvkResource>, DxvkAccess>> m_resources;
    
  };
  
}