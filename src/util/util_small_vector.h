#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace dxvk {

/** Small vector state with stateful allocator */
template<typename Allocator, bool Stateless>
struct SmallVectorState {
  SmallVectorState(size_t initialCapacity, const Allocator& alloc)
  : capacity(initialCapacity), allocator(alloc) { }

  size_t capacity = 0;
  size_t size     = 0;

  Allocator allocator = { };

  Allocator& getAllocator() {
    return allocator;
  }

  void setAllocator(const Allocator& alloc) {
    allocator = alloc;
  }
};


/** Small vector state with stateless allocator. This avoids unnecessary
 *  padding that would otherwise be caused by an empty struct, and creates
 *  a new allocator instance on the fly instead. */
template<typename Allocator>
struct SmallVectorState<Allocator, true> {
  SmallVectorState(size_t initialCapacity, const Allocator&)
  : capacity(initialCapacity) { }

  size_t capacity = 0;
  size_t size     = 0;

  Allocator getAllocator() {
    return Allocator();
  }

  void setAllocator(const Allocator&) { }
};


/** Small vector container with embedded storage for a given number of
 *  elements. Largely functions as a drop-in replacement for std::vector. */
template<typename T, size_t N, typename Allocator = std::allocator<T>>
class small_vector {
  using storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

  template<typename T_, size_t N_, typename Allocator_>
  friend class small_vector;
public:

  constexpr static size_t EmbeddedCapacity = N;

  using iterator = T*;
  using const_iterator = const T*;

  small_vector()
  : small_vector(Allocator()) { }

  /** Initialize small vector with allocator */
  explicit small_vector(const Allocator& alloc)
  : m_state(N, alloc) {
    /* Somewhat nonsensical since we will first populat the,
     * internal array anyway, but hides a GCC warning */
    u.m_ptr = nullptr;
  }

  /** Initialize small vector with size and default element value */
  explicit small_vector(size_t size, T value = T(), const Allocator& alloc = Allocator())
  : small_vector(alloc) {
    resize(size, std::move(value));
  }

  /** Initialize small vector from initializer list */
  small_vector(const std::initializer_list<T>& list, const Allocator& alloc = Allocator())
  : small_vector(alloc) {
    reserve(list.size());

    for (auto iter = list.begin(); iter != list.end(); iter++)
      new (ptr(m_state.size++)) T(*iter);
  }

  /** Move constructor. */
  small_vector(small_vector&& other)
  : m_state(N, other.m_state.getAllocator()) {
    move(std::move(other));
  }

  /** Move constructor from different small_vector type. */
  template<size_t N_, typename Allocator_>
  small_vector(small_vector<T, N_, Allocator_>&& other)
  : m_state(N, other.m_state.getAllocator()) {
    move(std::move(other));
  }

  /** Copy constructor. */
  small_vector(const small_vector& other)
  : m_state(N, Allocator()) {
    copy(other);
  }

  /** Copy constructor from different small_vector type. */
  template<size_t N_, typename Allocator_>
  small_vector(const small_vector<T, N_, Allocator_>& other)
  : m_state(N, Allocator()) {
    copy(other);
  }

  /** Move assignment. */
  small_vector& operator = (small_vector&& other) {
    free();
    move(std::move(other));
    return *this;
  }

  /** Move assignment from different small_vector type. */
  template<size_t N_, typename Allocator_>
  small_vector& operator = (small_vector<T, N_, Allocator_>&& other) {
    free();
    move(std::move(other));
    return *this;
  }

  /** Copy assignment. */
  small_vector& operator = (const small_vector& other) {
    if (this == &other)
      return *this;

    clear();
    copy(other);
    return *this;
  }

  /** Copy assignment from different small_vector type. */
  template<size_t N_, typename Allocator_>
  small_vector& operator = (const small_vector<T, N_, Allocator_>& other) {
    clear();
    copy(other);
    return *this;
  }

  ~small_vector() {
    free();
  }

