#pragma once

#include <vector>

#include "dxvk_resource.h"
#include "dxvk_sampler.h"

namespace dxvk {
  
  /**
   * \brief Resource pointer
   *
   * Keeps a resource alive and stores access information.
   */
  template<typename T>
  class DxvkLifetime {
    static constexpr uintptr_t AccessMask = 0x3u;
    static constexpr uintptr_t PointerMask = ~AccessMask;

    static_assert(alignof(T) > AccessMask);
  public:

    DxvkLifetime() = default;

    template<typename Tx>
    DxvkLifetime(
            Rc<Tx>&&                resource,
            DxvkAccess              access)
    : m_ptr(reinterpret_cast<uintptr_t>(resource.ptr()) | uintptr_t(access)) {
      resource.unsafeExtract()->convertRef(DxvkAccess::None, access);
    }

    DxvkLifetime(
      const T*                      resource,
            DxvkAccess              access)
    : m_ptr(reinterpret_cast<uintptr_t>(resource) | uintptr_t(access)) {
      acquire();
    }

    DxvkLifetime(DxvkLifetime&& other)
    : m_ptr(other.m_ptr) {
      other.m_ptr = 0u;
    }

    DxvkLifetime(const DxvkLifetime& other)
    : m_ptr(other.m_ptr) {
      acquire();
    }

    DxvkLifetime& operator = (DxvkLifetime&& other) {
      release();

      m_ptr = other.m_ptr;
      other.m_ptr = 0u;
      return *this;
    }

    DxvkLifetime& operator = (const DxvkLifetime& other) {
      other.acquire();
      release();

      m_ptr = other.m_ptr;
      return *this;
    }

    ~DxvkLifetime() {
      release();
    }

  private:

    uintptr_t m_ptr = 0u;

    T* ptr() const {
      return reinterpret_cast<T*>(m_ptr & PointerMask);
    }

    DxvkAccess access() const {
      return DxvkAccess(m_ptr & AccessMask);
    }

    void acquire() const {
      if (m_ptr)
        ptr()->acquire(access());
    }

    void release() const {
      if (m_ptr)
        ptr()->release(access());
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
     * \brief Adds a sampler to track
     * \param [in] res The sampler to track
     */
    void trackSampler(const Rc<DxvkSampler>& res) {
      m_samplers.push_back(res);
    }

    /**
     * \brief Adds a resource to track
     * \param [in] res The resource to track
     */
    void trackResource(DxvkLifetime<DxvkResource>&& res) {
      m_resources.push_back(std::move(res));
    }

    /**
     * \brief Adds a resource allocation to track
     * \param [in] res The allocation to track
     */
    void trackResource(DxvkLifetime<DxvkResourceAllocation>&& res) {
      m_allocations.push_back(std::move(res));
    }

    /**
     * \brief Resets the command list
     * 
     * Called automatically by the device when
     * the command list has completed execution.
     */
    void reset();

  private:

    std::vector<Rc<DxvkSampler>> m_samplers;

    std::vector<DxvkLifetime<DxvkResource>> m_resources;
    std::vector<DxvkLifetime<DxvkResourceAllocation>> m_allocations;

  };
  
}