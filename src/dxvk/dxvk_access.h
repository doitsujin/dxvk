#pragma once

#include <cstdint>

#include "../util/util_flags.h"

#include "../util/rc/util_rc.h"
#include "../util/rc/util_rc_ptr.h"

namespace dxvk {

  /**
   * \brief Resource access flags
   */
  enum class DxvkAccess : uint32_t {
    None    = 0,
    Read    = 1,
    Write   = 2,
  };

  using DxvkAccessFlags = Flags<DxvkAccess>;


  /**
   * \brief Trackable object
   *
   * Base class for any object that can be lifetime-tracked
   * either with or without specific access modes. Methods
   * are mostly expected to call the corresponding \c decRef
   * or \c release methods.
   */
  class DxvkTrackable {

  public:

    virtual ~DxvkTrackable();

    virtual void trackRelease(DxvkAccess access) = 0;

  };


  /**
   * \brief Lifetime-tracking reference
   *
   * Tagged pointer that stores the access type, and releases
   * the tracked object once the reference goes out of scope.
   */
  class DxvkTrackingRef {
    static constexpr uintptr_t AccessMask = 0x3u;
    static constexpr uintptr_t PointerMask = ~AccessMask;

    static_assert(alignof(DxvkTrackable) > AccessMask);
  public:

    DxvkTrackingRef() = default;

    template<typename T>
    explicit DxvkTrackingRef(
            T*                      object,
            DxvkAccess              access)
    : m_ptr(reinterpret_cast<uintptr_t>(static_cast<DxvkTrackable*>(object)) | uintptr_t(access)) {
      // Avoid a virtual method call here since this
      // is going to be used on very hot code paths.
      object->acquire(access);
    }

    template<typename T>
    explicit DxvkTrackingRef(
            T*                      object)
    : m_ptr(reinterpret_cast<uintptr_t>(static_cast<DxvkTrackable*>(object))) {
      object->incRef();
    }

    template<typename T>
    explicit DxvkTrackingRef(
            Rc<T>&&                 object)
    : m_ptr(reinterpret_cast<uintptr_t>(static_cast<DxvkTrackable*>(object.unsafeExtract()))) {
      // No need to do anything since we're not changing
      // the reference type
    }

    DxvkTrackingRef(DxvkTrackingRef&& other)
    : m_ptr(other.m_ptr) {
      other.m_ptr = 0u;
    }

    DxvkTrackingRef& operator = (DxvkTrackingRef&& other) {
      release();

      m_ptr = other.m_ptr;
      other.m_ptr = 0u;
      return *this;
    }

    ~DxvkTrackingRef() {
      release();
    }

  private:

    uintptr_t m_ptr = 0u;

    DxvkTrackable* ptr() const {
      return reinterpret_cast<DxvkTrackable*>(m_ptr & PointerMask);
    }

    DxvkAccess access() const {
      return DxvkAccess(m_ptr & AccessMask);
    }

    void release() const {
      if (m_ptr)
        ptr()->trackRelease(access());
    }

  };

}
