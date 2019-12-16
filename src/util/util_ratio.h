#include <numeric>
#include <algorithm>
#include <cstdint>
#include <string>
#include <charconv>

namespace dxvk {

  /**
   * \brief Simplest ratio helper
   */
  template <typename T>
  class Ratio {

  public:

    Ratio(T num, T denom) {
      set(num, denom);
    }

    Ratio(std::string_view view) {
      set(0, 0);

      size_t colon = view.find(":");

      if (colon == std::string_view::npos)
        return;

      std::string_view numStr   = view.substr(0, colon);
      std::string_view denomStr = view.substr(colon + 1);

      T num = 0, denom = 0;
      std::from_chars(numStr.data(),   numStr.data()   + numStr.size(),   num);
      std::from_chars(denomStr.data(), denomStr.data() + denomStr.size(), denom);

      set(num, denom);
    }

    inline T num()    const { return m_num; }
    inline T denom() const { return m_denom; }

    inline bool undefined() const { return m_denom == 0; }

    inline void set(T num, T denom) {
      const T gcd = std::gcd(num, denom);

      if (gcd == 0) {
        m_num   = 0;
        m_denom = 0;

        return;
      }

      m_num = num / gcd;
      m_denom = denom / gcd;
    }

    inline bool operator == (const Ratio& other) const {
      return num() == other.num() && denom() == other.denom();
    }

    inline bool operator != (const Ratio& other) const {
      return !(*this == other);
    }

    inline bool operator >= (const Ratio& other) const {
      return num() * other.denom() >= other.num() * denom();
    }

    inline bool operator > (const Ratio& other) const {
      return num() * other.denom() > other.num() * denom();
    }

    inline bool operator < (const Ratio& other) const {
      return  !(*this >= other);
    }

    inline bool operator <= (const Ratio& other) const {
      return  !(*this > other);
    }

  private:

    T m_num, m_denom;

  };

}