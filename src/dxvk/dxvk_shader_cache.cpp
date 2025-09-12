#include <iomanip>
#include <version.h>

#include "dxvk_shader_cache.h"

#include "../util/util_time.h"

namespace dxvk {

  DxvkShaderCache::DxvkShaderCache(FilePaths paths)
  : m_filePaths(std::move(paths)) {

  }


  DxvkShaderCache::~DxvkShaderCache() {
    if (m_writer.joinable()) {
      { std::unique_lock lock(m_writeMutex);
        m_writeQueue.push(nullptr);
        m_writeCond.notify_one();
      }

      m_writer.join();
    }
  }


  Rc<DxvkIrShader> DxvkShaderCache::lookupShader(
    const std::string&                name,
    const DxvkIrShaderCreateInfo&     options) {
    if (!ensureStatus(Status::OpenReadWrite))
      return nullptr;

    LutKey k = { };
    k.name = name;
    k.createInfo = options;

    auto entry = m_lut.find(k);

    if (entry == m_lut.end()) {
      if (Logger::logLevel() <= LogLevel::Debug)
        Logger::debug(str::format("Shader cache miss: ", name));

      return nullptr;
    }

    if (Logger::logLevel() <= LogLevel::Debug) {
      Logger::debug(str::format("Shader cache hit: ", name,
        " (offset: ", entry->second.offset,
        ", size: ", entry->second.binarySize,
        ", metadata: ", entry->second.metadataSize, ")"));
    }

    std::unique_lock lock(m_fileMutex);
    auto shader = loadCachedShaderLocked(entry->first, entry->second);

    if (!shader) {
      Logger::warn(str::format("Failed to load cached shader ", name));

      if (!openWriteOnlyLocked())
        Logger::warn(str::format("Failed to re-initialize shader cache ", name));

      m_status.store(Status::OpenWriteOnly, std::memory_order_release);
    }

    return shader;
  }


  void DxvkShaderCache::addShader(Rc<DxvkIrShader> shader) {
    if (!ensureStatus(Status::OpenReadWrite))
      return;

    LutKey k = { };
    k.name = shader->debugName();
    k.createInfo = shader->getShaderCreateInfo();

    if (m_lut.find(k) == m_lut.end()) {
      std::unique_lock lock(m_writeMutex);
      m_writeQueue.push(std::move(shader));
      m_writeCond.notify_one();

      if (!m_writer.joinable())
        m_writer = dxvk::thread([this] { runWriter(); });
    }
  }


  bool DxvkShaderCache::ensureStatus(Status status) {
    auto currentStatus = m_status.load(std::memory_order_acquire);

    if (currentStatus == Status::Uninitialized)
      currentStatus = initialize();

    return currentStatus >= status;
  }


  DxvkShaderCache::Status DxvkShaderCache::initialize() {
    std::unique_lock lock(m_fileMutex);
    auto status = m_status.load(std::memory_order_relaxed);

    if (status != Status::Uninitialized)
      return status;

    status = tryInitializeLocked();

    m_status.store(status, std::memory_order_release);
    return status;
  }


  DxvkShaderCache::Status DxvkShaderCache::tryInitializeLocked() {
    if (m_filePaths.directory.empty() || m_filePaths.binFile.empty() || m_filePaths.lutFile.empty()) {
      Logger::warn("No path found for shader cache, consider setting DXVK_SHADER_CACHE_PATH.");
      return Status::CacheDisabled;
    }

    if (openReadWriteLocked()) {
      if (parseLut())
        return Status::OpenReadWrite;
    }

    if (openWriteOnlyLocked())
      return Status::OpenReadWrite;

    return Status::CacheDisabled;
  }


