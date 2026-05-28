#include <sm3/sm3_io_map.h>
#include <sm3/sm3_types.h>

#include "d3d9_swvp_emu.h"

#include "d3d9_device.h"
#include "d3d9_vertex_declaration.h"

#include "../dxvk/dxvk_shader_spirv.h"

#include "../spirv/spirv_module.h"

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

  enum class DecltypeClass {
    Float, Byte, Short, Dec, Half
  };

  enum DecltypeFlags {
    Signed     = 1,
    Normalize  = 2,
    ReverseRGB = 4
  };

  struct Decltype {
    DecltypeClass Class;
    uint32_t      VectorCount;
    uint32_t      Flags;
  };

  Decltype ClassifyDecltype(D3DDECLTYPE Type) {
    switch (Type) {
      case D3DDECLTYPE_FLOAT1:    return { DecltypeClass::Float, 1, DecltypeFlags::Signed };
      case D3DDECLTYPE_FLOAT2:    return { DecltypeClass::Float, 2, DecltypeFlags::Signed };
      case D3DDECLTYPE_FLOAT3:    return { DecltypeClass::Float, 3, DecltypeFlags::Signed };
      case D3DDECLTYPE_FLOAT4:    return { DecltypeClass::Float, 4, DecltypeFlags::Signed };
      case D3DDECLTYPE_D3DCOLOR:  return { DecltypeClass::Byte,  4, DecltypeFlags::Normalize | DecltypeFlags::ReverseRGB };
      case D3DDECLTYPE_UBYTE4:    return { DecltypeClass::Byte,  4, 0 };
      case D3DDECLTYPE_SHORT2:    return { DecltypeClass::Short, 2, DecltypeFlags::Signed };
      case D3DDECLTYPE_SHORT4:    return { DecltypeClass::Short, 4, DecltypeFlags::Signed };
      case D3DDECLTYPE_UBYTE4N:   return { DecltypeClass::Byte,  4, DecltypeFlags::Normalize };
      case D3DDECLTYPE_SHORT2N:   return { DecltypeClass::Short, 2, DecltypeFlags::Signed | DecltypeFlags::Normalize };
      case D3DDECLTYPE_SHORT4N:   return { DecltypeClass::Short, 4, DecltypeFlags::Signed | DecltypeFlags::Normalize };
      case D3DDECLTYPE_USHORT2N:  return { DecltypeClass::Short, 2, DecltypeFlags::Normalize };
      case D3DDECLTYPE_USHORT4N:  return { DecltypeClass::Short, 4, DecltypeFlags::Normalize };
      case D3DDECLTYPE_UDEC3:     return { DecltypeClass::Dec,   3, 0 };
      case D3DDECLTYPE_DEC3N:     return { DecltypeClass::Dec,   3, DecltypeFlags::Signed | DecltypeFlags::Normalize };
      case D3DDECLTYPE_FLOAT16_2: return { DecltypeClass::Half,  2, DecltypeFlags::Signed };
      case D3DDECLTYPE_FLOAT16_4: return { DecltypeClass::Half,  4, DecltypeFlags::Signed };
      default:                    return { DecltypeClass::Float, 4, DecltypeFlags::Signed };
    }
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

      return D3D9ShaderResourceMapping::getSWVPBufferSlot();
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


  class D3D9SWVPEmulatorGenerator {

  public:

    D3D9SWVPEmulatorGenerator(const std::string& name)
    : m_module(spvVersion(1, 3)) {
      m_entryPointId = m_module.allocateId();

      m_module.setDebugSource(
        spv::SourceLanguageUnknown, 0,
        m_module.addDebugString(name.c_str()),
        nullptr);

      m_module.setMemoryModel(
        spv::AddressingModelLogical,
        spv::MemoryModelGLSL450);

      m_module.enableCapability(spv::CapabilityGeometry);

      m_module.setExecutionMode(m_entryPointId, spv::ExecutionModeInputPoints);
      m_module.setExecutionMode(m_entryPointId, spv::ExecutionModeOutputPoints);
      // This has to be > 0 for some reason even though
      // we will never emit a vertex
      m_module.setOutputVertices(m_entryPointId, 1);
      m_module.setInvocations(m_entryPointId, 1);

      m_module.functionBegin(m_module.defVoidType(), m_entryPointId, m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr), spv::FunctionControlMaskNone);
      m_module.opLabel(m_module.allocateId());
    }

    void compile(const D3D9CompactVertexElements& elements) {
      uint32_t uint_t     = m_module.defIntType(32, false);
      uint32_t float_t    = m_module.defFloatType(32);
      uint32_t vec4_t     = m_module.defVectorType(float_t, 4);

      uint32_t vec4_singular_array_t = m_module.defArrayType(vec4_t, m_module.constu32(1));

      uint32_t push_const_t = m_module.defStructTypeUnique(1, &uint_t);
      m_module.decorate(push_const_t, spv::DecorationBlock);
      m_module.setDebugName(push_const_t, "pc_t");
      m_module.setDebugMemberName(push_const_t, 0, "offset");
      m_module.memberDecorateOffset(push_const_t, 0, MaxSharedPushDataSize);

      uint32_t push_const_uint_ptr_t = m_module.defPointerType(uint_t, spv::StorageClassPushConstant);
      uint32_t push_const_ptr_t = m_module.defPointerType(push_const_t, spv::StorageClassPushConstant);

      uint32_t pushConst = m_module.newVar(push_const_ptr_t, spv::StorageClassPushConstant);
      m_module.setDebugName(pushConst, "pc");

      // Setup the buffer
      uint32_t bufferSlot = D3D9ShaderResourceMapping::getSWVPBufferSlot();

      uint32_t arrayType    = m_module.defRuntimeArrayTypeUnique(uint_t);
      m_module.decorateArrayStride(arrayType, sizeof(uint32_t));

      uint32_t buffer_t     = m_module.defStructTypeUnique(1, &arrayType);
      m_module.memberDecorateOffset(buffer_t, 0, 0);
      m_module.decorate(buffer_t, spv::DecorationBufferBlock);

      uint32_t buffer       = m_module.newVar(m_module.defPointerType(buffer_t, spv::StorageClassUniform), spv::StorageClassUniform);
      m_module.decorateDescriptorSet(buffer, 0);
      m_module.decorateBinding(buffer, bufferSlot);

      m_bufferBinding = { };
      m_bufferBinding.set             = 0u;
      m_bufferBinding.binding         = bufferSlot;
      m_bufferBinding.resourceIndex   = bufferSlot;
      m_bufferBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      m_bufferBinding.access          = VK_ACCESS_SHADER_WRITE_BIT;

      // Load our builtins
      uint32_t primitiveIdPtr = m_module.newVar(m_module.defPointerType(uint_t, spv::StorageClassInput), spv::StorageClassInput);
      m_module.decorateBuiltIn(primitiveIdPtr, spv::BuiltInPrimitiveId);

      uint32_t primitiveId = m_module.opLoad(uint_t, primitiveIdPtr);
      
      // The size of any given vertex
      uint32_t size = 0;
      for (uint32_t i = 0; i < elements.size(); i++) {
        const auto& element = elements[i];
        if (element.Stream == 0 && element.Type != D3DDECLTYPE_UNUSED) {
          size = std::max(size, element.Offset + GetDecltypeSize(D3DDECLTYPE(element.Type)));
        }
      }

      uint32_t vertexSize       = m_module.constu32(size / sizeof(uint32_t));

      // The offset of this vertex from the beginning of the buffer
      uint32_t pushConstIndex = m_module.constu32(0u);

      uint32_t globalOffset = m_module.opLoad(uint_t, m_module.opAccessChain(
        push_const_uint_ptr_t, pushConst, 1, &pushConstIndex));
               globalOffset = m_module.opShiftRightLogical(uint_t, globalOffset, m_module.constu32(2u));

      uint32_t thisVertexOffset = m_module.opIMul(uint_t, vertexSize, primitiveId);
               thisVertexOffset = m_module.opIAdd(uint_t, thisVertexOffset, globalOffset);

      for (uint32_t i = 0; i < elements.size(); i++) {
        const auto& element = elements[i];
        // Load the slot associated with this element
        DxsoSemantic semantic = { DxsoUsage(element.Usage), element.UsageIndex };

        uint32_t elementPtr;
        uint32_t elementVar;

        elementPtr = m_module.newVar(m_module.defPointerType(vec4_singular_array_t, spv::StorageClassInput), spv::StorageClassInput);
        if ((semantic.usage == DxsoUsage::Position || semantic.usage == DxsoUsage::PositionT) && element.UsageIndex == 0) {
          // Load from builtin
          m_module.decorateBuiltIn(elementPtr, spv::BuiltInPosition);
        }
        else {
          // Load from slot
          uint32_t slotIdx      = RegisterLinkerSlot(semantic);

          m_module.decorateLocation(elementPtr, slotIdx);
          m_inputMask |= 1u << slotIdx;
        }

        uint32_t zero = m_module.constu32(0);
        elementVar = m_module.opAccessChain(m_module.defPointerType(vec4_t, spv::StorageClassInput), elementPtr, 1, &zero);
        elementVar = m_module.opLoad(vec4_t, elementVar);

        // The offset of this element from the beginning of any given vertex
        uint32_t perVertexElementOffset = m_module.constu32(element.Offset / sizeof(uint32_t));

        // The offset of this element from the beginning of the buffer for **THIS** vertex
        uint32_t elementOffset          = m_module.opIAdd(uint_t, thisVertexOffset, perVertexElementOffset);

        // Write to the buffer at the element offset for each part of the vector.
        Decltype elementInfo = ClassifyDecltype(D3DDECLTYPE(element.Type));

        if (elementInfo.Class == DecltypeClass::Dec) {
          // TODO!
          Logger::warn("Encountered DEC3/UDEC3N class, ignoring...");
          continue;
        }

        uint32_t vecn_t = m_module.defVectorType(float_t, elementInfo.VectorCount);
        uint32_t componentSet;
        
        // Modifiers...
        if (elementInfo.Flags & DecltypeFlags::ReverseRGB) {
          std::array<uint32_t, 4> indices = { 2, 1, 0, 3 };
          componentSet = m_module.opVectorShuffle(vecn_t, elementVar, elementVar, elementInfo.VectorCount, indices.data());
        }
        else {
          std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };
          componentSet = m_module.opVectorShuffle(vecn_t, elementVar, elementVar, elementInfo.VectorCount, indices.data());
        }

        if (elementInfo.Flags & DecltypeFlags::Normalize)
          componentSet = m_module.opVectorTimesScalar(vecn_t, componentSet, m_module.constf32(255.0f));


        bool isSigned = elementInfo.Flags & DecltypeFlags::Signed;

        // Convert the component to the correct type/value.
        switch (elementInfo.Class) {
          case DecltypeClass::Float: break; // Do nothing!
          case DecltypeClass::Byte: {
            m_module.enableCapability(spv::CapabilityInt8);

            uint32_t type = m_module.defIntType(8, isSigned);
                     type = m_module.defVectorType(type, elementInfo.VectorCount);

            componentSet = isSigned
              ? m_module.opConvertFtoS(type, componentSet)
              : m_module.opConvertFtoU(type, componentSet);

            break;
          }
          case DecltypeClass::Short: {
            m_module.enableCapability(spv::CapabilityInt16);

            uint32_t type = m_module.defIntType(16, isSigned);
                     type = m_module.defVectorType(type, elementInfo.VectorCount);

            componentSet = isSigned
              ? m_module.opConvertFtoS(type, componentSet)
              : m_module.opConvertFtoU(type, componentSet);

            break;
          }
          case DecltypeClass::Half: {
            m_module.enableCapability(spv::CapabilityFloat16);

            uint32_t type = m_module.defFloatType(16);
                     type = m_module.defVectorType(type, elementInfo.VectorCount);
            componentSet = m_module.opFConvert(type, componentSet);

            break;
          }
          case DecltypeClass::Dec: {
            // TODO!
            break;
          }
        }

        // Bitcast to dwords before we write.
        uint32_t dwordCount  = GetDecltypeSize(D3DDECLTYPE(element.Type)) / sizeof(uint32_t);
        uint32_t dwordVal = m_module.opBitcast(
          dwordCount != 1 ? m_module.defVectorType(uint_t, dwordCount) : uint_t,
          componentSet);

        // Finally write each dword to the buffer!
        for (uint32_t i = 0; i < dwordCount; i++) {
          std::array<uint32_t, 2> bufferIndices = { m_module.constu32(0), elementOffset };

          uint32_t writeDest = m_module.opAccessChain(m_module.defPointerType(uint_t, spv::StorageClassUniform), buffer, bufferIndices.size(), bufferIndices.data());
          uint32_t currentDword = dwordCount != 1 ? m_module.opCompositeExtract(uint_t, dwordVal, 1, &i) : dwordVal;

          m_module.opStore(writeDest, currentDword);

          elementOffset = m_module.opIAdd(uint_t, elementOffset, m_module.constu32(1));
        }
      }
    }

    Rc<DxvkShader> finalize() {
      m_module.opReturn();
      m_module.functionEnd();

      m_module.addEntryPoint(m_entryPointId,
        spv::ExecutionModelGeometry, "main");
      m_module.setDebugName(m_entryPointId, "main");

      DxvkSpirvShaderCreateInfo info = { };
      info.bindingCount = 1;
      info.bindings = &m_bufferBinding;
      info.localPushData = DxvkPushDataBlock(MaxSharedPushDataSize, sizeof(D3D9SwvpEmuArgs), 4u, 0u);

      return new DxvkSpirvShader(info, m_module.compile());
    }

  private:

    SpirvModule m_module;

    uint32_t              m_entryPointId = 0;
    uint32_t              m_inputMask = 0u;
    DxvkBindingInfo       m_bufferBinding;

  };

  Rc<DxvkShader> D3D9SWVPEmulator::GetShaderModule(D3D9DeviceEx* pDevice, D3D9CompactVertexElements&& elements) {
    // Use the shader's unique key for the lookup
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto entry = m_modules.find(elements);
      if (entry != m_modules.end())
        return entry->second;
    }

    Sha1Hash hash = Sha1Hash::compute(elements.data(), elements.size() * sizeof(elements[0]));
    DxvkShaderHash key(VK_SHADER_STAGE_GEOMETRY_BIT, 0u, hash.digest(), hash.digestLength());
    
    // This shader has not been compiled yet, so we have to create a
    // new module. This takes a while, so we won't lock the structure.
    DxvkIrShaderCreateInfo createInfo = { };
    createInfo.options = pDevice->GetShaderOptions();

    Rc<DxvkShader> shader = pDevice->GetDXVKDevice()->createCachedShader(key.toString(), createInfo, nullptr);

    if (!shader) {
      shader = pDevice->GetDXVKDevice()->createCachedShader(key.toString(), createInfo,
        new D3D9SWVPShaderGenerator(key, elements));
    }

    pDevice->GetDXVKDevice()->registerShader(shader);
    
    // Insert the new module into the lookup table. If another thread
    // has compiled the same shader in the meantime, we should return
    // that object instead and discard the newly created module.
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      std::pair<D3D9CompactVertexElements, Rc<DxvkShader>> pair = { std::move(elements), shader };
      auto status = m_modules.insert(std::move(pair));
      if (!status.second)
        return status.first->second;
    }

    return shader;
  }

}
