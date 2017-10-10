#pragma once

#include <atomic>

#include "com_include.h"

#define COM_QUERY_IFACE(riid, ppvObject, Iface) \
  if (riid == __uuidof(Iface)) {                \
    this->AddRef();                             \
    *ppvObject = static_cast<Iface*>(this);     \
    return S_OK;                                \
  }

namespace dxvk {
  
  template<typename... Base>
  class ComObject : public Base... {
    
  public:
    
    virtual ~ComObject() { }
    
    ULONG AddRef() final {
      return ++m_refCount;
    }
    
    ULONG Release() final {
      ULONG refCount = --m_refCount;
      if (refCount == 0)
        delete this;
      return refCount;
    }
    
  private:
    
    std::atomic<ULONG> m_refCount = { 0ul };
    
  };
  
}
