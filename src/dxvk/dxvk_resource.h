#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  enum class DxvkAccess : uint32_t {
    Read    = 0,
    Write   = 1,
    None    = 2,
  };
  
  using DxvkAccessFlags = Flags<DxvkAccess>;
  
  /**
   * \brief DXVK resource
   * 
   * Keeps track of whether the resource is currently in use
   * by the GPU. As soon as a command that uses the resource
   * is recorded, it will be marked as 'in use'.
   */
  class DxvkResource {
    static constexpr uint64_t RdAccessShift = 24;
    static constexpr uint64_t WrAccessShift = 44;

    static constexpr uint64_t RefcountMask  = (1ull << RdAccessShift) - 1;
    static constexpr uint64_t RdAccessMask  = ((1ull << (WrAccessShift - RdAccessShift)) - 1) << RdAccessShift;
    static constexpr uint64_t WrAccessMask  = ((1ull << (64 - WrAccessShift)) - 1) << WrAccessShift;

    static constexpr uint64_t RefcountInc   = 1ull;
    static constexpr uint64_t RdAccessInc   = 1ull << RdAccessShift;
    static constexpr uint64_t WrAccessInc   = 1ull << WrAccessShift;
  public:

    DxvkResource();

    virtual ~DxvkResource();

    /**
     * \brief Unique object identifier
     *
     * Can be used to identify an object even when
     * the lifetime of the object is unknown, and
     * without referencing the actual object.
     * \returns Unique identifier
     */
    uint64_t cookie() const {
      return m_cookie;
    }

    /**
     * \brief Increments reference count
     * \returns New reference count
     */
    uint32_t incRef() {
      return acquire(DxvkAccess::None);
    }

    /**
     * \brief Decrements reference count
     * \returns New reference count
     */
    uint32_t decRef() {
      return release(DxvkAccess::None);
    }

    /**
     * \brief Acquires resource with given access
     *
     * Atomically increments both the reference count
     * as well as the use count for the given access.
     * \returns New reference count
     */
    uint32_t acquire(DxvkAccess access) {
      return uint32_t((m_useCount += getIncrement(access)) & RefcountMask);
    }

    /**
     * \brief Releases resource with given access
     *
     * Atomically decrements both the reference count
     * as well as the use count for the given access.
     * \returns New reference count
     */
    uint32_t release(DxvkAccess access) {
      return uint32_t((m_useCount -= getIncrement(access)) & RefcountMask);
    }

    /**
     * \brief Checks whether resource is in use
     * 
     * Returns \c true if there are pending accesses to
     * the resource by the GPU matching the given access
     * type. Note that checking for reads will also return
     * \c true if the resource is being written to.
     * \param [in] access Access type to check for
     * \returns \c true if the resource is in use
     */
    bool isInUse(DxvkAccess access = DxvkAccess::Read) const {
      uint64_t mask = WrAccessMask;
      if (access == DxvkAccess::Read)
        mask |= RdAccessMask;
      return bool(m_useCount.load() & mask);
    }
    
  private:
    
    std::atomic<uint64_t> m_useCount;
    uint64_t              m_cookie;

    static constexpr uint64_t getIncrement(DxvkAccess access) {
      uint64_t increment = RefcountInc;

      if (access != DxvkAccess::None) {
        increment |= (access == DxvkAccess::Read)
          ? RdAccessInc : WrAccessInc;
      }

      return increment;
    }

    static std::atomic<uint64_t> s_cookie;

  };

}