  bool DxvkShaderCache::openReadWriteLocked() {
    // Try to open both files in read-only mode for now, re-open
    // in read-write mode when we actually add new cache entries.
    auto path = m_filePaths.directory + env::PlatformDirSlash;
    auto iosFlags = std::ios_base::in | std::ios_base::out | std::ios_base::binary;

    m_binFile = std::fstream(path + m_filePaths.binFile, iosFlags);
    m_lutFile = std::fstream(path + m_filePaths.lutFile, iosFlags);

    if (!m_binFile.is_open() || !m_lutFile.is_open())
      return false;

    Logger::info(str::format("Found cache file: ", path + m_filePaths.binFile));
    return true;
  }


  bool DxvkShaderCache::openWriteOnlyLocked() {
    // Didn't have a lot of success so far, nuke the files and retry.
    auto path = m_filePaths.directory + env::PlatformDirSlash;
    auto iosFlags = std::ios_base::out | std::ios_base::binary | std::ios_base::trunc;

    m_binFile = std::fstream(path + m_filePaths.binFile, iosFlags);
    m_lutFile = std::fstream(path + m_filePaths.lutFile, iosFlags);

    if (!m_binFile.is_open() || !m_lutFile.is_open()) {
      if (!env::createDirectory(m_filePaths.directory)) {
        Logger::warn(str::format("Failed to create directory: ", m_filePaths.directory));
        return false;
      }

      m_binFile = std::fstream(path + m_filePaths.binFile, iosFlags);
      m_lutFile = std::fstream(path + m_filePaths.lutFile, iosFlags);
    }

    if (!m_binFile.is_open())
      Logger::warn(str::format("Failed to create ", path + m_filePaths.binFile, ", disabling cache"));

    if (!m_lutFile.is_open())
      Logger::warn(str::format("Failed to create ", path + m_filePaths.lutFile, ", disabling cache"));

    if (!m_binFile.is_open() || !m_lutFile.is_open())
      return false;

    Logger::info(str::format("Created cache file: ", path + m_filePaths.binFile));

    LutHeader header = { };
    header.magic = { 'D', 'X', 'V', 'K' };
    header.versionString = DXVK_VERSION;

    if (!writeHeader(m_lutFile, header)) {
      Logger::warn(str::format("Failed to write cache header: ", path + m_filePaths.lutFile));
      return false;
    }

    return true;
  }


  bool DxvkShaderCache::parseLut() {
    LutHeader header;

    m_lutFile.seekg(0, std::ios_base::end);
    auto size = m_lutFile.tellg();
    m_lutFile.seekg(0, std::ios_base::beg);

    if (!readBytes(m_lutFile, header.magic.data(), header.magic.size())
     || !readString(m_lutFile, header.versionString)) {
      Logger::warn("Failed to parse cache file header.");
      return false;
    }

    if (header.versionString != DXVK_VERSION) {
      Logger::warn(str::format("Cache was created with DXVK version ", header.versionString,
        ", but current version is ", DXVK_VERSION, ". Discarding old cache."));
      return false;
    }

    while (m_lutFile.tellg() != size) {
      LutKey k;
      LutEntry e;

      if (!readShaderLutEntry(k, e)) {
        Logger::warn("Failed to parse cache look-up table.");
        return false;
      }

      m_lut.insert_or_assign(k, e);
    }

    return true;
  }


  bool DxvkShaderCache::writeShaderXfbInfo(std::ostream& stream, const dxbc_spv::ir::IoXfbInfo& xfb) {
    return writeString(stream, xfb.semanticName)
        && write(stream, xfb.semanticIndex)
        && write(stream, xfb.componentMask)
        && write(stream, xfb.stream)
        && write(stream, xfb.buffer)
        && write(stream, xfb.offset)
        && write(stream, xfb.stride);
  }


  bool DxvkShaderCache::writeShaderCreateInfo(std::ostream& stream, const DxvkIrShaderCreateInfo& createInfo) {
    bool status = write(stream, createInfo.options)
               && write(stream, createInfo.flatShadingInputs)
               && write(stream, createInfo.rasterizedStream);

    status = status && write(stream, uint32_t(createInfo.xfbEntries.size()));

    for (const auto& xfb : createInfo.xfbEntries)
      status = status && writeShaderXfbInfo(stream, xfb);

    return status;
  }


