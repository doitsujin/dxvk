#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_parser.h>

#include "d3d11_device.h"
#include "d3d11_shader.h"

namespace dxvk {

  class D3D11ShaderConverter : public DxvkIrShaderConverter {

  public:

    D3D11ShaderConverter(
      const DxvkShaderHash&         ShaderKey,
      const DxvkIrShaderCreateInfo& ModuleInfo,
      const void*                   pShaderBytecode,
            size_t                  BytecodeLength,
            bool                    LowerIcb)
    : m_key(ShaderKey), m_info(ModuleInfo), m_lowerIcb(LowerIcb) {
      m_dxbc.resize(BytecodeLength);
      std::memcpy(m_dxbc.data(), pShaderBytecode, BytecodeLength);
    }

    ~D3D11ShaderConverter() { }

    void convertShader(
            dxbc_spv::ir::Builder&    builder) {
      auto debugName = m_key.toString();

      dxbc_spv::dxbc::Converter::Options options = { };
      options.name = debugName.c_str();
      options.includeDebugNames = true;
      options.boundCheckShaderIo = true;
      options.lowerIcb = m_lowerIcb;
      options.icbRegisterSpace = 0u;
      options.icbRegisterIndex = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
      options.maxTessFactor = float(m_info.options.maxTessFactor);

      dxbc_spv::dxbc::Container container(m_dxbc.data(), m_dxbc.size());

      dxbc_spv::dxbc::ShaderInfo shaderInfo =
        dxbc_spv::dxbc::Parser(container.getCodeChunk()).getShaderInfo();

      dxbc_spv::dxbc::Converter converter(std::move(container), options);

      // Determine whether to create a regular shader or a pass-through GS
      auto dstIsGs = m_key.stage() == VK_SHADER_STAGE_GEOMETRY_BIT;
      auto srcIsGs = shaderInfo.getType() == dxbc_spv::dxbc::ShaderType::eGeometry;

      if (dstIsGs && !srcIsGs) {
        if (!converter.createPassthroughGs(builder))
          throw DxvkError(str::format("Failed to create pass-through geometry shader: ", m_key.toString()));
      } else {
        if (!converter.convertShader(builder))
          throw DxvkError(str::format("Failed to convert shader: ", m_key.toString()));
      }
    }

    uint32_t determineResourceIndex(
            dxbc_spv::ir::ShaderStage stage,
            dxbc_spv::ir::ScalarType  type,
            uint32_t                  regSpace,
            uint32_t                  regIndex) const {
      switch (type) {
        case dxbc_spv::ir::ScalarType::eSampler:
          return D3D11ShaderResourceMapping::computeSamplerBinding(stage, regIndex);
        case dxbc_spv::ir::ScalarType::eCbv:
          return D3D11ShaderResourceMapping::computeCbvBinding(stage, regIndex);
        case dxbc_spv::ir::ScalarType::eSrv:
          return D3D11ShaderResourceMapping::computeSrvBinding(stage, regIndex);
        case dxbc_spv::ir::ScalarType::eUav:
          return D3D11ShaderResourceMapping::computeUavBinding(stage, regIndex);
        case dxbc_spv::ir::ScalarType::eUavCounter:
          return D3D11ShaderResourceMapping::computeUavCounterBinding(stage, regIndex);
        default:
          return -1u;
      }
    }

    void dumpSource(const std::string& path) const {
      std::ofstream file(str::topath(str::format(path, "/", m_key.toString(), ".dxbc").c_str()).c_str(), std::ios_base::trunc | std::ios_base::binary);
      file.write(reinterpret_cast<const char*>(m_dxbc.data()), m_dxbc.size());
    }

    std::string getDebugName() const {
      return m_key.toString();
    }

  private:

    std::vector<uint8_t> m_dxbc;

    DxvkShaderHash          m_key;
    DxvkIrShaderCreateInfo  m_info;

    bool                    m_lowerIcb = false;

  };



  
  D3D11CommonShader:: D3D11CommonShader() { }
  D3D11CommonShader::~D3D11CommonShader() { }
  
  
  D3D11CommonShader::D3D11CommonShader(
          D3D11Device*            pDevice,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
    const D3D11ShaderIcbInfo&     Icb,
    const DxbcBindingMask&        BindingMask)
  : m_bindings(BindingMask) {
    Logger::debug(str::format("Compiling shader ", ShaderKey.toString()));
    
    if (pDevice->GetOptions()->useDxbcSpirv)
      CreateIrShader(pDevice, ShaderKey, ModuleInfo, pShaderBytecode, BytecodeLength, Icb);
    else
      CreateLegacyShader(pDevice, ShaderKey, ModuleInfo, pShaderBytecode, BytecodeLength);

    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }


