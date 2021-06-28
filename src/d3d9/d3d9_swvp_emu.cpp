#include "d3d9_swvp_emu.h"

#include "d3d9_device.h"
#include "d3d9_vertex_declaration.h"

#include "../spirv/spirv_module.h"

namespace dxvk {

  // Doesn't compare everything, only what we use in SWVP.

  size_t D3D9VertexDeclHash::operator () (const D3D9VertexElements& key) const {
    DxvkHashState hash;

    std::hash<BYTE> bytehash;
    std::hash<WORD> wordhash;

    for (auto& element : key) {
      hash.add(wordhash(element.Stream));
      hash.add(wordhash(element.Offset));
      hash.add(bytehash(element.Type));
      hash.add(bytehash(element.Method));
      hash.add(bytehash(element.Usage));
      hash.add(bytehash(element.UsageIndex));
    }

    return hash;
  }

  bool D3D9VertexDeclEq::operator () (const D3D9VertexElements& a, const D3D9VertexElements& b) const {
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

    void compile(const D3D9VertexDecl* pDecl) {
      uint32_t uint_t     = m_module.defIntType(32, false);
      uint32_t float_t    = m_module.defFloatType(32);
      uint32_t vec4_t     = m_module.defVectorType(float_t, 4);

      uint32_t vec4_singular_array_t = m_module.defArrayType(vec4_t, m_module.constu32(1));

      // Setup the buffer
      uint32_t bufferSlot = getSWVPBufferSlot();

      uint32_t arrayType    = m_module.defRuntimeArrayTypeUnique(uint_t);
      m_module.decorateArrayStride(arrayType, sizeof(uint32_t));

      uint32_t buffer_t     = m_module.defStructTypeUnique(1, &arrayType);
      m_module.memberDecorateOffset(buffer_t, 0, 0);
      m_module.decorate(buffer_t, spv::DecorationBufferBlock);

      uint32_t buffer       = m_module.newVar(m_module.defPointerType(buffer_t, spv::StorageClassUniform), spv::StorageClassUniform);
      m_module.decorateDescriptorSet(buffer, 0);
      m_module.decorateBinding(buffer, bufferSlot);

      DxvkResourceSlot bufferRes;
      bufferRes.slot   = bufferSlot;
      bufferRes.type   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bufferRes.view   = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
      bufferRes.access = VK_ACCESS_SHADER_WRITE_BIT;
      m_resourceSlots.push_back(bufferRes);

      // Load our builtins
      uint32_t primitiveIdPtr = m_module.newVar(m_module.defPointerType(uint_t, spv::StorageClassInput), spv::StorageClassInput);
      m_module.decorateBuiltIn(primitiveIdPtr, spv::BuiltInPrimitiveId);
      m_entryPointInterfaces.push_back(primitiveIdPtr);

      uint32_t primitiveId = m_module.opLoad(uint_t, primitiveIdPtr);

      // The size of any given vertex
      uint32_t vertexSize       = m_module.constu32(pDecl->GetSize() / sizeof(uint32_t));

      //The offset of this vertex from the beginning of the buffer
      uint32_t thisVertexOffset = m_module.opIMul(uint_t, vertexSize, primitiveId);


      for (auto& element : pDecl->GetElements()) {
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
          m_interfaceSlots.inputSlots |= 1u << slotIdx;
        }

        uint32_t zero = m_module.constu32(0);
        elementVar = m_module.opAccessChain(m_module.defPointerType(vec4_t, spv::StorageClassInput), elementPtr, 1, &zero);
        elementVar = m_module.opLoad(vec4_t, elementVar);

        m_entryPointInterfaces.push_back(elementPtr);

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
        uint32_t dwordVector = m_module.opBitcast(
          m_module.defVectorType(uint_t, dwordCount),
          componentSet);

        // Finally write each dword to the buffer!
        for (uint32_t i = 0; i < dwordCount; i++) {
          std::array<uint32_t, 2> bufferIndices = { m_module.constu32(0), elementOffset };

          uint32_t writeDest = m_module.opAccessChain(m_module.defPointerType(uint_t, spv::StorageClassUniform), buffer, bufferIndices.size(), bufferIndices.data());
          uint32_t currentDword = m_module.opCompositeExtract(uint_t, dwordVector, 1, &i);

          m_module.opStore(writeDest, currentDword);

          elementOffset = m_module.opIAdd(uint_t, elementOffset, m_module.constu32(1));
        }
      }
    }

    Rc<DxvkShader> finalize() {
      m_module.opReturn();
      m_module.functionEnd();

      m_module.addEntryPoint(m_entryPointId,
        spv::ExecutionModelGeometry, "main",
        m_entryPointInterfaces.size(),
        m_entryPointInterfaces.data());
      m_module.setDebugName(m_entryPointId, "main");

      DxvkShaderConstData constData = { };

      return new DxvkShader(
        VK_SHADER_STAGE_GEOMETRY_BIT,
        m_resourceSlots.size(),
        m_resourceSlots.data(),
        m_interfaceSlots,
        m_module.compile(),
        DxvkShaderOptions(),
        std::move(constData));
    }

  private:

    SpirvModule m_module;

    std::vector<uint32_t> m_entryPointInterfaces;
    uint32_t              m_entryPointId = 0;

    std::vector<DxvkResourceSlot> m_resourceSlots;
    DxvkInterfaceSlots m_interfaceSlots;

  };

  Rc<DxvkShader> D3D9SWVPEmulator::GetShaderModule(D3D9DeviceEx* pDevice, const D3D9VertexDecl* pDecl) {
    auto& elements = pDecl->GetElements();

    // Use the shader's unique key for the lookup
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto entry = m_modules.find(elements);
      if (entry != m_modules.end())
        return entry->second;
    }

    Sha1Hash hash = Sha1Hash::compute(
      elements.data(), elements.size() * sizeof(elements[0]));

    DxvkShaderKey key = { VK_SHADER_STAGE_GEOMETRY_BIT , hash };
    std::string name = str::format("SWVP_", key.toString());
    
    // This shader has not been compiled yet, so we have to create a
    // new module. This takes a while, so we won't lock the structure.
    D3D9SWVPEmulatorGenerator generator(name);
    generator.compile(pDecl);
    Rc<DxvkShader> shader = generator.finalize();

    shader->setShaderKey(key);
    pDevice->GetDXVKDevice()->registerShader(shader);

    const std::string dumpPath = env::getEnvVar("DXVK_SHADER_DUMP_PATH");

    if (dumpPath.size() != 0) {
      std::ofstream dumpStream(
        str::format(dumpPath, "/", name, ".spv"),
        std::ios_base::binary | std::ios_base::trunc);

      shader->dump(dumpStream);
    }
    
    // Insert the new module into the lookup table. If another thread
    // has compiled the same shader in the meantime, we should return
    // that object instead and discard the newly created module.
    { std::unique_lock<dxvk::mutex> lock(m_mutex);
      
      auto status = m_modules.insert({ elements, shader });
      if (!status.second)
        return status.first->second;
    }

    return shader;
  }

}