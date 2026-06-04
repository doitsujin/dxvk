#include <sm3/sm3_io_map.h>
#include <sm3/sm3_types.h>

#include "d3d9_swvp_emu.h"

#include "d3d9_device.h"
#include "d3d9_vertex_declaration.h"

#include "../dxvk/dxvk_shader_spirv.h"

namespace dxvk {

  // Doesn't compare everything, only what we use in SWVP.

  size_t D3D9VertexDeclHash::operator () (const D3D9CompactVertexElements& key) const {
    DxvkHashState hash;

    std::hash<BYTE> bytehash;
    std::hash<WORD> wordhash;

    for (uint32_t i = 0; i < key.size(); i++) {
      const auto& element = key[i];
      hash.add(wordhash(element.Stream));
      hash.add(wordhash(element.Offset));
      hash.add(bytehash(element.Type));
      hash.add(bytehash(element.Method));
      hash.add(bytehash(element.Usage));
      hash.add(bytehash(element.UsageIndex));
    }

    return hash;
  }

  bool D3D9VertexDeclEq::operator () (const D3D9CompactVertexElements& a, const D3D9CompactVertexElements& b) const {
    if (a.size() != b.size())
      return false;

    bool equal = true;

    for (uint32_t i = 0; i < a.size(); i++)
      equal &= std::memcmp(&a[i], &b[i], sizeof(a[0])) == 0;

    return equal;
  }


  class D3D9SWVPShaderGenerator : public DxvkIrShaderConverter {

  public:

    D3D9SWVPShaderGenerator(
      const DxvkShaderHash&             ShaderKey,
      const D3D9CompactVertexElements&  Elements)
    : m_key(ShaderKey), m_elements(Elements) { }

    void convertShader(
            dxbc_spv::ir::Builder&    builder) {
      using namespace dxbc_spv;

      // Set up entry point as a geometry shader that operates on points
      auto function = builder.add(ir::Op::Function(ir::Type()));
      builder.add(ir::Op::DebugName(function, "main"));

      auto entryPoint = builder.add(ir::Op::EntryPoint(function, ir::ShaderStage::eGeometry));
      builder.add(ir::Op::DebugName(entryPoint, m_key.toString().c_str()));

      // Need to declare one output vertex even though we never emit anything
      builder.add(ir::Op::SetGsOutputVertices(entryPoint, 1u));
      builder.add(ir::Op::SetGsOutputPrimitive(entryPoint, ir::PrimitiveType::ePoints, 0x1u));
      builder.add(ir::Op::SetGsInputPrimitive(entryPoint, ir::PrimitiveType::ePoints));
      builder.add(ir::Op::SetGsInstances(entryPoint, 1u));
      builder.add(ir::Op::SetFpMode(entryPoint, ir::ScalarType::eF32,
        ir::OpFlags(), ir::RoundMode::eZero, ir::DenormMode::eFlush));

      // Declare and load primitive ID as the output index
      auto primId = builder.add(ir::Op::DclInputBuiltIn(ir::ScalarType::eU32, entryPoint, ir::BuiltIn::ePrimitiveId));
      builder.add(ir::Op::DebugName(primId, "prim_id"));

      auto outputIndex = builder.add(ir::Op::InputLoad(ir::ScalarType::eU32, primId, ir::SsaDef()));

      // Declare push data for position_t
      auto pushDataType = ir::Type()
        .addStructMember(ir::ScalarType::eF32, 2u)
        .addStructMember(ir::ScalarType::eF32, 2u);

      auto pushData = builder.add(ir::Op::DclPushData(pushDataType, entryPoint, 0u, ir::ShaderStage::eGeometry));
      builder.add(ir::Op::DebugMemberName(pushData, 0u, "vp_offset"));
      builder.add(ir::Op::DebugMemberName(pushData, 1u, "vp_size"));

      // Define UAV with dummy type for now, and build the actual data type as we go.
      auto uav = builder.add(ir::Op::DclUav(ir::Type(), entryPoint,
        0u, 0u, 1u, ir::ResourceKind::eBufferStructured, ir::UavFlag::eWriteOnly));
      builder.add(ir::Op::DebugName(uav, "dst"));

      auto descriptor = builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eUav, uav, builder.makeConstant(0u)));

