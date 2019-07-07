#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace dxvk::sync {
  
  /**
   * \brief Signal
   * 
   * Acts as a simple CPU fence which can be signaled by one
   * thread and waited upon by one more thread. Waiting on
   * more than one thread is not supported.
   */
  class Signal : public RcObject {
    
  public:
    
    Signal()
    : m_signaled(false) { }
    Signal(bool signaled)
    : m_signaled(signaled) { }
    ~Signal() { }
    
    Signal             (const Signal&) = delete;
    Signal& operator = (const Signal&) = delete;
    
    /**
     * \brief Notifies signal
     * Wakes any waiting thread.
     */
    void notify() {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_signaled.store(true);
      m_cond.notify_one();
    }
    
    /**
     * \brief Waits for signal
     * 
     * Blocks the calling thread until another
     * thread wakes it up, then resets it to
     * the non-signaled state.
     */
    void wait() {
      if (!m_signaled.exchange(false)) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cond.wait(lock, [this] {
          return m_signaled.exchange(false);
        });
      }
    }
    
  private:
    
    std::atomic<bool>       m_signaled;
    std::mutex              m_mutex;
    std::condition_variable m_cond;
    
  };
  
}
