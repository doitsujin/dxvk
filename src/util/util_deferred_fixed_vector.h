#pragma once

#include <type_traits>

namespace dxvk {

  /**
   * \brief Construction-time sized vector with deferred object construction
   * 
   * This class implements a vector whose size is fixed and determined
   * at construction time.
   * Construction of objects is left to the caller to do when they are ready.
   * All destructors for objects 0...N will be called, whether constructed or not.
   * This is useful for constructing large amounts of non-copyable types.
   */
  template <typename T>
  class deferred_fixed_vector {
    using storage = std::aligned_storage_t<sizeof(T), alignof(T)>;
  public:

    deferred_fixed_vector(size_t size)
      : m_size( size )
      , m_ptr ( new storage[size] ) { }

    deferred_fixed_vector             (const deferred_fixed_vector&) = delete;
    deferred_fixed_vector& operator = (const deferred_fixed_vector&) = delete;

    ~deferred_fixed_vector() {
      for (size_t i = 0; i < m_size; i++)
        ptr(i)->~T();

      delete[] m_ptr;
    }

    size_t size() const {
      return m_size;
    }

    template <typename... Args>
    void construct(size_t idx, Args&&... args) {
      new (ptr(idx)) T(std::forward<Args>(args)...);
    }

          T& operator [] (size_t idx)       { return *ptr(idx); }
    const T& operator [] (size_t idx) const { return *ptr(idx); }

    const T* data() const { return ptr(0); }
          T* data()       { return ptr(0); }

          T& front()       { return *ptr(0); }
    const T& front() const { return *ptr(0); }

          T& back()       { return *ptr(m_size - 1); }
    const T& back() const { return *ptr(m_size - 1); }

  private:

    size_t   m_size;
    storage* m_ptr;

    T* ptr(size_t idx) {
      return reinterpret_cast<T*>(&m_ptr[idx]);
    }

    const T* ptr(size_t idx) const {
      return reinterpret_cast<const T*>(&m_ptr[idx]);
    }
  };

}