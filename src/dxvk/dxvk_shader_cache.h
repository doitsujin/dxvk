#pragma once

#include <array>
#include <optional>
#include <string>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../util/thread.h"
#include "../util/util_env.h"
#include "../util/util_file.h"

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

    ~DxvkShaderCache();

    void incRef() {
      m_useCount.fetch_add(1u, std::memory_order_acquire);
    }

    void decRef() {
      if (m_useCount.fetch_sub(1u, std::memory_order_release) == 1u)
        freeInstance();
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

    /**
     * \brief Initializes shader cache
     * \returns Shader cache instance
     */
    static Rc<DxvkShaderCache> getInstance();

  private:

    struct Instance {
      dxvk::mutex       mutex;
      DxvkShaderCache*  instance = nullptr;
    };

    static Instance s_instance;

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

    util::File                    m_lutFile;
    util::File                    m_binFile;

    std::atomic<Status>           m_status = { Status::Uninitialized };

    std::unordered_map<LutKey, LutEntry, DxvkHash, DxvkEq> m_lut;

    dxvk::mutex                   m_writeMutex;
    dxvk::condition_variable      m_writeCond;
    std::queue<Rc<DxvkIrShader>>  m_writeQueue;

    dxvk::thread                  m_writer;

    DxvkShaderCache();

    bool ensureStatus(Status status);

    Status initialize();

    Status tryInitializeLocked();

    bool openReadWriteLocked();

    bool openWriteOnlyLocked();

    bool parseLut();

    Rc<DxvkIrShader> loadCachedShaderLocked(const LutKey& key, const LutEntry& entry);

    bool writeShaderLutEntry(DxvkIrShader& shader, const LutEntry& entry);

    bool writeShaderToCache(DxvkIrShader& shader);

    bool readShaderLutEntry(LutKey& key, LutEntry& entry, size_t& offset);

    void runWriter();

    void freeInstance();

    static bool writeShaderXfbInfo(util::File& stream, const dxbc_spv::ir::IoXfbInfo& xfb);

    static bool writeShaderCreateInfo(util::File& stream, const DxvkIrShaderCreateInfo& createInfo);

    static bool writeShaderLayout(util::File& stream, const DxvkPipelineLayoutBuilder& layout);

    static bool writeShaderIo(util::File& stream, const DxvkShaderIo& io);

    static bool writeShaderMetadata(util::File& stream, const DxvkShaderMetadata& metadata);

    static std::optional<LutEntry> writeShaderBinary(util::File& stream, DxvkIrShader& shader);

    static bool writeHeader(util::File& stream, const LutHeader& header);

    static bool readShaderIo(util::File& stream, size_t& offset, DxvkShaderIo& io);

    static bool readShaderXfbInfo(util::File& stream, size_t& offset, dxbc_spv::ir::IoXfbInfo& xfb);

    static bool readShaderLutKey(util::File& stream, size_t& offset, LutKey& key);

    static bool readShaderMetadata(util::File& stream, size_t& offset, DxvkShaderMetadata& metadata);

    static bool readShaderLayout(util::File& stream, size_t& offset, DxvkPipelineLayoutBuilder& layout);

    static bool writeBytes(util::File& stream, const char* data, size_t size) {
      return stream.append(size, data);
    }

    static bool writeBytes(util::File& stream, const uint8_t* data, size_t size) {
      return writeBytes(stream, reinterpret_cast<const char*>(data), size);
    }

    static bool writeString(util::File& stream, const std::string& string) {
      return write(stream, uint16_t(string.size())) && writeBytes(stream, string.data(), string.size());
    }

    template<typename T, std::enable_if_t<std::is_trivially_copyable_v<T>, bool> = true>
    static bool write(util::File& stream, const T& data) {
      return writeBytes(stream, reinterpret_cast<const char*>(&data), sizeof(data));
    }

    static bool readBytes(util::File& stream, char* data, size_t& offset, size_t size) {
      bool result = stream.read(offset, size, data);
      offset += size;
      return result;
    }

    static bool readBytes(util::File& stream, uint8_t* data, size_t& offset, size_t size) {
      return readBytes(stream, reinterpret_cast<char*>(data), offset, size);
    }

    static bool readString(util::File& stream, size_t& offset, std::string& string) {
      uint16_t len = 0u;

      if (!read(stream, offset, len))
        return false;

      string.resize(len);
      return readBytes(stream, string.data(), offset, len);
    }

    template<typename T, std::enable_if_t<std::is_trivially_copyable_v<T>, bool> = true>
    static bool read(util::File& stream, size_t& offset, T& data) {
      return readBytes(stream, reinterpret_cast<char*>(&data), offset, sizeof(data));
    }

  };

}
