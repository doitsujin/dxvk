#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <utility>

#include "../util/util_flags.h"

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
   * \brief Tracking reference
   *
   * Virtual base class for tracking references. The destructor
   * of each subclass is responsible for releasing the tracked
   * object in an implementation-defined manner.
   */
  class DxvkTrackingRef {

  public:

    /**
     * \brief Releases tracked object and destroys reference
     *
     * This is essentially a destructor with a parameter, and
     * subclasses \e must invoke their own destructor here.
     * \param [in] submission Last completed GPU submission
     */
    virtual ~DxvkTrackingRef() { }

  };


  /**
   * \brief Tracking reference storage
   *
   * Provides storage for the bae object and an additional pointer.
   * All tracking references must be compatible with this layout. 
   */
  struct DxvkTrackingRefStorage {
    alignas(DxvkTrackingRef)
    char data[sizeof(DxvkTrackingRef) + sizeof(void*)];
  };


  /**
   * \brief Typed tracking reference for normal ref-counted object
   */
  template<typename T>
  class DxvkObjectRef : public DxvkTrackingRef {

  public:

    explicit DxvkObjectRef(Rc<T>&& object)
    : m_object(std::move(object)) { }

  private:

    Rc<T> m_object;

  };


  /**
   * \brief Object tracker
   *
   * Stores tracking references which keep objects alive until the GPU
   * is done using them. Uses a list of arrays in order to avoid having
   * to move or copy the stored references at any time.
   */
  class DxvkObjectTracker {
    constexpr static size_t ListCapacity = 1024;
    constexpr static size_t ListMask = ListCapacity - 1u;
  public:

    DxvkObjectTracker();

    ~DxvkObjectTracker();

    template<typename T, typename... Args>
    force_inline void track(Args&&... args) {
      static_assert(sizeof(T) <= sizeof(DxvkTrackingRefStorage));
      new (m_next->storage[(m_size++) & ListMask].data) T(std::forward<Args>(args)...);

      if (unlikely(!(m_size & ListMask)))
        advanceList();
    }

    void clear();

  private:

    struct List {
      std::array<DxvkTrackingRefStorage, ListCapacity> storage = { };
      std::unique_ptr<List> next;
    };

    std::unique_ptr<List> m_head;
    List*                 m_next = nullptr;
    size_t                m_size = 0u;

    void advanceList();

  };

}