  void D3D11CommonShader::CreateIrShader(
          D3D11Device*            pDevice,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
    const D3D11ShaderIcbInfo&     Icb) {
    constexpr size_t MaxEmbeddedIcbSize = 64u;

    // Create icb if lowering is required
    size_t icbSizeInBytes = Icb.size * sizeof(*Icb.data);

    if (ModuleInfo.options.flags.test(DxvkShaderCompileFlag::LowerConstantArrays) && icbSizeInBytes > MaxEmbeddedIcbSize) {
      DxvkBufferCreateInfo info = { };
      info.size   = align(icbSizeInBytes, 256u);
      info.usage  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                  | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                  | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      info.stages = util::pipelineStages(ShaderKey.stage());
      info.access = VK_ACCESS_UNIFORM_READ_BIT
                  | VK_ACCESS_TRANSFER_READ_BIT
                  | VK_ACCESS_TRANSFER_WRITE_BIT;
      info.debugName = "Icb";

      m_buffer = pDevice->GetDXVKDevice()->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      pDevice->InitShaderIcb(this, icbSizeInBytes, Icb.data);
    }

    // Create actual shader converter
    m_shader = new DxvkIrShader(ModuleInfo,
      new D3D11ShaderConverter(ShaderKey, ModuleInfo, pShaderBytecode, BytecodeLength, bool(m_buffer)));
  }


  void D3D11CommonShader::CreateLegacyShader(
          D3D11Device*            pDevice,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength) {
    // If requested by the user, dump both the raw DXBC
    // shader and the compiled SPIR-V module to a file.
    const auto& dumpPath = DxvkShader::getShaderDumpPath();

    if (!dumpPath.empty()) {
      std::ofstream file(str::topath(str::format(dumpPath, "/", ShaderKey.toString(), ".dxbc").c_str()).c_str(), std::ios_base::binary | std::ios_base::trunc);
      file.write(reinterpret_cast<const char*>(pShaderBytecode), BytecodeLength);
    }

    DxbcReader reader(
      reinterpret_cast<const char*>(pShaderBytecode),
      BytecodeLength);

    // Compute legacy SHA-1 hash to pass as shader name
    Sha1Hash sha1Hash = Sha1Hash::compute(
      pShaderBytecode, BytecodeLength);

    DxvkShaderKey legacyKey(ShaderKey.stage(), sha1Hash);

    // Error out if the shader is invalid
    DxbcModule module(reader);
    auto programInfo = module.programInfo();

    if (!programInfo)
      throw DxvkError("Invalid shader binary.");

    DxbcXfbInfo xfb = { };
    xfb.rasterizedStream = ModuleInfo.rasterizedStream;
    xfb.entryCount = ModuleInfo.xfbEntries.size();

    for (size_t i = 0u; i < ModuleInfo.xfbEntries.size(); i++) {
      const auto& src = ModuleInfo.xfbEntries[i];

      auto& dst = xfb.entries[i];
      dst.semanticName = src.semanticName.c_str();
      dst.semanticIndex = src.semanticIndex;
      dst.componentIndex = bit::tzcnt(uint32_t(src.componentMask));
      dst.componentCount = bit::popcnt(uint32_t(src.componentMask));
      dst.streamId = src.stream;
      dst.bufferId = src.buffer;
      dst.offset = src.offset;

      xfb.strides[src.buffer] = src.stride;
    }

    DxbcTessInfo tessInfo = { };
    tessInfo.maxTessFactor = float(ModuleInfo.options.maxTessFactor);

    DxbcModuleInfo legacyInfo = { };
    legacyInfo.options.supportsTypedUavLoadR32 = !ModuleInfo.options.flags.test(DxvkShaderCompileFlag::TypedR32LoadRequiresFormat);
    legacyInfo.options.supportsRawAccessChains = ModuleInfo.options.spirv.test(DxvkShaderSpirvFlag::SupportsNvRawAccessChains);
    legacyInfo.options.rawAccessChainBug = legacyInfo.options.supportsRawAccessChains;
    legacyInfo.options.invariantPosition = true;
    legacyInfo.options.forceVolatileTgsmAccess = ModuleInfo.options.flags.test(DxvkShaderCompileFlag::InsertSharedMemoryBarriers);
    legacyInfo.options.forceComputeUavBarriers = ModuleInfo.options.flags.test(DxvkShaderCompileFlag::InsertResourceBarriers);
    legacyInfo.options.disableMsaa = ModuleInfo.options.flags.test(DxvkShaderCompileFlag::DisableMsaa);
    legacyInfo.options.forceSampleRateShading = ModuleInfo.options.flags.test(DxvkShaderCompileFlag::EnableSampleRateShading);
    legacyInfo.options.needsPointSizeExport = ModuleInfo.options.spirv.test(DxvkShaderSpirvFlag::ExportPointSize);
    legacyInfo.options.sincosEmulation = ModuleInfo.options.flags.test(DxvkShaderCompileFlag::LowerSinCos);
    legacyInfo.options.supports16BitPushData = ModuleInfo.options.flags.test(DxvkShaderCompileFlag::Supports16BitPushData);
    legacyInfo.options.minSsboAlignment = ModuleInfo.options.minStorageBufferAlignment;

    if (ModuleInfo.options.spirv.test(DxvkShaderSpirvFlag::SupportsDenormFlush32) &&
        ModuleInfo.options.spirv.test(DxvkShaderSpirvFlag::SupportsRte32))
      legacyInfo.options.floatControl.set(DxbcFloatControlFlag::DenormFlushToZero32);

    if (ModuleInfo.options.spirv.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve32))
      legacyInfo.options.floatControl.set(DxbcFloatControlFlag::PreserveNan32);

