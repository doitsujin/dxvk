#include <cstdint>
#include <list>
#include <unordered_map>

namespace dxvk {

  template<typename T>
  class lru_list {

  public:
    typedef typename std::list<T>::const_iterator const_iterator;

    void insert(T value) {
      auto cacheIter = m_cache.find(value);
      if (cacheIter != m_cache.end())
        m_list.erase(cacheIter->second);

      m_list.push_back(value);
      auto iter = m_list.cend();
      iter--;
      m_cache[value] = iter;
    }

    void remove(const T& value) {
      auto cacheIter = m_cache.find(value);
      if (cacheIter == m_cache.end())
        return;

      m_list.erase(cacheIter->second);
      m_cache.erase(cacheIter);
    }

    const_iterator remove(const_iterator iter) {
      auto cacheIter = m_cache.find(*iter);
      m_cache.erase(cacheIter);
      return m_list.erase(iter);
    }

    void touch(const T& value) {
      auto cacheIter = m_cache.find(value);
      if (cacheIter == m_cache.end())
        return;

      m_list.erase(cacheIter->second);
      m_list.push_back(value);
      auto iter = m_list.cend();
      --iter;
      m_cache[value] = iter;
    }

    const_iterator leastRecentlyUsedIter() const {
      return m_list.cbegin();
    }

    const_iterator leastRecentlyUsedEndIter() const {
      return m_list.cend();
    }

    uint32_t size() const noexcept {
      return m_list.size();
    }

  private:
    std::list<T> m_list;
    std::unordered_map<T, const_iterator> m_cache;

  };

}