  Rc<DxvkIrShader> DxvkShaderCache::loadCachedShaderLocked(const LutKey& key, const LutEntry& entry) {
    std::vector<uint8_t> ir(entry.binarySize);

    m_binFile.seekg(entry.offset, std::ios_base::beg);

    if (!readBytes(m_binFile, ir.data(), ir.size())) {
      Logger::warn("Failed to read cached shader binary");
      return nullptr;
    }

    if (entry.checksum != bit::fnv1a_hash(ir.data(), ir.size())) {
      Logger::warn("Checksum mismatch for cached shader");
      return nullptr;
    }

    DxvkShaderMetadata metadata;

    if (!readShaderMetadata(m_binFile, metadata)) {
      Logger::warn("Failed to read cached shader metadata");
      return nullptr;
    }

    DxvkPipelineLayoutBuilder layout;

    if (!readShaderLayout(m_binFile, layout)) {
      Logger::warn("Failed to read cached shader binding layout");
      return nullptr;
    }

    return new DxvkIrShader(key.name, key.createInfo, std::move(metadata), std::move(layout), std::move(ir));
  }


  bool DxvkShaderCache::writeShaderLutEntry(DxvkIrShader& shader, const LutEntry& entry) {
    return writeString(m_lutFile, shader.debugName())
        && writeShaderCreateInfo(m_lutFile, shader.getShaderCreateInfo())
        && write(m_lutFile, entry);
  }


  bool DxvkShaderCache::writeShaderToCache(DxvkIrShader& shader) {
    auto entry = writeShaderBinary(m_binFile, shader);

    if (!entry)
      return false;

    return writeShaderLutEntry(shader, *entry);
  }


  bool DxvkShaderCache::readShaderIo(std::istream& stream, DxvkShaderIo& io) {
    uint8_t varCount = 0u;

    if (!read(stream, varCount))
      return false;

    for (uint32_t i = 0u; i < varCount; i++) {
      DxvkShaderIoVar var = { };

      if (!read(stream, var))
        return false;

      io.add(var);
    }

    return true;
  }


  bool DxvkShaderCache::readShaderMetadata(std::istream& stream, DxvkShaderMetadata& metadata) {
    bool status = read(stream, metadata.stage)
               && read(stream, metadata.flags)
               && read(stream, metadata.specConstantMask)
               && readShaderIo(stream, metadata.inputs)
               && readShaderIo(stream, metadata.outputs)
               && read(stream, metadata.inputTopology)
               && read(stream, metadata.outputTopology)
               && read(stream, metadata.flatShadingInputs)
               && read(stream, metadata.rasterizedStream)
               && read(stream, metadata.patchVertexCount);

    for (auto& xfb : metadata.xfbStrides)
      status = status && read(stream, xfb);

    return status;
  }


  bool DxvkShaderCache::readShaderLayout(std::istream& stream, DxvkPipelineLayoutBuilder& layout) {
    VkShaderStageFlags stageMask = { };

    if (!read(stream, stageMask))
      return false;

    layout = DxvkPipelineLayoutBuilder(stageMask);

    // Read push data blocks
    uint32_t pushDataMask = 0u;

    if (!read(stream, pushDataMask))
      return false;

    for (uint32_t i = 0u; i < bit::popcnt(pushDataMask); i++) {
      DxvkPushDataBlock block = { };

      if (!read(stream, block))
        return false;

      layout.addPushData(block);
    }

    // Read shader binding info
    uint32_t bindingCount = 0u;

    if (!read(stream, bindingCount))
      return false;

    for (uint32_t i = 0u; i < bindingCount; i++) {
      DxvkShaderDescriptor binding = { };

      if (!read(stream, binding))
        return false;

      layout.addBindings(1u, &binding);
    }

    // Read sampler heap mappings
    uint32_t samplerHeapCount = 0u;

    if (!read(stream, samplerHeapCount))
      return false;

    for (uint32_t i = 0u; i < samplerHeapCount; i++) {
      DxvkShaderBinding binding = { };

      if (!read(stream, binding))
        return false;

      layout.addSamplerHeap(binding);
    }

    return true;
  }


