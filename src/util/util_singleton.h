#pragma once

#include "rc/util_rc_ptr.h"

#include "thread.h"

namespace dxvk {

/**
 * \brief Singleton helper
 *
 * Class that manages a dynamically created 
 */
template<typename T>
class Singleton {

public:

  template<typename... Args>
  Rc<T> acquire(Args... constantArgs) {
    std::lock_guard lock(m_mutex);

    if (!(m_useCount++))
      m_object = new T(constantArgs...);

    return m_object;
  }

  void release() {
    std::lock_guard lock(m_mutex);

    if (!(--m_useCount))
      m_object = nullptr;
  }

private:

  dxvk::mutex m_mutex;
  size_t      m_useCount  = 0;
  Rc<T>       m_object    = nullptr;;

};

}