      // Work out the highest byte offset of any valid element
      ir::Type uavType;

      uint32_t maxOffset = 0u;
      uint32_t unusedCount = 0u;

      for (const auto& e : m_elements)
        maxOffset = std::max(maxOffset, uint32_t(e.Offset));

      uint32_t memberOffset = 0u;
      uint32_t nextLocation = sm3::IoMap::getFixedFunctionLocationCount();;

      while ((memberOffset = uavType.byteSize()) <= maxOffset) {
        uint32_t memberIndex = uavType.getStructMemberCount();

        // Find element with matching offset
        const D3D9CompactVertexElement* e = nullptr;

        for (const auto& element : m_elements) {
          if (uint32_t(element.Offset) == memberOffset) {
            e = &element;
            break;
          }
        }

        if (e) {
          // Found a valid element at the current offset
          uavType.addStructMember(GetTypeForDecltype(D3DDECLTYPE(e->Type)));

          // Map to location based on semantic
          sm3::Semantic semantic = {};
          semantic.usage = sm3::SemanticUsage(e->Usage);
          semantic.index = uint32_t(e->UsageIndex);

          auto location = sm3::IoMap::findFixedFunctionLocation(semantic).value_or(nextLocation);

          if (location == nextLocation)
            nextLocation += 1u;

          // If the UAV type is a struct, the address must be a vector
          auto outputAddress = outputIndex;

          if (maxOffset) {
            builder.add(ir::Op::DebugMemberName(uav, memberIndex, GetElementName(
              D3DDECLUSAGE(e->Usage), e->UsageIndex).c_str()));

            outputAddress = builder.add(ir::Op::CompositeConstruct(
              ir::BasicType(ir::ScalarType::eU32, 2u), outputAddress,
              builder.makeConstant(uint32_t(memberIndex))));
          }

          auto outputValue = ProcessElement(builder, entryPoint, pushData, *e, location);
          builder.add(ir::Op::BufferStore(descriptor, outputAddress, outputValue, 4u));
        } else {
          // No element found, pad with empty u32.
          uavType.addStructMember(ir::ScalarType::eU32);

          builder.add(ir::Op::DebugMemberName(uav, memberIndex,
            str::format("unused" + (unusedCount++)).c_str()));
        }
      }