  bool DxvkShaderCache::readShaderXfbInfo(std::istream& stream, dxbc_spv::ir::IoXfbInfo& xfb) {
    return readString(stream, xfb.semanticName)
        && read(stream, xfb.semanticIndex)
        && read(stream, xfb.componentMask)
        && read(stream, xfb.stream)
        && read(stream, xfb.buffer)
        && read(stream, xfb.offset)
        && read(stream, xfb.stride);
 }


  bool DxvkShaderCache::readShaderLutKey(std::istream& stream, LutKey& key) {
    bool status = readString(stream, key.name)
               && read(stream, key.createInfo.options)
               && read(stream, key.createInfo.flatShadingInputs)
               && read(stream, key.createInfo.rasterizedStream);

    uint32_t xfbCount = 0u;
    status = status && read(stream, xfbCount);

    key.createInfo.xfbEntries.resize(xfbCount);

    for (uint32_t i = 0u; i < xfbCount; i++)
      status = status && readShaderXfbInfo(stream, key.createInfo.xfbEntries[i]);

    return status;
  }


  bool DxvkShaderCache::readShaderLutEntry(LutKey& key, LutEntry& entry) {
    return readShaderLutKey(m_lutFile, key) && read(m_lutFile, entry);
  }


  void DxvkShaderCache::runWriter() {
    small_vector<Rc<DxvkIrShader>, 128u> localQueue;

    env::setThreadName("dxvk-cache");

    bool stop = false;

    while (!stop) {
      std::unique_lock lock(m_writeMutex);

      m_writeCond.wait(lock, [this] {
        return !m_writeQueue.empty();
      });

      auto entry = std::move(m_writeQueue.front());
      m_writeQueue.pop();

      lock.unlock();

      stop = !entry;
      bool drain = stop;

      if (entry) {
        localQueue.push_back(std::move(entry));
        drain = localQueue.size() == localQueue.capacity();
      }

      if (drain) {
        std::unique_lock fileLock(m_fileMutex);

        m_binFile.seekp(0, std::ios_base::end);
        m_lutFile.seekp(0, std::ios_base::end);

        for (const auto& shader : localQueue) {
          if (!writeShaderToCache(*shader)) {
            Logger::err("Failed to write cache file.");
            m_status = Status::CacheDisabled;
            return;
          }
        }

        localQueue.clear();

        m_binFile.flush();
        m_lutFile.flush();
      }
    }
  }


  bool DxvkShaderCache::writeShaderLayout(std::ostream& stream, const DxvkPipelineLayoutBuilder& layout) {
    bool status = write(stream, layout.getStageMask())
               && write(stream, layout.getPushDataMask());

    for (auto pushIndex : bit::BitMask(layout.getPushDataMask()))
      status = status && write(stream, layout.getPushDataBlock(pushIndex));

    auto bindings = layout.getBindings();
    status = status && write(stream, uint32_t(bindings.bindingCount));

    for (size_t i = 0u; i < bindings.bindingCount; i++)
      status = status && write(stream, bindings.bindings[i]);

    status = status && write(stream, uint32_t(layout.getSamplerHeapBindingCount()));

    for (size_t i = 0u; i < layout.getSamplerHeapBindingCount(); i++)
      status = status && write(stream, layout.getSamplerHeapBinding(i));

    return status;
  }


  bool DxvkShaderCache::writeShaderIo(std::ostream& stream, const DxvkShaderIo& io) {
    bool status = write(stream, uint8_t(io.getVarCount()));

    for (uint32_t i = 0u; i < io.getVarCount(); i++)
      status = status && write(stream, io.getVar(i));

    return status;
  }


