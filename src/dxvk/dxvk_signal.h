#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  /**
   * \brief Signal tracker
   */
  class DxvkSignalTracker {
    
  public:
    
    DxvkSignalTracker();
    ~DxvkSignalTracker();
    
    /**
     * \brief Adds a signal to track
     *
     * \param [in] signal The signal
     * \param [in] value Target value
     */
    void add(const Rc<sync::Signal>& signal, uint64_t value);
    
    /**
     * \brief Notifies tracked signals
     */
    void notify();
    
    /**
     * \brief Resets signal tracker
     */
    void reset();
    
  private:
    
    std::vector<std::pair<Rc<sync::Signal>, uint64_t>> m_signals;
    
  };
  
}