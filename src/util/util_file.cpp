#include <fstream>

#include "./com/com_include.h"

#include "./log/log.h"

#include "util_file.h"
#include "util_string.h"

namespace dxvk::util {

#ifdef _WIN32
  class Win32File : public FileIface {

  public:

    Win32File(const std::string& path, FileFlags flags)
    : m_flags(flags) {
      DWORD access = 0u;
      DWORD share = 0u;
      DWORD mode = 0u;

      if (flags.test(FileFlag::AllowRead)) {
        access |= GENERIC_READ;
        share |= FILE_SHARE_READ;
        mode = OPEN_EXISTING;
      }

      if (flags.test(FileFlag::AllowWrite)) {
        access |= GENERIC_WRITE;
        share |= FILE_SHARE_WRITE;
        mode = OPEN_EXISTING;

        if (flags.test(FileFlag::Truncate))
          mode = CREATE_ALWAYS;
      }

      if (flags.test(FileFlag::Exclusive))
        share = 0u;

      std::array<WCHAR, MAX_PATH + 1u> pathCvt;

      size_t len = str::transcodeString(
        pathCvt.data(), pathCvt.size(),
        path.data(), path.size());

      pathCvt[len] = '\0';

      m_file = CreateFileW(pathCvt.data(),
        access, share, nullptr, mode, FILE_ATTRIBUTE_NORMAL, nullptr);

      if (!m_file)
        m_file = INVALID_HANDLE_VALUE;
    }

    ~Win32File() {
      CloseHandle(m_file);
    }

    bool read(size_t offset, size_t size, void* data) {
      if (!seek(offset, FILE_BEGIN))
        return false;

      return read(size, data);
    }

    bool write(size_t offset, size_t size, const void* data) {
      if (!seek(offset, FILE_BEGIN))
        return false;

      return write(size, data);
    }

    bool append(size_t size, const void* data) {
      if (!seek(0, FILE_END))
        return false;

      return write(size, data);
    }

    size_t size() {
      if (m_file == INVALID_HANDLE_VALUE)
        return 0u;

      LARGE_INTEGER size = { };

      if (!GetFileSizeEx(m_file, &size))
        return 0u;

      return size.QuadPart;
    }

    bool status() const {
      return m_file != INVALID_HANDLE_VALUE;
    }

    bool flush() {
      return FlushFileBuffers(m_file);
    }

  private:

    FileFlags m_flags = { };
    HANDLE    m_file  = INVALID_HANDLE_VALUE;

    bool seek(size_t offset, DWORD method) {
      if (!m_file)
        return false;

      LARGE_INTEGER address = { };
      address.QuadPart = offset;

      return SetFilePointerEx(m_file, address, nullptr, method);
    }

    bool read(size_t size, void* data) {
      auto buffer = reinterpret_cast<char*>(data);

      while (size) {
        DWORD read = 0u;

        if (!ReadFile(m_file, buffer, size, &read, nullptr) || !read)
          return false;

        buffer += read;
        size -= read;
      }

      return true;
    }

    bool write(size_t size, const void* data) {
      auto buffer = reinterpret_cast<const char*>(data);

      while (size) {
        DWORD written = 0u;

        if (!WriteFile(m_file, buffer, size, &written, nullptr) || !written)
          return false;

        buffer += written;
        size -= written;
      }

      return true;
    }

  };

  using FileImpl = Win32File;
#else

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
#endif

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
