#pragma once

#include "dxso_include.h"

#include "../dxbc/dxbc_tag.h"

#include <cstdint>

namespace dxvk {

  /**
    * \brief DXSO (d3d9) bytecode reader
    *
    * Holds references to the shader byte code and
    * provides methods to read
    */
  class DxsoReader {

  public:
    constexpr static size_t TokenSize = sizeof(uint32_t);

    DxsoReader(const char* data, size_t size)
      : DxsoReader(data, size, 0) { }

    size_t size() {
      return m_size;
    }

    size_t pos() {
      return m_pos;
    }

    size_t remaining() {
      return m_size - m_pos;
    }

    auto readu32() { return this->readNum<uint32_t> (); }
    auto readf32() { return this->readNum<float>    (); }

    DxbcTag readTag();

    void read(void* dst, size_t n);

    void skip(size_t n);
    void skipTokens(size_t n) { skip(n * TokenSize); }

    DxsoReader clone(size_t pos) const;

    DxsoReader resize(size_t size) const;

    bool eof() const {
      return m_pos >= m_size;
    }

    void store(std::ostream&& stream) const;

  private:

    DxsoReader(const char* data, size_t size, size_t pos)
      : m_data(data), m_size(size), m_pos(pos) { }

    const char* m_data = nullptr;
    size_t      m_size = 0;
    size_t      m_pos = 0;

    template<typename T>
    T readNum() {
      T result;
      this->read(&result, sizeof(result));
      return result;
    }

  };

}