      // Rewrite UAV op with proper type
      builder.rewriteOp(uav, ir::Op(builder.getOp(uav)).setType(uavType.addArrayDimension(0u)));
      builder.add(ir::Op::FunctionEnd());
    }

    uint32_t determineResourceIndex(
            dxbc_spv::ir::ShaderStage stage,
            dxbc_spv::ir::ScalarType  type,
            uint32_t                  regSpace,
            uint32_t                  regIndex) const {
      // We only have one single resource in this shader
      dxbc_spv_assert(stage == dxbc_spv::ir::ShaderStage::eGeometry);
      dxbc_spv_assert(type == dxbc_spv::ir::ScalarType::eUav && !regSpace && !regIndex);

      return D3D9ShaderResourceMapping::getSwvpBufferIndex();
    }

    void dumpSource(const std::string& path) const {
      // No actual source to dump here, but we can stringify the vertex elements
      std::ofstream file(str::topath(str::format(path, "/", m_key.toString(), ".decl").c_str()).c_str(), std::ios_base::trunc);

      uint32_t index = 0u;

      for (const auto& e : m_elements) {
        file << "[" << (index++) << "] " << GetElementName(D3DDECLUSAGE(e.Usage), e.UsageIndex) << ": ";
        file << "Offset = " << e.Offset << ", Type = " << e.Type << std::endl;
      }
    }

    std::string getDebugName() const {
      return m_key.toString();
    }

  private:

    DxvkShaderHash            m_key;
    D3D9CompactVertexElements m_elements;

    dxbc_spv::ir::BasicType GetTypeForDecltype(D3DDECLTYPE ty) {
      using namespace dxbc_spv;

      switch (ty) {
        case D3DDECLTYPE_FLOAT1:
          return ir::BasicType(ir::ScalarType::eF32, 1u);

        case D3DDECLTYPE_FLOAT2:
          return ir::BasicType(ir::ScalarType::eF32, 2u);

        case D3DDECLTYPE_FLOAT3:
          return ir::BasicType(ir::ScalarType::eF32, 3u);

        case D3DDECLTYPE_FLOAT4:
          return ir::BasicType(ir::ScalarType::eF32, 4u);

        case D3DDECLTYPE_D3DCOLOR:
        case D3DDECLTYPE_UBYTE4:
        case D3DDECLTYPE_UBYTE4N:
          return ir::BasicType(ir::ScalarType::eU8, 4u);

        case D3DDECLTYPE_SHORT2:
        case D3DDECLTYPE_SHORT2N:
          return ir::BasicType(ir::ScalarType::eI16, 2u);

        case D3DDECLTYPE_SHORT4:
        case D3DDECLTYPE_SHORT4N:
          return ir::BasicType(ir::ScalarType::eI16, 4u);

        case D3DDECLTYPE_USHORT2N:
          return ir::BasicType(ir::ScalarType::eU16, 2u);

        case D3DDECLTYPE_USHORT4N:
          return ir::BasicType(ir::ScalarType::eU16, 4u);

        // Essentially RGB10 encoding
        case D3DDECLTYPE_UDEC3:
        case D3DDECLTYPE_DEC3N:
          return ir::BasicType(ir::ScalarType::eU32, 1u);

        case D3DDECLTYPE_FLOAT16_2:
          return ir::BasicType(ir::ScalarType::eF16, 2u);

        case D3DDECLTYPE_FLOAT16_4:
          return ir::BasicType(ir::ScalarType::eF16, 4u);

        default:
          Logger::err(str::format("SWVP: Unsupported decltype ", ty));
          return ir::ScalarType::eU32;
      }
    }

    dxbc_spv::ir::SsaDef ProcessElement(
            dxbc_spv::ir::Builder&      builder,
            dxbc_spv::ir::SsaDef        entryPoint,
            dxbc_spv::ir::SsaDef        pushData,
      const D3D9CompactVertexElement&   element,
            uint32_t                    location) {
      using namespace dxbc_spv;

      auto semantic = sm3::SemanticUsage(D3DDECLUSAGE(element.Usage));
      auto semanticIndex = uint32_t(element.UsageIndex);

      // Location must match fixed-function locations, we cannot rely on semantic I/O.
      auto inputSize = semantic == sm3::SemanticUsage::eFog ? 1u : 4u;
      auto inputType = ir::Type(ir::ScalarType::eF32, inputSize).addArrayDimension(1u);

      bool isPosition = !semanticIndex &&
        (semantic == sm3::SemanticUsage::ePosition || semantic == sm3::SemanticUsage::ePositionT);

      auto inputVar = builder.add(isPosition
        ? ir::Op::DclInputBuiltIn(inputType, entryPoint, ir::BuiltIn::ePosition)
        : ir::Op::DclInput(inputType, entryPoint, location, 0u));

      builder.add(ir::Op::Semantic(inputVar, semanticIndex, str::format(semantic).c_str()));

      std::array<ir::SsaDef, 4u> components = { };

      for (uint32_t i = 0u; i < 4u; i++) {
        auto index = inputSize > 1u
          ? builder.makeConstant(0u, i)
          : builder.makeConstant(0u);

        components[i] = i < inputSize
          ? builder.add(ir::Op::InputLoad(ir::ScalarType::eF32, inputVar, index))
          : builder.makeConstant(0.0f);
      }

      // Compute transformed screen-space coordinate for POSITIONT:
      // - Reciprocate W and multiply x, y and z with the result
      // - Map x and y into the viewport
      if (D3DDECLUSAGE(element.Usage) == D3DDECLUSAGE_POSITIONT && !element.UsageIndex) {
        components[3u] = builder.add(ir::Op::FRcp(ir::ScalarType::eF32, components[3u]));

        for (uint32_t i = 0u; i < 3u; i++)
          components[i] = builder.add(ir::Op::FMul(ir::ScalarType::eF32, components[i], components[3u]));

        for (uint32_t i = 0u; i < 2u; i++) {
          components[i] = builder.add(ir::Op::FMad(ir::ScalarType::eF32, components[i],
            builder.makeConstant(0.5f), builder.makeConstant(0.5f)));
          components[i] = builder.add(ir::Op::FMad(ir::ScalarType::eF32, components[i],
            builder.add(ir::Op::PushDataLoad(ir::ScalarType::eF32, pushData, builder.makeConstant(1u, i))),
            builder.add(ir::Op::PushDataLoad(ir::ScalarType::eF32, pushData, builder.makeConstant(0u, i)))));
        }
      }

      for (uint32_t i = 0u; i < 4u; i++)
        components[i] = convertElement(builder, element, components[i], i);

      // Build and return result vector
      auto type = GetTypeForDecltype(D3DDECLTYPE(element.Type));
      return packElement(builder, element, type, components.data());
    }

    dxbc_spv::ir::SsaDef convertElement(
            dxbc_spv::ir::Builder&      builder,
      const D3D9CompactVertexElement&   element,
            dxbc_spv::ir::SsaDef        value,
            uint32_t                    index) {
      using namespace dxbc_spv;

      switch (D3DDECLTYPE(element.Type)) {
        case D3DDECLTYPE_FLOAT1:
        case D3DDECLTYPE_FLOAT2:
        case D3DDECLTYPE_FLOAT3:
        case D3DDECLTYPE_FLOAT4:
          return value;

        case D3DDECLTYPE_D3DCOLOR:
        case D3DDECLTYPE_UBYTE4N:
          value = builder.add(ir::Op::FClamp(ir::ScalarType::eF32, value,
            builder.makeConstant(0.0f), builder.makeConstant(1.0f)));
          value = builder.add(ir::Op::FMul(ir::ScalarType::eF32, value,
            builder.makeConstant(255.0f)));
          return builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eU8, value));

        case D3DDECLTYPE_UBYTE4:
          value = builder.add(ir::Op::FClamp(ir::ScalarType::eF32, value,
            builder.makeConstant(0.0f), builder.makeConstant(255.0f)));
          return builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eU8, value));

        case D3DDECLTYPE_SHORT2N:
        case D3DDECLTYPE_SHORT4N:
          value = builder.add(ir::Op::FClamp(ir::ScalarType::eF32, value,
            builder.makeConstant(-1.0f), builder.makeConstant(1.0f)));
          value = builder.add(ir::Op::FMul(ir::ScalarType::eF32, value,
            builder.makeConstant(32767.0f)));
          return builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eI16, value));

        case D3DDECLTYPE_SHORT2:
        case D3DDECLTYPE_SHORT4:
          value = builder.add(ir::Op::FClamp(ir::ScalarType::eF32, value,
            builder.makeConstant(-32768.0f), builder.makeConstant(32767.0f)));
          return builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eI16, value));

        case D3DDECLTYPE_USHORT2N:
        case D3DDECLTYPE_USHORT4N:
          value = builder.add(ir::Op::FClamp(ir::ScalarType::eF32, value,
            builder.makeConstant(0.0f), builder.makeConstant(1.0f)));
          value = builder.add(ir::Op::FMul(ir::ScalarType::eF32, value,
            builder.makeConstant(65535.0f)));
          return builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eU16, value));

        case D3DDECLTYPE_DEC3N:
          value = builder.add(ir::Op::FClamp(ir::ScalarType::eF32, value,
            builder.makeConstant(-1.0f), builder.makeConstant(1.0f)));
          value = builder.add(ir::Op::FMul(ir::ScalarType::eF32, value,
            builder.makeConstant(511.0f)));
          value = builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eI32, value));
          return builder.add(ir::Op::Cast(ir::ScalarType::eU32, value));

        case D3DDECLTYPE_UDEC3:
          value = builder.add(ir::Op::FClamp(ir::ScalarType::eF32, value,
            builder.makeConstant(0.0f), builder.makeConstant(1023.0f)));
          return builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eU32, value));

        case D3DDECLTYPE_FLOAT16_2:
        case D3DDECLTYPE_FLOAT16_4:
          return builder.add(ir::Op::ConvertFtoF(ir::ScalarType::eF16, value));

        default:
          // We already log an error
          return builder.makeUndef(ir::ScalarType::eU32);
      }
    }


    dxbc_spv::ir::SsaDef packElement(
            dxbc_spv::ir::Builder&      builder,
      const D3D9CompactVertexElement&   element,
            dxbc_spv::ir::BasicType     type,
      const dxbc_spv::ir::SsaDef*       values) {
      using namespace dxbc_spv;

      switch (D3DDECLTYPE(element.Type)) {
        // Build vec4, but flip color components
        case D3DDECLTYPE_D3DCOLOR: {
          return builder.add(ir::Op::CompositeConstruct(type,
            values[2u], values[1u], values[0u], values[3u]));
        }

        // Pack stuff into single uint32
        case D3DDECLTYPE_DEC3N:
        case D3DDECLTYPE_UDEC3: {
          auto result = values[0u];
          result = builder.add(ir::Op::IBitInsert(ir::ScalarType::eU32, result,
            values[1u], builder.makeConstant(10u), builder.makeConstant(10u)));
          result = builder.add(ir::Op::IBitInsert(ir::ScalarType::eU32, result,
            values[2u], builder.makeConstant(20u), builder.makeConstant(10u)));
          return result;
        }

        // For everything else, just build a regular old vector
        default: {
          if (type.isScalar())
            return values[0u];

          auto op = ir::Op(ir::OpCode::eCompositeConstruct, type);

          for (uint32_t i = 0u; i < type.getVectorSize(); i++)
            op.addOperand(values[i]);

          return builder.add(std::move(op));
        }
      }
    }

    static std::string GetSemanticName(D3DDECLUSAGE semantic) {
      return str::format(dxbc_spv::sm3::SemanticUsage(semantic));
    }

    static std::string GetElementName(D3DDECLUSAGE semantic, UINT index) {
      return str::format(dxbc_spv::sm3::SemanticUsage(semantic), index);
    }

  };


  Rc<DxvkShader> D3D9SWVPEmulator::GetShaderModule(D3D9DeviceEx* pDevice, D3D9CompactVertexElements&& elements) {
    std::unique_lock<dxvk::mutex> lock(m_mutex);
    auto entry = m_modules.find(elements);

    if (entry != m_modules.end())
      return entry->second;

    Sha1Hash hash = Sha1Hash::compute(elements.data(), elements.size() * sizeof(elements[0]));
    DxvkShaderHash key(VK_SHADER_STAGE_GEOMETRY_BIT, 0u, hash.digest(), hash.digestLength());

    DxvkIrShaderCreateInfo createInfo = { };
    createInfo.options = pDevice->GetShaderOptions();

    Rc<DxvkShader> shader = pDevice->GetDXVKDevice()->createCachedShader(key.toString(), createInfo, nullptr);

    if (!shader) {
      shader = pDevice->GetDXVKDevice()->createCachedShader(key.toString(), createInfo,
        new D3D9SWVPShaderGenerator(key, elements));
    }

    pDevice->GetDXVKDevice()->registerShader(shader);

    m_modules.insert({ std::move(elements), shader });
    return shader;
  }

}
