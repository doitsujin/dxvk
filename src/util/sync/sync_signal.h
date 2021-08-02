#pragma once

#include <atomic>
#include <functional>
#include <list>

#include "../rc/util_rc.h"

#include "../thread.h"

namespace dxvk::sync {
  
  /**
   * \brief Signal
   * 
   * Interface for a CPU-side fence. Can be signaled
   * to a given value, and any thread waiting for a
   * lower value will be woken up.
   */
  class Signal : public RcObject {
    
  public:
    
    virtual ~Signal() { }

    /**
     * \brief Last signaled value
     * \returns Last signaled value
     */
    virtual uint64_t value() const = 0;
    
    /**
     * \brief Notifies signal
     *
     * Wakes up all threads currently waiting for
     * a value lower than \c value. Note that
     * \c value must monotonically increase.
     * \param [in] value Value to signal to
     */
    virtual void signal(uint64_t value) = 0;
    
    /**
     * \brief Waits for signal
     * 
     * Blocks the calling thread until another
     * thread signals it with a value equal to
     * or greater than \c value.
     * \param [in] value The value to wait for
     */
    virtual void wait(uint64_t value) = 0;
    
  };


  /**
   * \brief Fence
   *
   * Simple CPU-side fence.
   */
  class Fence final : public Signal {

  public:

    Fence()
    : m_value(0ull) { }

    explicit Fence(uint64_t value)
    : m_value(value) { }

    uint64_t value() const {
      return m_value.load(std::memory_order_acquire);  
    }

    void signal(uint64_t value) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_value.store(value, std::memory_order_release);
      m_cond.notify_all();
    }
    
    void wait(uint64_t value) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_cond.wait(lock, [this, value] {
        return value <= m_value.load(std::memory_order_acquire);
      });
    }

  private:

    std::atomic<uint64_t>    m_value;
    dxvk::mutex              m_mutex;
    dxvk::condition_variable m_cond;

  };


  /**
   * \brief Callback signal
   *
   * CPU-side fence with the ability to call a
   * function when signaled to a given value.
   */
  class CallbackFence final : public Signal {

  public:

    CallbackFence()
    : m_value(0ull) { }

    explicit CallbackFence(uint64_t value)
    : m_value(value) { }

    uint64_t value() const {
      return m_value.load(std::memory_order_acquire);
    }

    void signal(uint64_t value) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_value.store(value, std::memory_order_release);
      m_cond.notify_all();

      for (auto i = m_callbacks.begin(); i != m_callbacks.end(); ) {
        if (value >= i->first) {
          i->second();
          i = m_callbacks.erase(i);
        } else {
          i++;
        }
      }
    }

    void wait(uint64_t value) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_cond.wait(lock, [this, value] {
        return value <= m_value.load(std::memory_order_acquire);
      });
    }

    template<typename Fn>
    void setCallback(uint64_t value, Fn&& proc) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);

      if (value > this->value())
        m_callbacks.emplace_back(std::piecewise_construct,
          std::make_tuple(value),
          std::make_tuple(proc));
      else
        proc();
    }

  private:

    std::atomic<uint64_t>    m_value;
    dxvk::mutex              m_mutex;
    dxvk::condition_variable m_cond;

    std::list<std::pair<uint64_t, std::function<void ()>>> m_callbacks;

  };
  
}
