#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_parser.h>

#include "d3d11_device.h"
#include "d3d11_shader.h"

namespace dxvk {

  static D3D11ShaderResourceMapping g_d3d11ShaderMapping;


  D3D11ShaderResourceMapping::~D3D11ShaderResourceMapping() {

  }


  uint32_t D3D11ShaderResourceMapping::determineResourceIndex(
          dxbc_spv::ir::ShaderStage stage,
          dxbc_spv::ir::ScalarType  type,
          uint32_t                  regSpace,
          uint32_t                  regIndex) const {
    switch (type) {
      case dxbc_spv::ir::ScalarType::eSampler:
        return computeSamplerBinding(stage, regIndex);
      case dxbc_spv::ir::ScalarType::eCbv:
        return computeCbvBinding(stage, regIndex);
      case dxbc_spv::ir::ScalarType::eSrv:
        return computeSrvBinding(stage, regIndex);
      case dxbc_spv::ir::ScalarType::eUav:
        return computeUavBinding(stage, regIndex);
      case dxbc_spv::ir::ScalarType::eUavCounter:
        return computeUavCounterBinding(stage, regIndex);
      default:
        return -1u;
    }
  }

  
  D3D11CommonShader:: D3D11CommonShader() { }
  D3D11CommonShader::~D3D11CommonShader() { }
  
  
  D3D11CommonShader::D3D11CommonShader(
          D3D11Device*            pDevice,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
    const DxbcBindingMask&        BindingMask)
  : m_bindings(BindingMask) {
    const std::string name = ShaderKey.toString();
    Logger::debug(str::format("Compiling shader ", name));
    
    // If requested by the user, dump both the raw DXBC
    // shader and the compiled SPIR-V module to a file.
    const std::string& dumpPath = pDevice->GetOptions()->shaderDumpPath;
    
    if (!dumpPath.empty()) {
      std::ofstream file(str::topath(str::format(dumpPath, "/", name, ".dxbc").c_str()).c_str(), std::ios_base::binary | std::ios_base::trunc);
      file.write(reinterpret_cast<const char*>(pShaderBytecode), BytecodeLength);
    }

    if (pDevice->GetOptions()->useDxbcSpirv)
      CreateIrShader(pDevice, ShaderKey, ModuleInfo, pShaderBytecode, BytecodeLength);
    else
      CreateLegacyShader(pDevice, ShaderKey, ModuleInfo, pShaderBytecode, BytecodeLength);

    if (!dumpPath.empty()) {
      std::ofstream dumpStream(
        str::topath(str::format(dumpPath, "/", name, ".spv").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc);
      m_shader->dump(dumpStream);
    }

    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }


  void D3D11CommonShader::CreateIrShader(
          D3D11Device*            pDevice,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength) {
    auto debugName = ShaderKey.toString();

    dxbc_spv::dxbc::Converter::Options options = { };
    options.name = debugName.c_str();
    options.includeDebugNames = true;
    options.boundCheckScratch = true;
    options.boundCheckShaderIo = true;
    options.boundCheckIcb = true;
    options.maxTessFactor = float(ModuleInfo.options.compileOptions.maxTessFactor);

    dxbc_spv::dxbc::Container container(pShaderBytecode, BytecodeLength);

    dxbc_spv::dxbc::ShaderInfo shaderInfo =
      dxbc_spv::dxbc::Parser(container.getCodeChunk()).getShaderInfo();

    dxbc_spv::dxbc::Converter converter(std::move(container), options);

    dxbc_spv::ir::Builder builder;

    // Determine whether to create a regular shader or a pass-through GS
    auto dstStage = ConvertShaderStage(shaderInfo.getType());
    auto srcStage = ShaderKey.stage();

    if (dstStage == srcStage) {
      if (!converter.convertShader(builder))
        throw DxvkError("Failed to convert shader.");
    } else {
      if (!converter.createPassthroughGs(builder))
        throw DxvkError("Failed to create pass-through GS.");
    }

    // Figure out used resource bindings
    auto [a, b] = builder.getDeclarations();

    for (auto iter = a; iter != b; iter++) {
      switch (iter->getOpCode()) {
        case dxbc_spv::ir::OpCode::eDclCbv: {
          m_bindings.cbvMask |= 1u << uint32_t(iter->getOperand(2u));
        } break;

        case dxbc_spv::ir::OpCode::eDclSrv: {
          uint32_t index = uint32_t(iter->getOperand(2u));
          m_bindings.srvMask.at(index / 64u) |= 1ull << (index % 64u);
        } break;

        case dxbc_spv::ir::OpCode::eDclUav: {
          uint32_t index = uint32_t(iter->getOperand(2u));
          m_bindings.uavMask |= 1ull << uint32_t(index);
        } break;

        case dxbc_spv::ir::OpCode::eDclSampler: {
          m_bindings.samplerMask |= 1u << uint32_t(iter->getOperand(2u));
        } break;

        default:
          break;
      }
    }

    // Create and process actual shader
    m_shader = new DxvkIrShader(ModuleInfo, g_d3d11ShaderMapping, std::move(builder));
  }


  void D3D11CommonShader::CreateLegacyShader(
          D3D11Device*            pDevice,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength) {
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
    tessInfo.maxTessFactor = float(ModuleInfo.options.compileOptions.maxTessFactor);

    DxbcModuleInfo legacyInfo = { };
    legacyInfo.options.supportsTypedUavLoadR32 = !ModuleInfo.options.compileOptions.flags.test(DxvkShaderCompileFlag::TypedR32LoadRequiresFormat);
    legacyInfo.options.supportsRawAccessChains = ModuleInfo.options.spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsNvRawAccessChains);
    legacyInfo.options.rawAccessChainBug = legacyInfo.options.supportsRawAccessChains;
    legacyInfo.options.invariantPosition = true;
    legacyInfo.options.forceVolatileTgsmAccess = ModuleInfo.options.compileOptions.flags.test(DxvkShaderCompileFlag::InsertSharedMemoryBarriers);
    legacyInfo.options.forceComputeUavBarriers = ModuleInfo.options.compileOptions.flags.test(DxvkShaderCompileFlag::InsertResourceBarriers);
    legacyInfo.options.disableMsaa = ModuleInfo.options.compileOptions.flags.test(DxvkShaderCompileFlag::DisableMsaa);
    legacyInfo.options.forceSampleRateShading = ModuleInfo.options.compileOptions.flags.test(DxvkShaderCompileFlag::EnableSampleRateShading);
    legacyInfo.options.needsPointSizeExport = ModuleInfo.options.spirvOptions.flags.test(DxvkShaderSpirvFlag::ExportPointSize);
    legacyInfo.options.sincosEmulation = ModuleInfo.options.compileOptions.flags.test(DxvkShaderCompileFlag::LowerSinCos);
    legacyInfo.options.supports16BitPushData = ModuleInfo.options.compileOptions.flags.test(DxvkShaderCompileFlag::Supports16BitPushData);
    legacyInfo.options.minSsboAlignment = ModuleInfo.options.compileOptions.minStorageBufferAlignment;

    if (ModuleInfo.options.spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsDenormFlush32) &&
        ModuleInfo.options.spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsRte32))
      legacyInfo.options.floatControl.set(DxbcFloatControlFlag::DenormFlushToZero32);

    if (ModuleInfo.options.spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve32))
      legacyInfo.options.floatControl.set(DxbcFloatControlFlag::PreserveNan32);

    if (ModuleInfo.options.spirvOptions.flags.test(DxvkShaderSpirvFlag::IndependentDenormMode)) {
      if (ModuleInfo.options.spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsDenormPreserve64) &&
          ModuleInfo.options.spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsRte64))
        legacyInfo.options.floatControl.set(DxbcFloatControlFlag::DenormPreserve64);

      if (ModuleInfo.options.spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve64))
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
  }


  VkShaderStageFlagBits D3D11CommonShader::ConvertShaderStage(
          dxbc_spv::dxbc::ShaderType Type) {
    switch (Type) {
      case dxbc_spv::dxbc::ShaderType::eVertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
      case dxbc_spv::dxbc::ShaderType::eHull:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
      case dxbc_spv::dxbc::ShaderType::eDomain:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
      case dxbc_spv::dxbc::ShaderType::eGeometry:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
      case dxbc_spv::dxbc::ShaderType::ePixel:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
      case dxbc_spv::dxbc::ShaderType::eCompute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    }

    return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
  }

  
  D3D11ShaderModuleSet:: D3D11ShaderModuleSet() { }
  D3D11ShaderModuleSet::~D3D11ShaderModuleSet() { }
  
  
  HRESULT D3D11ShaderModuleSet::GetShaderModule(
          D3D11Device*            pDevice,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
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
        ModuleInfo, pShaderBytecode, BytecodeLength, BindingMask);
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