  /** Pointer to underlying array */
  const T* data() const { return ptr(0); }
        T* data()       { return ptr(0); }

  /** Array size */
  size_t size() const {
    return m_state.size;
  }

  /** Checks whether array is empty. */
  bool empty() const {
    return !size();
  }

  /** Current capacity. Will always be greater
   *  or equal to \c EmbeddedCapacity. */
  size_t capacity() const {
    return m_state.capacity;
  }

  /** Checks whether array elements are embedded in the
   *  object itself or use a separate memory allocation. */
  bool is_embedded() const {
    return capacity() <= EmbeddedCapacity;
  }

  /** Allocates enough storage for \c n elements. */
  void reserve(size_t n) {
    n = pick_capacity(n);

    if (n <= capacity())
      return;

    T* data = m_state.getAllocator().allocate(n);

    for (size_t i = 0; i < size(); i++) {
      auto object = std::launder(ptr(i));

      new (&data[i]) T(std::move(*object));
      object->~T();
    }

    if (!is_embedded())
      m_state.getAllocator().deallocate(u.m_ptr, capacity());

    m_state.capacity = n;
    u.m_ptr = data;
  }

  /** Creates the given number of elements. */
  void resize(size_t n, T value = T()) {
    reserve(n);

    for (size_t i = n; i < size(); i++)
      std::launder(ptr(i))->~T();

    for (size_t i = size(); i < n; i++)
      new (ptr(i)) T(value);

    m_state.size = n;
  }

  /** Appends an element. */
  void push_back(const T& object) {
    reserve(m_state.size + 1);
    new (ptr(m_state.size++)) T(object);
  }

  /** Appends an element with move semantics. */
  void push_back(T&& object) {
    reserve(m_state.size + 1);
    new (ptr(m_state.size++)) T(std::move(object));
  }

  /** Constructs an element in-place at the end of the array. */
  template<typename... Args>
  T& emplace_back(Args&&... args) {
    reserve(m_state.size + 1);
    return *(new (ptr(m_state.size++)) T(std::forward<Args>(args)...));
  }

  /** Removes an element at a given index. */
  iterator erase(size_t idx) {
    std::launder(ptr(idx))->~T();

    for (size_t i = idx; i < size() - 1; i++) {
      auto object = std::launder(ptr(i + 1));

      new (ptr(i)) T(std::move(*object));
      object->~T();
    }

    m_state.size--;
    return begin() + idx;
  }

  /** Removes an element at a given iterator. */
  iterator erase(const T* iter) {
    return erase(std::distance(cbegin(), iter));
  }

  /** Inserts element at given iterator */
  iterator insert(const_iterator iter, const T& element) {
    auto ptr = insert(std::distance(cbegin(), iter));
    return new (ptr) T(element);
  }

  /** Move-inserts element at given iterator */
  iterator insert(const_iterator iter, T&& element) {
    auto ptr = insert(std::distance(cbegin(), iter));
    return new (ptr) T(std::move(element));
  }

  /** Removes last element from array. */
  void pop_back() {
    std::launder(ptr(--m_state.size))->~T();
  }

  /** Removes all elements from array. Does
   *  not deallcate any storage. */
  void clear() {
    for (size_t i = 1; i <= size(); i++)
      std::launder(ptr(size() - i))->~T();

    m_state.size = 0;
  }

  /** Moves array to an allocation of exactly the required
   *  size, or to embedded storage if possible. */
  void shrink_to_fit() {
    if (is_embedded() || size() == capacity())
      return;

    auto oldCapacity = capacity();
    auto oldStorage = u.m_ptr;

    /* Capacity must be at least as large as the embedded storage */
    m_state.capacity = std::max(size(), EmbeddedCapacity);

    if (!is_embedded())
      u.m_ptr = m_state.getAllocator().allocate(capacity());

    for (size_t i = 0u; i < size(); i++) {
      new (ptr(i)) T(std::move(oldStorage[i]));
      oldStorage[i].~T();
    }

    m_state.getAllocator().deallocate(oldStorage, oldCapacity);
  }

