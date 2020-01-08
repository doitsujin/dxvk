#pragma once

#include <type_traits>

namespace dxvk {

  template<typename T, size_t N>
  class small_vector {
    using storage = std::aligned_storage_t<sizeof(T), alignof(T)>;
  public:

    small_vector() { }

    small_vector             (const small_vector&) = delete;
    small_vector& operator = (const small_vector&) = delete;

    ~small_vector() {
      for (size_t i = 0; i < m_size; i++)
        ptr(i)->~T();

      if (m_capacity > N)
        delete[] u.m_ptr;
    }
    
    size_t size() const {
      return m_size;
    }

    void reserve(size_t n) {
      n = pick_capacity(n);

      if (n <= m_capacity)
        return;

      storage* data = new storage[n];

      for (size_t i = 0; i < m_size; i++) {
        new (&data[i]) T(std::move(*ptr(i)));
        ptr(i)->~T();
      }

      if (m_capacity > N)
        delete[] u.m_ptr;
      
      m_capacity = n;
      u.m_ptr = data;
    }

    const T* data() const { return ptr(0); }
          T* data()       { return ptr(0); }

    void resize(size_t n) {
      reserve(n);

      for (size_t i = n; i < m_size; i++)
        ptr(i)->~T();
      
      for (size_t i = m_size; i < n; i++)
        new (ptr(i)) T();
    }

    void push_back(const T& object) {
      reserve(m_size + 1);
      new (ptr(m_size++)) T(object);
    }

    void push_back(T&& object) {
      reserve(m_size + 1);
      new (ptr(m_size++)) T(std::move(object));
    }

    template<typename... Args>
    void emplace_back(Args... args) {
      reserve(m_size + 1);
      new (ptr(m_size++)) T(std::forward<Args>(args)...);
    }

    void erase(size_t idx) {
      ptr(idx)->~T();

      for (size_t i = idx; i < m_size - 1; i++) {
        new (ptr(i)) T(std::move(*ptr(i + 1)));
        ptr(i + 1)->~T();
      }
    }

    void pop_back() {
      ptr(--m_size)->~T();
    }

          T& operator [] (size_t idx)       { return *ptr(idx); }
    const T& operator [] (size_t idx) const { return *ptr(idx); }

          T& front()       { return *ptr(0); }
    const T& front() const { return *ptr(0); }

          T& back()       { return *ptr(m_size - 1); }
    const T& back() const { return *ptr(m_size - 1); }

  private:

    size_t m_capacity = N;
    size_t m_size     = 0;

    union {
      storage* m_ptr;
      storage  m_data[sizeof(T) * N];
    } u;

    size_t pick_capacity(size_t n) {
      size_t capacity = m_capacity;

      while (capacity < n)
        capacity *= 2;

      return capacity;
    }

    T* ptr(size_t idx) {
      return m_capacity == N
        ? reinterpret_cast<T*>(&u.m_data[idx])
        : reinterpret_cast<T*>(&u.m_ptr[idx]);
    }

    const T* ptr(size_t idx) const {
      return m_capacity == N
        ? reinterpret_cast<const T*>(&u.m_data[idx])
        : reinterpret_cast<const T*>(&u.m_ptr[idx]);
    }

  };

}
