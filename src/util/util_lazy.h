#pragma once

#include <mutex>

namespace dxvk {

  /**
   * \brief Lazy-initialized object
   * 
   * Creates an object on demand with
   * the given constructor arguments.
   */
  template<typename T>
  class Lazy {

  public:

    template<typename... Args>
    T& get(Args... args) {
      if (m_object)
        return *m_object;

      std::lock_guard lock(m_mutex);

      if (!m_object) {
        m_object = std::make_unique<T>(
          std::forward<Args>(args)...);
      }

      return *m_object;
    }

  private:

    dxvk::mutex        m_mutex;
    std::unique_ptr<T> m_object;

  };

}