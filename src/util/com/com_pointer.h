#pragma once

#include "com_include.h"

namespace dxvk {
  
  /**
   * \brief Increment public ref count
   * 
   * If the pointer is not \c nullptr, this 
   * calls \c AddRef for the given object.
   * \returns Pointer to the object
   */
  template<typename T>
  T* ref(T* object) {
    if (object != nullptr)
      object->AddRef();
    return object;
  }


  /**
   * \brief Ref count methods for public references
   */
  template<typename T, bool Public>
  struct ComRef_ {
    static void incRef(T* ptr) { ptr->AddRef(); }
    static void decRef(T* ptr) { ptr->Release(); }
  };


  /**
   * \brief Ref count methods for private references
   */
  template<typename T>
  struct ComRef_<T, false> {
    static void incRef(T* ptr) { ptr->AddRefPrivate(); }
    static void decRef(T* ptr) { ptr->ReleasePrivate(); }
  };

  
  /**
   * \brief COM pointer
   * 
   * Implements automatic reference
   * counting for COM objects.
   */
  template<typename T, bool Public = true>
  class Com {
    using ComRef = ComRef_<T, Public>;
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
      m_ptr = nullptr;
    }
    
    T* operator -> () const {
      return m_ptr;
    }
    
    T**       operator & ()       { return &m_ptr; }
    T* const* operator & () const { return &m_ptr; }
    
    template<bool Public_>
    bool operator == (const Com<T, Public_>& other) const { return m_ptr == other.m_ptr; }
    template<bool Public_>
    bool operator != (const Com<T, Public_>& other) const { return m_ptr != other.m_ptr; }
    
    bool operator == (const T* other) const { return m_ptr == other; }
    bool operator != (const T* other) const { return m_ptr != other; }
    
    bool operator == (std::nullptr_t) const { return m_ptr == nullptr; }
    bool operator != (std::nullptr_t) const { return m_ptr != nullptr; }
    
    T* ref() const {
      return dxvk::ref(m_ptr);
    }
    
    T* ptr() const {
      return m_ptr;
    }

    Com<T, true>  pubRef() const { return m_ptr; }
    Com<T, false> prvRef() const { return m_ptr; }
    
  private:
    
    T* m_ptr = nullptr;
    
    void incRef() const {
      if (m_ptr != nullptr)
        ComRef::incRef(m_ptr);
    }
    
    void decRef() const {
      if (m_ptr != nullptr)
        ComRef::decRef(m_ptr);
    }
    
  };
  
}
