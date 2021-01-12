#include "dxbc_decoder.h"

namespace dxvk {
  
  const uint32_t* DxbcCodeSlice::ptrAt(uint32_t id) const {
    if (m_ptr + id >= m_end)
      throw DxvkError("DxbcCodeSlice: End of stream");
    return m_ptr + id;
  }
  
  
  uint32_t DxbcCodeSlice::at(uint32_t id) const {
    if (m_ptr + id >= m_end)
      throw DxvkError("DxbcCodeSlice: End of stream");
    return m_ptr[id];
  }
  
  
  uint32_t DxbcCodeSlice::read() {
    if (m_ptr >= m_end)
      throw DxvkError("DxbcCodeSlice: End of stream");
    return *(m_ptr++);
  }
  
  
  DxbcCodeSlice DxbcCodeSlice::take(uint32_t n) const {
    if (m_ptr + n > m_end)
      throw DxvkError("DxbcCodeSlice: End of stream");
    return DxbcCodeSlice(m_ptr, m_ptr + n);
  }
  
  
  DxbcCodeSlice DxbcCodeSlice::skip(uint32_t n) const {
    if (m_ptr + n > m_end)
      throw DxvkError("DxbcCodeSlice: End of stream");
    return DxbcCodeSlice(m_ptr + n, m_end);
  }


