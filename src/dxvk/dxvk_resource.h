#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  enum class DxvkAccess {
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
  class DxvkResource : public RcObject {
    constexpr static uint32_t UseCountIncrementW = 1 << 18;
    constexpr static uint32_t UseCountIncrementR = 1;
    constexpr static uint32_t UseCountMaskW      = ~(UseCountIncrementW - 1);
    constexpr static uint32_t UseCountMaskR      = ~(UseCountIncrementR - 1);
  public:
    
    virtual ~DxvkResource();
    
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
      uint32_t mask = access == DxvkAccess::Write
        ? UseCountMaskW
        : UseCountMaskR;
      return m_useCount.load() & mask;
    }
    
    /**
     * \brief Acquires resource
     * 
     * Increments use count for the given access type.
     * \param Access Resource access type
     */
    void acquire(DxvkAccess access) {
      if (access != DxvkAccess::None)
        m_useCount += getIncrement(access);
    }

    /**
     * \brief Releases resource
     * 
     * Decrements use count for the given access type.
     * \param Access Resource access type
     */
    void release(DxvkAccess access) {
      if (access != DxvkAccess::None)
        m_useCount -= getIncrement(access);
    }
    
  private:
    
    std::atomic<uint32_t> m_useCount = { 0u };

    static constexpr uint32_t getIncrement(DxvkAccess access) {
      return access == DxvkAccess::Write
        ? UseCountIncrementW
        : UseCountIncrementR;
    }

  };
  
}