  bool DxvkShaderCache::writeShaderMetadata(std::ostream& stream, const DxvkShaderMetadata& metadata) {
    bool status = write(stream, metadata.stage)
               && write(stream, metadata.flags)
               && write(stream, metadata.specConstantMask)
               && writeShaderIo(stream, metadata.inputs)
               && writeShaderIo(stream, metadata.outputs)
               && write(stream, metadata.inputTopology)
               && write(stream, metadata.outputTopology)
               && write(stream, metadata.flatShadingInputs)
               && write(stream, metadata.rasterizedStream)
               && write(stream, metadata.patchVertexCount);

    for (const auto& xfb : metadata.xfbStrides)
      status = status && write(stream, xfb);

    return status;
  }


  std::optional<DxvkShaderCache::LutEntry> DxvkShaderCache::writeShaderBinary(std::ostream& stream, DxvkIrShader& shader) {
    auto [data, size] = shader.getSerializedIr();

    LutEntry entry = { };
    entry.offset = stream.tellp();
    entry.binarySize = size;

    if (!writeBytes(stream, data, size)
     || !writeShaderMetadata(stream, shader.getShaderMetadata())
     || !writeShaderLayout(stream, shader.getLayout()))
      return std::nullopt;

    entry.metadataSize = uint32_t(uint64_t(stream.tellp()) - (entry.offset + entry.binarySize));
    entry.checksum = bit::fnv1a_hash(data, size);
    return std::make_optional(entry);
  }


  bool DxvkShaderCache::writeHeader(std::ostream& stream, const LutHeader& header) {
    return writeBytes(stream, header.magic.data(), header.magic.size())
        && writeString(stream, header.versionString);
  }


  DxvkShaderCache::FilePaths DxvkShaderCache::getDefaultFilePaths() {
    std::string cachePath = env::getEnvVar("DXVK_SHADER_CACHE_PATH");

    if (cachePath.empty()) {
      #ifdef _WIN32
      cachePath = env::getEnvVar("LOCALAPPDATA");
      #endif

      if (cachePath.empty())
        cachePath = env::getEnvVar("XDG_CACHE_HOME");

      if (cachePath.empty()) {
        cachePath = env::getEnvVar("HOME");

        if (!cachePath.empty()) {
          cachePath += env::PlatformDirSlash;
          cachePath += ".cache";
        }
      }

      if (!cachePath.empty()) {
        cachePath += env::PlatformDirSlash;
        cachePath += "dxvk";
      }
    }

    if (cachePath.empty())
      return FilePaths();

    // Determine file name based on the actual executable,
    // including the containing directory.
    std::string exePath = env::getExePath();

    if (exePath.empty())
      return FilePaths();

    size_t pathStart = exePath.find_last_of(env::PlatformDirSlash);

    if (pathStart != std::string::npos)
      pathStart = exePath.find_last_of(env::PlatformDirSlash, pathStart);

    if (pathStart == std::string::npos)
      pathStart = 0u;

    uint64_t hash = bit::fnv1a_init();

    for (size_t i = pathStart; i < exePath.size(); i++)
      hash = bit::fnv1a_iter(hash, uint8_t(exePath[i]));

    std::string baseName = str::format(std::hex, std::setw(16u), std::setfill('0'), hash);

    FilePaths paths;
    paths.directory = cachePath;
    paths.lutFile = baseName + ".dxvk.lut";
    paths.binFile = baseName + ".dxvk.bin";
    return paths;
  }


  size_t DxvkShaderCache::LutKey::hash() const {
    DxvkHashState hash;
    hash.add(bit::fnv1a_hash(name.data(), name.size()));
    hash.add(createInfo.hash());
    return hash;
  }


  bool DxvkShaderCache::LutKey::eq(const LutKey& k) const {
    return name == k.name && createInfo.eq(k.createInfo);
  }

}
