#pragma once

#include <atomic>

#include "com_include.h"

#define COM_QUERY_IFACE(riid, ppvObject, Iface) \
  do {                                          \
    if (riid == __uuidof(Iface)) {              \
      this->AddRef();                           \
      *ppvObject = static_cast<Iface*>(this);   \
      return S_OK;                              \
    }                                           \
  } while (0)
  
namespace dxvk {
  
  template<typename... Base>
  class ComObject : public Base... {
    
  public:
    
    virtual ~ComObject() { }
    
    ULONG STDMETHODCALLTYPE AddRef() {
      return ++m_refCount;
    }
    
    ULONG STDMETHODCALLTYPE Release() {
      ULONG refCount = --m_refCount;
      if (refCount == 0) {
        m_refCount += 0x80000000u;
        delete this;
      }
      return refCount;
    }
    
  private:
    
    std::atomic<ULONG> m_refCount = { 0ul };
    
  };
  
  template<typename T>
  inline void InitReturnPtr(T** ptr) {
    if (ptr != nullptr)
      *ptr = nullptr;
  }
  
}