    if (ModuleInfo.options.spirv.test(DxvkShaderSpirvFlag::IndependentDenormMode)) {
      if (ModuleInfo.options.spirv.test(DxvkShaderSpirvFlag::SupportsDenormPreserve64) &&
          ModuleInfo.options.spirv.test(DxvkShaderSpirvFlag::SupportsRte64))
        legacyInfo.options.floatControl.set(DxbcFloatControlFlag::DenormPreserve64);

      if (ModuleInfo.options.spirv.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve64))
        legacyInfo.options.floatControl.set(DxbcFloatControlFlag::PreserveNan64);
    }

    if (tessInfo.maxTessFactor >= 8.0f)
      legacyInfo.tess = &tessInfo;

    if (xfb.entryCount)
      legacyInfo.xfb = &xfb;

    // Decide whether we need to create a pass-through
    // geometry shader for vertex shader stream output
    bool isPassthroughShader = (ShaderKey.stage() == VK_SHADER_STAGE_GEOMETRY_BIT)
      && (programInfo->type() == DxbcProgramType::VertexShader
       || programInfo->type() == DxbcProgramType::DomainShader);

    if (programInfo->shaderStage() != ShaderKey.stage() && !isPassthroughShader)
      throw DxvkError("Mismatching shader type.");

    m_shader = isPassthroughShader
      ? module.compilePassthroughShader(legacyInfo, legacyKey.toString())
      : module.compile                 (legacyInfo, legacyKey.toString());

    // Create shader constant buffer if necessary
    auto icb = module.icbInfo();

    if (icb.size) {
      DxvkBufferCreateInfo info = { };
      info.size   = align(icb.size, 256u);
      info.usage  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                  | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                  | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      info.stages = util::pipelineStages(m_shader->metadata().stage);
      info.access = VK_ACCESS_UNIFORM_READ_BIT
                  | VK_ACCESS_TRANSFER_READ_BIT
                  | VK_ACCESS_TRANSFER_WRITE_BIT;
      info.debugName = "Icb";

      m_buffer = pDevice->GetDXVKDevice()->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      // Upload immediate constant buffer to VRAM
      pDevice->InitShaderIcb(this, icb.size, icb.data);
    }

    if (!dumpPath.empty()) {
      std::ofstream dumpStream(
        str::topath(str::format(dumpPath, "/", ShaderKey.toString(), ".spv").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc);
      m_shader->dump(dumpStream);
    }
  }

  
  D3D11ShaderModuleSet:: D3D11ShaderModuleSet() { }
  D3D11ShaderModuleSet::~D3D11ShaderModuleSet() { }
  
  
  HRESULT D3D11ShaderModuleSet::GetShaderModule(
          D3D11Device*            pDevice,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
    const D3D11ShaderIcbInfo&     Icb,
    const DxbcBindingMask&        BindingMask,
          D3D11CommonShader*      pShader) {
    // Use the shader's unique key for the lookup
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto entry = m_modules.find(ShaderKey);
      if (entry != m_modules.end()) {
        *pShader = entry->second;
        return S_OK;
      }
    }
    
    // This shader has not been compiled yet, so we have to create a
    // new module. This takes a while, so we won't lock the structure.
    D3D11CommonShader module;
    
    try {
      module = D3D11CommonShader(pDevice, ShaderKey,
        ModuleInfo, pShaderBytecode, BytecodeLength, Icb, BindingMask);
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }
    
    // Insert the new module into the lookup table. If another thread
    // has compiled the same shader in the meantime, we should return
    // that object instead and discard the newly created module.
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto status = m_modules.insert({ ShaderKey, module });

      if (!status.second) {
        *pShader = status.first->second;
        return S_OK;
      }
    }
    
    *pShader = std::move(module);
    return S_OK;
  }

}
