#include "dxvk_signal.h"

namespace dxvk {
  
  DxvkSignalTracker::DxvkSignalTracker() {

  }


  DxvkSignalTracker::~DxvkSignalTracker() {

  }
    
  
  void DxvkSignalTracker::add(const Rc<sync::Signal>& signal, uint64_t value) {
    for (auto& s : m_signals) {
      if (s.first == signal) {
        s.second = std::max(s.second, value);
        break;
      }
    }

    m_signals.push_back({ signal, value });
  }


  void DxvkSignalTracker::notify() {
    for (const auto& pair : m_signals)
      pair.first->signal(pair.second);
  }


  void DxvkSignalTracker::reset() {
    m_signals.clear();
  }
  
}
