#pragma once

#include "dxso_include.h"

#include <cstdint>

namespace dxvk {

  /**
   * \brief Four-character tag
   */
  class DxsoTag {

  public:

    DxsoTag() {
      for (size_t i = 0; i < 4; i++)
        m_chars[i] = '\0';
    }

    DxsoTag(const char* tag) {
      for (size_t i = 0; i < 4; i++)
        m_chars[i] = tag[i];
    }

    bool operator == (const DxsoTag& other) const {
      bool result = true;
      for (size_t i = 0; i < 4; i++)
        result &= m_chars[i] == other.m_chars[i];
      return result;
    }

    bool operator != (const DxsoTag& other) const {
      return !this->operator == (other);
    }

    const char* operator & () const { return m_chars; }
          char* operator & ()       { return m_chars; }

  private:

    char m_chars[4];

  };


  /**
    * \brief DXSO (d3d9) bytecode reader
    *
    * Holds references to the shader byte code and
    * provides methods to read
    */
  class DxsoReader {

  public:

    DxsoReader(const char* data)
      : DxsoReader(data, 0) { }

    size_t pos() {
      return m_pos;
    }

    auto readu32() { return this->readNum<uint32_t> (); }
    auto readf32() { return this->readNum<float>    (); }

    DxsoTag readTag();

    void read(void* dst, size_t n);

    void skip(size_t n);

    void store(std::ostream&& stream, size_t size) const;

    const char* currentPtr() {
      return m_data + m_pos;
    }

  private:

    DxsoReader(const char* data, size_t pos)
      : m_data(data), m_pos(pos) { }

    const char* m_data = nullptr;
    size_t      m_pos = 0;

    template<typename T>
    T readNum() {
      T result;
      this->read(&result, sizeof(result));
      return result;
    }

  };

}