  /** Provides unchecked access to elements */
  const T& operator [] (size_t idx) const { return *ptr(idx); }
        T& operator [] (size_t idx)       { return *ptr(idx); }

  /** Iterator begin/end. */
  T* begin() { return ptr(0); }
  T* end() { return begin() + size(); }

  /** Constant iterator begin/end. */
  const T* begin() const { return ptr(0); }
  const T* end() const { return begin() + size(); }

  const T* cbegin() const { return begin(); }
  const T* cend() const { return end(); }

  const T& front() const { return *ptr(0); }
        T& front()       { return *ptr(0); }

  const T& back() const { return *ptr(size() - 1); }
        T& back()       { return *ptr(size() - 1); }

private:

  SmallVectorState<Allocator, std::allocator_traits<Allocator>::is_always_equal::value> m_state;

  union {
    T*      m_ptr;
    storage m_data[N];
  } u;

  /** Computes ideal capacity for given size. */
  size_t pick_capacity(size_t n) {
    size_t newCapacity = capacity();

    while (newCapacity < n)
      newCapacity *= 2;

    return newCapacity;
  }

  /** Retrieves raw pointer to element. Must be laundered
   *  before use since it may point to local storage. */
  T* ptr(size_t idx) {
    return is_embedded()
      ? reinterpret_cast<T*>(u.m_data + idx)
      : u.m_ptr + idx;
  }

  const T* ptr(size_t idx) const {
    return is_embedded()
      ? reinterpret_cast<const T*>(u.m_data + idx)
      : u.m_ptr + idx;
  }

  /** Frees all elements and deallocates storage. */
  void free() {
    for (size_t i = 0; i < size(); i++)
      std::launder(ptr(i))->~T();

    if (!is_embedded())
      m_state.getAllocator().deallocate(u.m_ptr, capacity());

    m_state.capacity = N;
    m_state.size = 0;
  }

  /** Convenience method to move from another vector, assumes
   *  that the destination vector is empty. */
  template<size_t N_, typename Allocator_>
  void move(small_vector<T, N_, Allocator_>&& other) {
    /* We may inherit the data pointer if there is one,
     * so we need to inherit the allocator as well. */
    if constexpr (std::is_same_v<Allocator, Allocator_>)
      m_state.setAllocator(other.m_state.getAllocator());

    if (!std::is_same_v<Allocator, Allocator_> || other.capacity() <= std::max(N, N_)) {
      /* Move individual elements if the source capacity is not
       * greater than either vector's embedded capacity, since
       * the capacity determines which storage to use. */
      reserve(other.size());

      for (size_t i = 0; i < other.size(); i++) {
        auto object = std::launder(other.ptr(i));

        new (ptr(i)) T(std::move(*object));
        object->~T();
      }

      m_state.size = std::exchange(other.m_state.size, size_t(0));
    } else {
      /* Move the storage itself without reallocating any memory. */
      m_state.capacity = std::exchange(other.m_state.capacity, N_);
      m_state.size = std::exchange(other.m_state.size, size_t(0));
      u.m_ptr = std::exchange(other.u.m_ptr, nullptr);
    }
  }

  /** Convenience method to copy from another vector, assumes
   *  that the destination vector is empty. */
  template<size_t N_, typename Allocator_>
  void copy(const small_vector<T, N_, Allocator_>& other) {
    reserve(other.size());

    for (size_t i = 0; i < other.size(); i++)
      emplace_back(other[i]);
  }

  /** Ensures free element at given index */
  T* insert(size_t idx) {
    size_t last = size();
    reserve(last + 1u);

    while (last > idx) {
      auto prev = std::launder(ptr(last - 1u));
      new (ptr(last--)) T(std::move(*prev));
      prev->~T();
    }

    m_state.size++;
    return ptr(idx);
  }

};

}
