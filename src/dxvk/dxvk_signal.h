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
     * \param [in] signal The signal
     */
    void add(const Rc<sync::Signal>& signal);
    
    /**
     * \brief Notifies tracked signals
     */
    void notify();
    
    /**
     * \brief Resets signal tracker
     */
    void reset();
    
  private:
    
    std::vector<Rc<sync::Signal>> m_signals;
    
  };
  
}