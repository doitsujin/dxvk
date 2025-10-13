#include <fstream>

#include "util_file.h"

namespace dxvk::util {

  class StlFile : public FileIface {

  public:

    StlFile(const std::string& path, FileFlags flags) {
      std::ios_base::openmode mode = std::ios_base::binary;

      if (flags.test(FileFlag::AllowRead))
        mode |= std::ios_base::in;

      if (flags.test(FileFlag::AllowWrite))
        mode |= std::ios_base::out;

      if (flags.test(FileFlag::Truncate))
        mode |= std::ios_base::trunc;

      m_file.open(path, mode);
    }

    ~StlFile() {

    }

    bool read(size_t offset, size_t size, void* data) {
      if (!status())
        return false;

      if (!m_file.seekg(offset, std::ios_base::beg))
        return false;

      return bool(m_file.read(reinterpret_cast<char*>(data), size));
    }

    bool write(size_t offset, size_t size, const void* data) {
      if (!status())
        return false;

      if (!m_file.seekp(offset, std::ios_base::beg))
        return false;

      return bool(m_file.write(reinterpret_cast<const char*>(data), size));
    }

    bool append(size_t size, const void* data) {
      if (!status())
        return false;

      if (!m_file.seekp(0, std::ios_base::end))
        return false;

      return bool(m_file.write(reinterpret_cast<const char*>(data), size));
    }

    size_t size() {
      if (status()) {
        if (m_flags.test(FileFlag::AllowWrite)) {
          if (m_file.seekp(0, std::ios_base::end))
            return size_t(m_file.tellp());
        } else {
          if (m_file.seekg(0, std::ios_base::end))
            return size_t(m_file.tellg());
        }
      }

      return 0u;
    }

    bool status() const {
      return m_file.is_open() && m_file;
    }

    bool flush() {
      if (!status())
        return false;

      m_file.flush();
      return true;
    }

  private:

    FileFlags     m_flags = { };
    std::fstream  m_file;

  };

  using FileImpl = StlFile;


  FileIface::~FileIface() {

  }


  File::File() {

  }

  File::File(const std::string& path, FileFlags flags)
  : m_impl(new FileImpl(path, flags)) {

  }

  File::File(File&& other)
  : m_impl(std::move(other.m_impl)) {

  }

  File& File::operator = (File&& other) {
    if (&other != this) {
      m_impl = nullptr;
      m_impl = std::move(other.m_impl);
    }

    return *this;
  }

  File::~File() {

  }

  bool File::open(const std::string& path, FileFlags flags) {
    m_impl = nullptr;
    m_impl = new FileImpl(path, flags);
    return m_impl->status();
  }

  bool File::read(size_t offset, size_t size, void* data) {
    return m_impl && m_impl->read(offset, size, data);
  }

  bool File::write(size_t offset, size_t size, const void* data) {
    return m_impl && m_impl->write(offset, size, data);
  }

  bool File::append(size_t size, const void* data) {
    return m_impl && m_impl->append(size, data);
  }

  size_t File::size() {
    if (!m_impl)
      return 0u;

    return m_impl->size();
  }

  bool File::flush() {
    return m_impl && m_impl->flush();
  }

  File::operator bool () const {
    return m_impl && m_impl->status();
  }

}
