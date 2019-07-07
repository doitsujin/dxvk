#include "dxvk_signal.h"

namespace dxvk {
  
  DxvkSignalTracker::DxvkSignalTracker() {

  }


  DxvkSignalTracker::~DxvkSignalTracker() {

  }
    
  
  void DxvkSignalTracker::add(const Rc<sync::Signal>& signal) {
    m_signals.push_back(signal);
  }


  void DxvkSignalTracker::notify() {
    for (const auto& sig : m_signals)
      sig->notify();
  }


  void DxvkSignalTracker::reset() {
    m_signals.clear();
  }
  
}