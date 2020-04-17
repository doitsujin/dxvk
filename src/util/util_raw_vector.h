#pragma once

#include <type_traits>
#include <memory>

namespace dxvk {

  /**
   * \brief Uninitialized vector
   *
   * Implements a vector with uinitialized storage to avoid
   * std::vector default initialization.
   */
  template<typename T>
  class raw_vector {
    struct deleter {
      void operator()(void* p) const { std::free(p); }
    };
    using pointer_type = std::unique_ptr<T, deleter>;

  public:

    void reserve(size_t n) {
      n = pick_capacity(n);
      if (n > m_capacity)
        reallocate(n);
    }

    void shrink_to_fit() {
      size_t n = pick_capacity(m_size);
      reallocate(n);
    }

    void resize(size_t n) {
      if (n >= m_size) {
        reserve(n);
        std::uninitialized_default_construct(ptr(m_size), ptr(n));
      }
      m_size = n;
    }

    void push_back(const T& object) {
      reserve(m_size + 1);
      *ptr(m_size++) = object;
    }

    void push_back(T&& object) {
      reserve(m_size + 1);
      *ptr(m_size++) = std::move(object);
    }

    template<typename... Args>
    void emplace_back(Args... args) {
      reserve(m_size + 1);
      *ptr(m_size++) = T(std::forward<Args>(args)...);
    }

    void erase(size_t idx) {
      if (idx < m_size)
        std::memmove(ptr(idx), ptr(idx + 1), (m_size - idx) * sizeof(T));
      m_size -= 1;
    }

    void insert(const T* pos, const T* begin, const T* end) {
      if (begin == end)
        return;

      size_t off = pos - ptr(0);
      size_t size = m_size;
      size_t count = (end - begin);
      resize(size + count);

      if (off < size)
        std::memmove(ptr(off) + count, ptr(off),
                     (size - off) * sizeof(T));
      std::memcpy(ptr(off), begin, count * sizeof(T));
    }

    void pop_back() { m_size--; }

    void clear() { m_size = 0; }

    size_t size() const { return m_size; }

    const T* data() const { return ptr(0); }
          T* data()       { return ptr(0); }

    const T* begin() const { return ptr(0); }
          T* begin()       { return ptr(0); }

    const T* end() const { return ptr(m_size); }
          T* end()       { return ptr(m_size); }

          T& operator [] (size_t idx)       { return *ptr(idx); }
    const T& operator [] (size_t idx) const { return *ptr(idx); }

          T& front()       { return *ptr(0); }
    const T& front() const { return *ptr(0); }

          T& back()       { return *ptr(m_size - 1); }
    const T& back() const { return *ptr(m_size - 1); }

  private:

    pointer_type m_ptr = nullptr;
    size_t       m_size = 0;
    size_t       m_capacity = 0;

    size_t pick_capacity(size_t n) {
      size_t capacity = m_capacity;
      if (capacity < 128)
        capacity = 128;

      while (capacity < n)
        capacity *= 2;

      return capacity;
    }

    void reallocate(size_t n) {
      void* ptr = std::realloc(m_ptr.get(), n * sizeof(T));
      m_ptr.release();
      m_ptr.reset(static_cast<T*>(ptr));
      m_capacity = n;
    }

    T* ptr(size_t idx) {
      return reinterpret_cast<T*>(m_ptr.get()) + idx;
    }

    const T* ptr(size_t idx) const {
      return reinterpret_cast<const T*>(m_ptr.get()) + idx;
    }

  };

}
