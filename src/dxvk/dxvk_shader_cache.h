#pragma once

#include <array>
#include <fstream>
#include <optional>
#include <string>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../util/thread.h"
#include "../util/util_env.h"

#include "dxvk_shader_ir.h"

namespace dxvk {

  /**
   * \brief Shader cache
   *
   * On-disk cache for shaders using the internal IR.
   *
   * The implementation creates two files that can trivially grow by appending
   * data to them: A binary blob that contains the actual serialized IR as well
   * as shader metadata, and a look-up table
   */
  class DxvkShaderCache {

  public:

    struct FilePaths {
      std::string directory;
      std::string lutFile;
      std::string binFile;
    };

    DxvkShaderCache(FilePaths paths);

    ~DxvkShaderCache();

    void incRef() {
      m_useCount.fetch_add(1u, std::memory_order_acquire);
    }

    void decRef() {
      if (m_useCount.fetch_sub(1u, std::memory_order_release) == 1u)
        delete this;
    }

    /**
     * \brief Looks up shader with matching name and options
     *
     * \param [in] name Shader name
     * \param [in] options Shader properties and compile options
     * \returns Shader object, or \c nullptr if the shader in
     *    question could not be found in the cache.
     */
    Rc<DxvkIrShader> lookupShader(
      const std::string&                name,
      const DxvkIrShaderCreateInfo&     options);

    /**
     * \brief Writes shader to cache file
     *
     * The shader binary will be written asynchronously.
     * \param [in] shader Shader to write to cache
     */
    void addShader(Rc<DxvkIrShader> shader);

    /**
     * \brief Determines cache file path based on current environment and executable
     * \returns File paths and file names for cache files
     */
    static FilePaths getDefaultFilePaths();

  private:

    struct LutHeader {
      std::array<char, 4u>  magic = { };
      std::string           versionString = { };
    };

    struct LutKey {
      std::string name;
      DxvkIrShaderCreateInfo createInfo;

      size_t hash() const;

      bool eq(const LutKey& k) const;
    };

    struct LutEntry {
      uint64_t offset = 0u;
      uint32_t binarySize = 0u;
      uint32_t metadataSize = 0u;
      uint64_t checksum = 0u;
    };

    enum class Status : uint32_t {
      Uninitialized   = 0u,
      CacheDisabled   = 1u,
      OpenWriteOnly   = 2u,
      OpenReadWrite   = 3u,
    };

    std::atomic<uint32_t>         m_useCount = { 0u };

    FilePaths                     m_filePaths;
    dxvk::mutex                   m_fileMutex;

    std::fstream                  m_lutFile;
    std::fstream                  m_binFile;

    std::atomic<Status>           m_status = { Status::Uninitialized };

    std::unordered_map<LutKey, LutEntry, DxvkHash, DxvkEq> m_lut;

    dxvk::mutex                   m_writeMutex;
    dxvk::condition_variable      m_writeCond;
    std::queue<Rc<DxvkIrShader>>  m_writeQueue;

    dxvk::thread                  m_writer;

    bool ensureStatus(Status status);

    Status initialize();

    Status tryInitializeLocked();

    bool openReadWriteLocked();

    bool openWriteOnlyLocked();

    bool parseLut();

    Rc<DxvkIrShader> loadCachedShaderLocked(const LutKey& key, const LutEntry& entry);

    bool writeShaderLutEntry(DxvkIrShader& shader, const LutEntry& entry);

    bool writeShaderToCache(DxvkIrShader& shader);

    bool readShaderLutEntry(LutKey& key, LutEntry& entry);

    void runWriter();

    static bool writeShaderXfbInfo(std::ostream& stream, const dxbc_spv::ir::IoXfbInfo& xfb);

    static bool writeShaderCreateInfo(std::ostream& stream, const DxvkIrShaderCreateInfo& createInfo);

    static bool writeShaderLayout(std::ostream& stream, const DxvkPipelineLayoutBuilder& layout);

    static bool writeShaderIo(std::ostream& stream, const DxvkShaderIo& io);

    static bool writeShaderMetadata(std::ostream& stream, const DxvkShaderMetadata& metadata);

    static std::optional<LutEntry> writeShaderBinary(std::ostream& stream, DxvkIrShader& shader);

    static bool writeHeader(std::ostream& stream, const LutHeader& header);

    static bool readShaderIo(std::istream& stream, DxvkShaderIo& io);

    static bool readShaderXfbInfo(std::istream& stream, dxbc_spv::ir::IoXfbInfo& xfb);

    static bool readShaderLutKey(std::istream& stream, LutKey& key);

    static bool readShaderMetadata(std::istream& stream, DxvkShaderMetadata& metadata);

    static bool readShaderLayout(std::istream& stream, DxvkPipelineLayoutBuilder& layout);

    static bool writeBytes(std::ostream& stream, const char* data, size_t size) {
      return bool(stream.write(data, size));
    }

    static bool writeBytes(std::ostream& stream, const uint8_t* data, size_t size) {
      return writeBytes(stream, reinterpret_cast<const char*>(data), size);
    }

    static bool writeString(std::ostream& stream, const std::string& string) {
      return write(stream, uint16_t(string.size())) && writeBytes(stream, string.data(), string.size());
    }

    template<typename T, std::enable_if_t<std::is_trivially_copyable_v<T>, bool> = true>
    static bool write(std::ostream& stream, const T& data) {
      return writeBytes(stream, reinterpret_cast<const char*>(&data), sizeof(data));
    }

    static bool readBytes(std::istream& stream, char* data, size_t size) {
      return bool(stream.read(data, size));
    }

    static bool readBytes(std::istream& stream, uint8_t* data, size_t size) {
      return readBytes(stream, reinterpret_cast<char*>(data), size);
    }

    static bool readString(std::istream& stream, std::string& string) {
      uint16_t len = 0u;

      if (!read(stream, len))
        return false;

      string.resize(len);
      return readBytes(stream, string.data(), len);
    }

    template<typename T, std::enable_if_t<std::is_trivially_copyable_v<T>, bool> = true>
    static bool read(std::istream& stream, T& data) {
      return readBytes(stream, reinterpret_cast<char*>(&data), sizeof(data));
    }

  };

}
