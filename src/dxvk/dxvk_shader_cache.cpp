#include <iomanip>
#include <version.h>

#include "dxvk_shader_cache.h"

#include "../util/util_time.h"

namespace dxvk {

  DxvkShaderCache::Instance DxvkShaderCache::s_instance;

  DxvkShaderCache::DxvkShaderCache()
  : m_filePaths(getDefaultFilePaths()) {

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

    auto flags = util::FileFlags(
      util::FileFlag::AllowRead,
      util::FileFlag::AllowWrite,
      util::FileFlag::Exclusive);

    m_binFile.open(path + m_filePaths.binFile, flags);
    m_lutFile.open(path + m_filePaths.lutFile, flags);

    if (!m_binFile || !m_lutFile)
      return false;

    Logger::info(str::format("Found cache file: ", path + m_filePaths.binFile));
    return true;
  }


  bool DxvkShaderCache::openWriteOnlyLocked() {
    // Didn't have a lot of success so far, nuke the files and retry.
    auto path = m_filePaths.directory + env::PlatformDirSlash;

    auto flags = util::FileFlags(
      util::FileFlag::AllowWrite,
      util::FileFlag::Truncate,
      util::FileFlag::Exclusive);

    m_binFile.open(path + m_filePaths.binFile, flags);
    m_lutFile.open(path + m_filePaths.lutFile, flags);

    if (!m_binFile || !m_lutFile) {
      if (!env::createDirectory(m_filePaths.directory)) {
        Logger::warn(str::format("Failed to create directory: ", m_filePaths.directory));
        return false;
      }

      m_binFile.open(path + m_filePaths.binFile, flags);
      m_lutFile.open(path + m_filePaths.lutFile, flags);
    }

    if (!m_binFile)
      Logger::warn(str::format("Failed to create ", path + m_filePaths.binFile, ", disabling cache"));

    if (!m_lutFile)
      Logger::warn(str::format("Failed to create ", path + m_filePaths.lutFile, ", disabling cache"));

    if (!m_binFile || !m_lutFile)
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

    size_t size = m_lutFile.size();
    size_t offset = 0u;

    if (!readBytes(m_lutFile, header.magic.data(), offset, header.magic.size())
     || !readString(m_lutFile, offset, header.versionString)) {
      Logger::warn("Failed to parse cache file header.");
      return false;
    }

    if (header.versionString != DXVK_VERSION) {
      Logger::warn(str::format("Cache was created with DXVK version ", header.versionString,
        ", but current version is ", DXVK_VERSION, ". Discarding old cache."));
      return false;
    }

    while (offset < size) {
      LutKey k;
      LutEntry e;

      if (!readShaderLutEntry(k, e, offset)) {
        Logger::warn("Failed to parse cache look-up table.");
        return false;
      }

      m_lut.insert_or_assign(k, e);
    }

    return true;
  }


  bool DxvkShaderCache::writeShaderXfbInfo(util::File& stream, const dxbc_spv::ir::IoXfbInfo& xfb) {
    return writeString(stream, xfb.semanticName)
        && write(stream, xfb.semanticIndex)
        && write(stream, xfb.componentMask)
        && write(stream, xfb.stream)
        && write(stream, xfb.buffer)
        && write(stream, xfb.offset)
        && write(stream, xfb.stride);
  }


