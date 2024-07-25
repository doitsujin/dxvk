#pragma once

#include <cstdint>

#include "../vulkan/vulkan_loader.h"

#include "util_string.h"

namespace dxvk {

  /**
   * \brief Decoded driver version
   */
  class Version {

  public:

    Version() = default;

    Version(uint32_t major, uint32_t minor, uint32_t patch)
    : m_raw((uint64_t(major) << 48) | (uint64_t(minor) << 24) | uint64_t(patch)) { }

    uint32_t major() const { return uint32_t(m_raw >> 48); }
    uint32_t minor() const { return uint32_t(m_raw >> 24) & 0xffffffu; }
    uint32_t patch() const { return uint32_t(m_raw) & 0xffffffu; }

    bool operator == (const Version& other) const { return m_raw == other.m_raw; }
    bool operator != (const Version& other) const { return m_raw != other.m_raw; }
    bool operator >= (const Version& other) const { return m_raw >= other.m_raw; }
    bool operator <= (const Version& other) const { return m_raw <= other.m_raw; }
    bool operator >  (const Version& other) const { return m_raw >  other.m_raw; }
    bool operator <  (const Version& other) const { return m_raw <  other.m_raw; }

    std::string toString() const {
      return str::format(major(), ".", minor(), ".", patch());
    }

    explicit operator bool () const {
      return m_raw != 0;
    }

  private:

    uint64_t m_raw = 0;

  };

}