  static bool check_f1_2020() {
      const char *sgi = getenv("SteamGameId");
      return sgi && strcmp(sgi, "1080110") == 0;
  }
  
  
  void DxbcDecodeContext::decodeInstruction(DxbcCodeSlice& code) {
    const uint32_t token0 = code.at(0);
    
    // Initialize the instruction structure. Some of these values
    // may not get written otherwise while decoding the instruction.
    m_instruction.op             = static_cast<DxbcOpcode>(bit::extract(token0, 0, 10));
    m_instruction.opClass        = DxbcInstClass::Undefined;
    m_instruction.sampleControls = { 0, 0, 0 };
    m_instruction.dstCount       = 0;
    m_instruction.srcCount       = 0;
    m_instruction.immCount       = 0;
    m_instruction.dst            = m_dstOperands.data();
    m_instruction.src            = m_srcOperands.data();
    m_instruction.imm            = m_immOperands.data();
    m_instruction.customDataType = DxbcCustomDataClass::Comment;
    m_instruction.customDataSize = 0;
    m_instruction.customData     = nullptr;
    
    // Reset the index pointer, which may still contain
    // a non-zero value from the previous iteration
    m_indexId = 0;
    
    // Instruction length, in DWORDs. This includes the token
    // itself and any other prefix that an instruction may have.
    uint32_t length = 0;
    
    if (m_instruction.op == DxbcOpcode::CustomData) {
      length = code.at(1);
      this->decodeCustomData(code.take(length));
    } else {
      length = bit::extract(token0, 24, 30);
      this->decodeOperation(code.take(length));
    }

    static bool is_f1_2020 = check_f1_2020();
    if (is_f1_2020 && m_name.substr(0, 3) == "CS_") {
        if (m_instruction.op == DxbcOpcode::DclTemps) {
            // Allocate a new temporary; tcl_temps is not used by
            // DXVK, but let's keep it consistent
            m_new_temp = m_immOperands[0].u32++;
        }

        if (m_instruction.opClass == DxbcInstClass::BufferStore) {
            const auto &dst = m_instruction.dst[0];
            if (dst.type == DxbcOperandType::ThreadGroupSharedMemory) {
                if (dst.idxDim == 1 && dst.idx[0].relReg == nullptr) {
                    const auto reg_idx = dst.idx[0].offset;
                    m_written_buffers.insert(reg_idx);
                }
            }
        } else if (m_instruction.opClass == DxbcInstClass::BufferLoad) {
            const auto &src = m_instruction.src[m_instruction.srcCount-1];
            if (src.type == DxbcOperandType::ThreadGroupSharedMemory) {
                if (src.idxDim == 1 && src.idx[0].relReg == nullptr) {
                    const auto reg_idx = src.idx[0].offset;
                    if (m_written_buffers.find(reg_idx) != m_written_buffers.end()) {
                        if (m_if_stack.empty() || (m_if_stack.size() == 1 && m_if_stack[0] == 0)) {
                            if (m_emitting_sync == 0) {
                                m_emitting_sync = 1;
                                if (!m_if_stack.empty()) {
                                    m_instruction.op = DxbcOpcode::EndIf;
                                    m_instruction.opClass = DxbcInstClass::ControlFlow;
                                    m_instruction.controls = 0;
                                    m_instruction.sampleControls = {0, 0, 0};
                                    m_instruction.srcCount = 0;
                                    m_instruction.dstCount = 0;
                                    m_instruction.immCount = 0;
                                    return;
                                }
                            }
                            if (m_emitting_sync == 1) {
                                m_emitting_sync = 2;
                                m_instruction.op = DxbcOpcode::Sync;
                                m_instruction.opClass = DxbcInstClass::Barrier;
                                m_instruction.controls = 0x1800;
                                m_instruction.sampleControls = {0, 0, 0};
                                m_instruction.srcCount = 0;
                                m_instruction.dstCount = 0;
                                m_instruction.immCount = 0;
                                return;
                            }
                            if (m_emitting_sync == 2) {
                                m_emitting_sync = 3;
                                if (!m_if_stack.empty()) {
                                    m_instruction.op = DxbcOpcode::If;
                                    m_instruction.opClass = DxbcInstClass::ControlFlow;
                                    m_instruction.controls = (m_zero_test == DxbcZeroTest::TestNz) << 18;
                                    m_instruction.sampleControls = {0, 0, 0};
                                    m_instruction.srcCount = 1;
                                    m_instruction.dstCount = 0;
                                    m_instruction.immCount = 0;
                                    m_srcOperands[0].type = DxbcOperandType::Temp;
                                    m_srcOperands[0].dataType = DxbcScalarType::Uint32;
                                    m_srcOperands[0].componentCount = DxbcComponentCount::Component4;
                                    m_srcOperands[0].idxDim = 1;
                                    m_srcOperands[0].idx[0].relReg = nullptr;
                                    m_srcOperands[0].idx[0].offset = m_new_temp;
                                    m_srcOperands[0].mask = DxbcRegMask(true, false, false, false);
                                    m_srcOperands[0].swizzle = DxbcRegSwizzle(0, 1, 2, 3);
                                    m_srcOperands[0].modifiers = 0;
                                    return;
                                }
                            }
                            if (m_emitting_sync == 3) {
                                m_emitting_sync = 0;
                                m_written_buffers.clear();
                            }
                        } else {
                            // To deep in the stack, we cannot emit a
                            // control barrier
                        }
                    }
                }
            }
        }

        switch (m_instruction.op) {
        case DxbcOpcode::If: m_if_stack.push_back(0); break;
        case DxbcOpcode::Else: m_if_stack.back() = 1; break;
        case DxbcOpcode::EndIf: m_if_stack.pop_back(); break;
        case DxbcOpcode::Loop: m_if_stack.push_back(2); break;
        case DxbcOpcode::EndLoop: m_if_stack.pop_back(); break;
        case DxbcOpcode::Switch: m_if_stack.push_back(3); break;
        case DxbcOpcode::EndSwitch: m_if_stack.pop_back(); break;
        default: break;
        }

        if (m_instruction.op == DxbcOpcode::If && m_if_stack.size() == 1 && m_if_stack[0] == 0) {
            if (m_if_condition_saved) {
                m_if_condition_saved = false;
            } else {
                // Save if condition
                m_if_condition_saved = true;
                m_zero_test = m_instruction.controls.zeroTest();
                m_if_stack.pop_back();

                m_instruction.op = DxbcOpcode::Mov;
                m_instruction.opClass = DxbcInstClass::VectorAlu;
                m_instruction.controls = 0;
                m_instruction.sampleControls = {0, 0, 0};
                m_instruction.srcCount = 1;
                m_instruction.dstCount = 1;
                m_instruction.immCount = 0;
                // Source is already correct from the if
                m_dstOperands[0].type = DxbcOperandType::Temp;
                m_dstOperands[0].dataType = DxbcScalarType::Uint32;
                m_dstOperands[0].componentCount = DxbcComponentCount::Component4;
                m_dstOperands[0].idxDim = 1;
                m_dstOperands[0].idx[0].relReg = nullptr;
                m_dstOperands[0].idx[0].offset = m_new_temp;
                m_dstOperands[0].mask = DxbcRegMask(true, false, false, false);
                m_dstOperands[0].swizzle = DxbcRegSwizzle(0, 1, 2, 3);
                m_dstOperands[0].modifiers = 0;
                return;
            }
        }
    }
    
    // Advance the caller's slice to the next token so that
    // they can make consecutive calls to decodeInstruction()
    code = code.skip(length);
  }
  
  
  void DxbcDecodeContext::decodeCustomData(DxbcCodeSlice code) {
    const uint32_t blockLength = code.at(1);
    
    if (blockLength < 2) {
      Logger::err("DxbcDecodeContext: Invalid custom data block");
      return;
    }
    
    // Custom data blocks have their own instruction class
    m_instruction.op      = DxbcOpcode::CustomData;
    m_instruction.opClass = DxbcInstClass::CustomData;
    
    // We'll point into the code buffer rather than making a copy
    m_instruction.customDataType = static_cast<DxbcCustomDataClass>(
      bit::extract(code.at(0), 11, 31));
    m_instruction.customDataSize = blockLength - 2;
    m_instruction.customData     = code.ptrAt(2);
  }
  
  
  void DxbcDecodeContext::decodeOperation(DxbcCodeSlice code) {
    uint32_t token = code.read();
    
    // Result modifiers, which are applied to common ALU ops
    m_instruction.modifiers.saturate = !!bit::extract(token, 13, 13);
    m_instruction.modifiers.precise  = !!bit::extract(token, 19, 22);
    
    // Opcode controls. It will depend on the
    // opcode itself which ones are valid.
    m_instruction.controls = DxbcShaderOpcodeControls(token);
    
    // Process extended opcode tokens
    while (bit::extract(token, 31, 31)) {
      token = code.read();
      
      const DxbcExtOpcode extOpcode
        = static_cast<DxbcExtOpcode>(bit::extract(token, 0, 5));
      
      switch (extOpcode) {
        case DxbcExtOpcode::SampleControls: {
          struct {
            int u : 4;
            int v : 4;
            int w : 4;
          } aoffimmi;
          
          aoffimmi.u = bit::extract(token,  9, 12);
          aoffimmi.v = bit::extract(token, 13, 16);
          aoffimmi.w = bit::extract(token, 17, 20);
          
          // Four-bit signed numbers, sign-extend them
          m_instruction.sampleControls.u = aoffimmi.u;
          m_instruction.sampleControls.v = aoffimmi.v;
          m_instruction.sampleControls.w = aoffimmi.w;
        } break;
        
        case DxbcExtOpcode::ResourceDim:
        case DxbcExtOpcode::ResourceReturnType:
          break;  // part of resource description
        
        default:
          Logger::warn(str::format(
            "DxbcDecodeContext: Unhandled extended opcode: ",
            extOpcode));
      }
    }
    
    // Retrieve the instruction format in order to parse the
    // operands. Doing this mostly automatically means that
    // the compiler can rely on the operands being valid.
    const DxbcInstFormat format = dxbcInstructionFormat(m_instruction.op);
    m_instruction.opClass = format.instructionClass;
    
    for (uint32_t i = 0; i < format.operandCount; i++)
      this->decodeOperand(code, format.operands[i]);
  }
  
  
  void DxbcDecodeContext::decodeComponentSelection(DxbcRegister& reg, uint32_t token) {
    // Pick the correct component selection mode based on the
    // component count. We'll simplify this here so that the
    // compiler can assume that everything is a 4D vector.
    reg.componentCount = static_cast<DxbcComponentCount>(bit::extract(token, 0, 1));
    
    switch (reg.componentCount) {
      // No components - used for samplers etc.
      case DxbcComponentCount::Component0:
        reg.mask    = DxbcRegMask(false, false, false, false);
        reg.swizzle = DxbcRegSwizzle(0, 0, 0, 0);
        break;
      
      // One component - used for immediates
      // and a few built-in registers.
      case DxbcComponentCount::Component1:
        reg.mask    = DxbcRegMask(true, false, false, false);
        reg.swizzle = DxbcRegSwizzle(0, 0, 0, 0);
        break;
      
      // Four components - everything else. This requires us
      // to actually parse the component selection mode.
      case DxbcComponentCount::Component4: {
        const DxbcRegMode componentMode =
          static_cast<DxbcRegMode>(bit::extract(token, 2, 3));
        
        switch (componentMode) {
          // Write mask for destination operands
          case DxbcRegMode::Mask:
            reg.mask    = bit::extract(token, 4, 7);
            reg.swizzle = DxbcRegSwizzle(0, 1, 2, 3);
            break;
          
          // Swizzle for source operands (including resources)
          case DxbcRegMode::Swizzle:
            reg.mask    = DxbcRegMask(true, true, true, true);
            reg.swizzle = DxbcRegSwizzle(
              bit::extract(token,  4,  5),
              bit::extract(token,  6,  7),
              bit::extract(token,  8,  9),
              bit::extract(token, 10, 11));
            break;
          
          // Selection of one component. We can generate both a
          // mask and a swizzle for this so that the compiler
          // won't have to deal with this case specifically.
          case DxbcRegMode::Select1: {
            const uint32_t n = bit::extract(token, 4, 5);
            reg.mask    = DxbcRegMask(n == 0, n == 1, n == 2, n == 3);
            reg.swizzle = DxbcRegSwizzle(n, n, n, n);
          } break;
          
          default:
            Logger::warn("DxbcDecodeContext: Invalid component selection mode");
        }
      } break;
          
      default:
        Logger::warn("DxbcDecodeContext: Invalid component count");
    }
  }
  
  
  void DxbcDecodeContext::decodeOperandExtensions(DxbcCodeSlice& code, DxbcRegister& reg, uint32_t token) {
    while (bit::extract(token, 31, 31)) {
      token = code.read();
      
      // Type of the extended operand token
      const DxbcOperandExt extTokenType =
        static_cast<DxbcOperandExt>(bit::extract(token, 0, 5));
      
      switch (extTokenType) {
        // Operand modifiers, which are used to manipulate the
        // value of a source operand during the load operation
        case DxbcOperandExt::OperandModifier:
          reg.modifiers = bit::extract(token, 6, 13);
          break;
        
        default:
          Logger::warn(str::format(
            "DxbcDecodeContext: Unhandled extended operand token: ",
            extTokenType));
      }
    }
  }
  
  
  void DxbcDecodeContext::decodeOperandImmediates(DxbcCodeSlice& code, DxbcRegister& reg) {
    if (reg.type == DxbcOperandType::Imm32
     || reg.type == DxbcOperandType::Imm64) {
      switch (reg.componentCount) {
        // This is commonly used if only one vector
        // component is involved in an operation
        case DxbcComponentCount::Component1: {
          reg.imm.u32_1 = code.read();
        } break;
        
        // Typical four-component vector
        case DxbcComponentCount::Component4: {
          reg.imm.u32_4[0] = code.read();
          reg.imm.u32_4[1] = code.read();
          reg.imm.u32_4[2] = code.read();
          reg.imm.u32_4[3] = code.read();
        } break;

        default:
          Logger::warn("DxbcDecodeContext: Invalid component count for immediate operand");
      }
    }
  }
  
  
  void DxbcDecodeContext::decodeOperandIndex(DxbcCodeSlice& code, DxbcRegister& reg, uint32_t token) {
    reg.idxDim = bit::extract(token, 20, 21);
    
    for (uint32_t i = 0; i < reg.idxDim; i++) {
      // An index can be encoded in various different ways
      const DxbcOperandIndexRepresentation repr =
        static_cast<DxbcOperandIndexRepresentation>(
          bit::extract(token, 22 + 3 * i, 24 + 3 * i));
      
      switch (repr) {
        case DxbcOperandIndexRepresentation::Imm32:
          reg.idx[i].offset = static_cast<int32_t>(code.read());
          reg.idx[i].relReg = nullptr;
          break;
        
        case DxbcOperandIndexRepresentation::Relative:
          reg.idx[i].offset = 0;
          reg.idx[i].relReg = &m_indices.at(m_indexId);
          
          this->decodeRegister(code,
            m_indices.at(m_indexId++),
            DxbcScalarType::Sint32);
          break;
        
        case DxbcOperandIndexRepresentation::Imm32Relative:
          reg.idx[i].offset = static_cast<int32_t>(code.read());
          reg.idx[i].relReg = &m_indices.at(m_indexId);
          
          this->decodeRegister(code,
            m_indices.at(m_indexId++),
            DxbcScalarType::Sint32);
          break;
        
        default:
          Logger::warn(str::format(
            "DxbcDecodeContext: Unhandled index representation: ",
            repr));
      }
    }
  }
  
  
  void DxbcDecodeContext::decodeRegister(DxbcCodeSlice& code, DxbcRegister& reg, DxbcScalarType type) {
    const uint32_t token = code.read();
    
    reg.type            = static_cast<DxbcOperandType>(bit::extract(token, 12, 19));
    reg.dataType        = type;
    reg.modifiers       = 0;
    reg.idxDim          = 0;
    
    for (uint32_t i = 0; i < DxbcMaxRegIndexDim; i++) {
      reg.idx[i].relReg = nullptr;
      reg.idx[i].offset = 0;
    }
    
    this->decodeComponentSelection(reg, token);
    this->decodeOperandExtensions(code, reg, token);
    this->decodeOperandImmediates(code, reg);
    this->decodeOperandIndex(code, reg, token);
  }
  
  
  void DxbcDecodeContext::decodeImm32(DxbcCodeSlice& code, DxbcImmediate& imm, DxbcScalarType type) {
    imm.u32 = code.read();
  }
  
  
  void DxbcDecodeContext::decodeOperand(DxbcCodeSlice& code, const DxbcInstOperandFormat& format) {
    switch (format.kind) {
      case DxbcOperandKind::DstReg: {
        const uint32_t operandId = m_instruction.dstCount++;
        this->decodeRegister(code, m_dstOperands.at(operandId), format.type);
      } break;
        
      case DxbcOperandKind::SrcReg: {
        const uint32_t operandId = m_instruction.srcCount++;
        this->decodeRegister(code, m_srcOperands.at(operandId), format.type);
      } break;
        
      case DxbcOperandKind::Imm32: {
        const uint32_t operandId = m_instruction.immCount++;
        this->decodeImm32(code, m_immOperands.at(operandId), format.type);
      } break;
      
      default:
        throw DxvkError("DxbcDecodeContext: Invalid operand format");
    }
  }
  
}