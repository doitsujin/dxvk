#pragma once

#include <unordered_set>

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
    void trackResource(
      const Rc<DxvkResource>& rc);
    
    /**
     * \brief Resets the command list
     * 
     * Called automatically by the device when
     * the command list has completed execution.
     */
    void reset();
    
  private:
    
    std::unordered_set<Rc<DxvkResource>, RcHash<DxvkResource>> m_resources;
    
  };
  
}