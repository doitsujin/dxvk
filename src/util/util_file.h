#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "util_flags.h"
#include "util_likely.h"

#include "./rc/util_rc_ptr.h"

namespace dxvk::util {

  /**
   * \brief File flags
   */
  enum class FileFlag : uint32_t {
    AllowRead       = 0,
    AllowWrite      = 1,
    Truncate        = 2,
    Exclusive       = 3,
  };

  using FileFlags = Flags<FileFlag>;


  /**
   * \brief Platform-specific file interface
   */
  class FileIface {

  public:

    virtual ~FileIface();

    virtual bool read(size_t offset, size_t size, void* data) = 0;

    virtual bool write(size_t offset, size_t size, const void* data) = 0;

    virtual bool append(size_t size, const void* data) = 0;

    virtual size_t size() = 0;

    virtual bool status() const = 0;

    virtual bool flush() = 0;

    force_inline void incRef() {
      m_refCount.fetch_add(1u, std::memory_order_acquire);
    }

    force_inline void decRef() {
      if (m_refCount.fetch_sub(1u, std::memory_order_acquire) == 1u)
        delete this;
    }

  private:

    std::atomic<uint32_t> m_refCount = { 0u };

  };


  /**
   * \brief Generic file interface
   *
   * Provides a basic API for exclusive file I/O, which is
   * not (yet) available in the cpp standard library.
   * Note that this file API is not thread-safe.
   */
  class File {

  public:

    File();

    File(const std::string& path, FileFlags flags);

    File(File&& other);

    File& operator = (File&& other);

    ~File();

    bool open(const std::string& path, FileFlags flags);

    bool read(size_t offset, size_t size, void* data);

    bool write(size_t offset, size_t size, const void* data);

    bool append(size_t size, const void* data);

    size_t size();

    bool flush();

    explicit operator bool () const;

  private:

    Rc<FileIface> m_impl;

  };

}
