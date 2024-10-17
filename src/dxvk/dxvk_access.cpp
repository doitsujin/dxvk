#include "dxvk_access.h"

namespace dxvk {

  DxvkObjectTracker::DxvkObjectTracker() {
    m_head = std::make_unique<List>();
    m_next = m_head.get();
  }


  DxvkObjectTracker::~DxvkObjectTracker() {
    // List should be empty unless something bad has already happened
    clear();
  }


  void DxvkObjectTracker::clear() {
    List* list = nullptr;

    for (size_t i = 0; i < m_size; i++) {
      if (!(i & ListMask))
        list = list ? list->next.get() : m_head.get();

      auto* ref = std::launder(reinterpret_cast<DxvkTrackingRef*>(list->storage[i & ListMask].data));
      ref->~DxvkTrackingRef();
    }

    m_next = m_head.get();
    m_size = 0u;
  }


  void DxvkObjectTracker::advanceList() {
    if (!m_next->next)
      m_next->next = std::make_unique<List>();

    m_next = m_next->next.get();
  }

}
