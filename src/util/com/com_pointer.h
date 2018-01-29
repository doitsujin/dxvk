#pragma once

#include "com_include.h"

namespace dxvk {
  
  /**
   * \brief COM pointer
   * 
   * Implements automatic reference
   * counting for COM objects.
   */
  template<typename T>
  class Com {
    
  public:
    
    Com() { }
    Com(std::nullptr_t) { }
    Com(T* object)
    : m_ptr(object) {
      this->incRef();
    }
    
    Com(const Com& other)
    : m_ptr(other.m_ptr) {
      this->incRef();
    }
    
    Com(Com&& other)
    : m_ptr(other.m_ptr) {
      other.m_ptr = nullptr;
    }
    
    Com& operator = (T* object) {
      this->decRef();
      m_ptr = object;
      this->incRef();
      return *this;
    }
    
    Com& operator = (const Com& other) {
      other.incRef();
      this->decRef();
      m_ptr = other.m_ptr;
      return *this;
    }
    
    Com& operator = (Com&& other) {
      this->decRef();
      this->m_ptr = other.m_ptr;
      other.m_ptr = nullptr;
      return *this;
    }
    
    Com& operator = (std::nullptr_t) {
      this->decRef();
      m_ptr = nullptr;
      return *this;
    }
    
    ~Com() {
      this->decRef();
    }
    
    T* operator -> () const {
      return m_ptr;
    }
    
    T**       operator & ()       { return &m_ptr; }
    T* const* operator & () const { return &m_ptr; }
    
    bool operator == (const Com<T>& other) const { return m_ptr == other.m_ptr; }
    bool operator != (const Com<T>& other) const { return m_ptr != other.m_ptr; }
    
    bool operator == (const T* other) const { return m_ptr == other; }
    bool operator != (const T* other) const { return m_ptr != other; }
    
    bool operator == (std::nullptr_t) const { return m_ptr == nullptr; }
    bool operator != (std::nullptr_t) const { return m_ptr != nullptr; }
    
    T* ref() const {
      this->incRef();
      return m_ptr;
    }
    
    T* ptr() const {
      return m_ptr;
    }
    
  private:
    
    T* m_ptr = nullptr;
    
    void incRef() const {
      if (m_ptr != nullptr)
        m_ptr->AddRef();
    }
    
    void decRef() const {
      if (m_ptr != nullptr)
        m_ptr->Release();
    }
    
  };
  
  template<typename T>
  T* ref(T* object) {
    if (object != nullptr)
      object->AddRef();
    return object;
  }
  
}