  bool DxvkShaderCache::writeShaderCreateInfo(util::File& stream, const DxvkIrShaderCreateInfo& createInfo) {
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

    size_t offset = entry.offset;

    if (!readBytes(m_binFile, ir.data(), offset, entry.binarySize)) {
      Logger::warn("Failed to read cached shader binary");
      return nullptr;
    }

    if (entry.checksum != bit::fnv1a_hash(ir.data(), ir.size())) {
      Logger::warn("Checksum mismatch for cached shader");
      return nullptr;
    }

    DxvkShaderMetadata metadata;

    if (!readShaderMetadata(m_binFile, offset, metadata)) {
      Logger::warn("Failed to read cached shader metadata");
      return nullptr;
    }

    DxvkPipelineLayoutBuilder layout;

    if (!readShaderLayout(m_binFile, offset, layout)) {
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


  bool DxvkShaderCache::readShaderIo(util::File& stream, size_t& offset, DxvkShaderIo& io) {
    uint8_t varCount = 0u;

    if (!read(stream, offset, varCount))
      return false;

    for (uint32_t i = 0u; i < varCount; i++) {
      DxvkShaderIoVar var = { };

      if (!read(stream, offset, var.builtIn)
       || !read(stream, offset, var.location)
       || !read(stream, offset, var.componentIndex)
       || !read(stream, offset, var.componentCount)
       || !read(stream, offset, var.isPatchConstant)
       || !read(stream, offset, var.semanticIndex)
       || !readString(stream, offset, var.semanticName))
        return false;

      io.add(std::move(var));
    }

    return true;
  }


  bool DxvkShaderCache::readShaderMetadata(util::File& stream, size_t& offset, DxvkShaderMetadata& metadata) {
    bool status = read(stream, offset, metadata.stage)
               && read(stream, offset, metadata.flags)
               && read(stream, offset, metadata.specConstantMask)
               && readShaderIo(stream, offset, metadata.inputs)
               && readShaderIo(stream, offset, metadata.outputs)
               && read(stream, offset, metadata.inputTopology)
               && read(stream, offset, metadata.outputTopology)
               && read(stream, offset, metadata.flatShadingInputs)
               && read(stream, offset, metadata.rasterizedStream)
               && read(stream, offset, metadata.patchVertexCount);

    for (auto& xfb : metadata.xfbStrides)
      status = status && read(stream, offset, xfb);

    return status;
  }


  bool DxvkShaderCache::readShaderLayout(util::File& stream, size_t& offset, DxvkPipelineLayoutBuilder& layout) {
    VkShaderStageFlags stageMask = { };

    if (!read(stream, offset, stageMask))
      return false;

    layout = DxvkPipelineLayoutBuilder(stageMask);

    // Read push data blocks
    uint32_t pushDataMask = 0u;

    if (!read(stream, offset, pushDataMask))
      return false;

    for (uint32_t i = 0u; i < bit::popcnt(pushDataMask); i++) {
      DxvkPushDataBlock block = { };

      if (!read(stream, offset, block))
        return false;

      layout.addPushData(block);
    }

    // Read shader binding info
    uint32_t bindingCount = 0u;

    if (!read(stream, offset, bindingCount))
      return false;

    for (uint32_t i = 0u; i < bindingCount; i++) {
      DxvkShaderDescriptor binding = { };

      if (!read(stream, offset, binding))
        return false;

      layout.addBindings(1u, &binding);
    }

    // Read sampler heap mappings
    uint32_t samplerHeapCount = 0u;

    if (!read(stream, offset, samplerHeapCount))
      return false;

    for (uint32_t i = 0u; i < samplerHeapCount; i++) {
      DxvkShaderBinding binding = { };

      if (!read(stream, offset, binding))
        return false;

      layout.addSamplerHeap(binding);
    }

    return true;
  }


  bool DxvkShaderCache::readShaderXfbInfo(util::File& stream, size_t& offset, dxbc_spv::ir::IoXfbInfo& xfb) {
    return readString(stream, offset, xfb.semanticName)
        && read(stream, offset, xfb.semanticIndex)
        && read(stream, offset, xfb.componentMask)
        && read(stream, offset, xfb.stream)
        && read(stream, offset, xfb.buffer)
        && read(stream, offset, xfb.offset)
        && read(stream, offset, xfb.stride);
 }


  bool DxvkShaderCache::readShaderLutKey(util::File& stream, size_t& offset, LutKey& key) {
    bool status = readString(stream, offset, key.name)
               && read(stream, offset, key.createInfo.options)
               && read(stream, offset, key.createInfo.flatShadingInputs)
               && read(stream, offset, key.createInfo.rasterizedStream);

    uint32_t xfbCount = 0u;
    status = status && read(stream, offset, xfbCount);

    key.createInfo.xfbEntries.resize(xfbCount);

    for (uint32_t i = 0u; i < xfbCount; i++)
      status = status && readShaderXfbInfo(stream, offset, key.createInfo.xfbEntries[i]);

    return status;
  }


  bool DxvkShaderCache::readShaderLutEntry(LutKey& key, LutEntry& entry, size_t& offset) {
    return readShaderLutKey(m_lutFile, offset, key) && read(m_lutFile, offset, entry);
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


  bool DxvkShaderCache::writeShaderLayout(util::File& stream, const DxvkPipelineLayoutBuilder& layout) {
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


  bool DxvkShaderCache::writeShaderIo(util::File& stream, const DxvkShaderIo& io) {
    bool status = write(stream, uint8_t(io.getVarCount()));

    for (uint32_t i = 0u; i < io.getVarCount(); i++) {
      const auto& var = io.getVar(i);

      status = status && write(stream, var.builtIn)
                      && write(stream, var.location)
                      && write(stream, var.componentIndex)
                      && write(stream, var.componentCount)
                      && write(stream, var.isPatchConstant)
                      && write(stream, var.semanticIndex)
                      && writeString(stream, var.semanticName);
    }

    return status;
  }


  bool DxvkShaderCache::writeShaderMetadata(util::File& stream, const DxvkShaderMetadata& metadata) {
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


  std::optional<DxvkShaderCache::LutEntry> DxvkShaderCache::writeShaderBinary(util::File& stream, DxvkIrShader& shader) {
    auto [data, size] = shader.getSerializedIr();

    LutEntry entry = { };
    entry.offset = stream.size();
    entry.binarySize = size;

    if (!writeBytes(stream, data, size)
     || !writeShaderMetadata(stream, shader.getShaderMetadata())
     || !writeShaderLayout(stream, shader.getLayout()))
      return std::nullopt;


    entry.metadataSize = uint32_t(uint64_t(stream.size()) - (entry.offset + entry.binarySize));
    entry.checksum = bit::fnv1a_hash(data, size);
    return std::make_optional(entry);
  }


  bool DxvkShaderCache::writeHeader(util::File& stream, const LutHeader& header) {
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


  Rc<DxvkShaderCache> DxvkShaderCache::getInstance() {
    std::lock_guard lock(s_instance.mutex);

    if (!s_instance.instance)
      s_instance.instance = new DxvkShaderCache();

    return s_instance.instance;
  }


  void DxvkShaderCache::freeInstance() {
    std::lock_guard lock(s_instance.mutex);

    // The ref count can only be incremented from 0 to 1 inside a locked
    // context, so this check is safe. Don't destroy the object if another
    // thread has essentially revived it.
    if (m_useCount.load(std::memory_order_relaxed) || s_instance.instance != this) {
      if (s_instance.instance == this)
        s_instance.instance = nullptr;

      delete this;
    }
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
