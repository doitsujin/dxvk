#pragma once

#include <vector>

#include "dxvk_resource.h"

namespace dxvk {
  
  /**
   * \brief Resource pointer
   *
   * Keeps a resource alive and stores access information.
   */
  class DxvkLifetime {

  public:

    DxvkLifetime()
    : m_resource(nullptr), m_access(DxvkAccess::None) { }

    DxvkLifetime(
            DxvkResource*           resource,
            DxvkAccess              access)
    : m_resource(resource), m_access(access) {
      acquire();
    }

    DxvkLifetime(DxvkLifetime&& other)
    : m_resource(other.m_resource), m_access(other.m_access) {
      other.m_resource = nullptr;
      other.m_access = DxvkAccess::None;
    }

    DxvkLifetime(const DxvkLifetime& other)
    : m_resource(other.m_resource), m_access(other.m_access) {
      acquire();
    }

    DxvkLifetime& operator = (DxvkLifetime&& other) {
      release();

      m_resource = other.m_resource;
      m_access = other.m_access;

      other.m_resource = nullptr;
      other.m_access = DxvkAccess::None;
      return *this;
    }

    DxvkLifetime& operator = (const DxvkLifetime& other) {
      other.acquire();
      release();

      m_resource  = other.m_resource;
      m_access = other.m_access;
      return *this;
    }

    ~DxvkLifetime() {
      release();
    }

  private:

    DxvkResource*   m_resource;
    DxvkAccess      m_access;

    void acquire() const {
      if (m_resource)
        m_resource->acquire(m_access);
    }

    void release() const {
      if (m_resource) {
        if (!m_resource->release(m_access))
          delete m_resource;
      }
    }

  };


  /**
   * \brief Lifetime tracker
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
    void trackResource(DxvkResource* rc) {
      m_resources.emplace_back(rc, Access);
    }

    /**
     * \brief Releases resources
     *
     * Marks all tracked resources as unused.
     */
    void notify();
    
    /**
     * \brief Resets the command list
     * 
     * Called automatically by the device when
     * the command list has completed execution.
     */
    void reset();
    
  private:
    
    std::vector<DxvkLifetime> m_resources;
    
  };
  
}