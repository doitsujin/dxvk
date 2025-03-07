#include "d3d9_shader_validator.h"

namespace dxvk {

  HRESULT STDMETHODCALLTYPE D3D9ShaderValidator::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = ref(this);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D9ShaderValidator::Begin(
      D3D9ShaderValidatorCallback pCallback,
      void*                       pUserData,
      DWORD                       Unknown) {
    if (unlikely(m_state != D3D9ShaderValidatorState::Begin)) {
      return ErrorCallback(nullptr, -1, 0, nullptr, 0,
        D3D9ShaderValidatorMessage::BeginOutOfOrder,
        "IDirect3DShaderValidator9::Begin called out of order. ::End must be called first.");
    }

    m_callback = pCallback;
    m_userData = pUserData;
    m_state    = D3D9ShaderValidatorState::ValidatingHeader;

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9ShaderValidator::Instruction(
      const char*  pFile,
            UINT   Line,
      const DWORD* pdwInst,
            DWORD  cdw) {
    if (unlikely(pdwInst == nullptr || !cdw)) {
      return ErrorCallback(pFile, Line, 0, pdwInst, cdw,
        D3D9ShaderValidatorMessage::InstructionNullArgs,
        "IDirect3DShaderValidator9::Instruction called with NULL == pdwInst or 0 == cdw.");
    }

    if (unlikely(m_state == D3D9ShaderValidatorState::Begin)) {
      return ErrorCallback(pFile, Line, 0, pdwInst, cdw,
        D3D9ShaderValidatorMessage::InstructionOutOfOrder,
        "IDirect3DShaderValidator9::Instruction called out of order. ::Begin must be called first.");
    } else if (unlikely(m_state == D3D9ShaderValidatorState::EndOfShader)) {
      return ErrorCallback(pFile, Line, 0, pdwInst, cdw,
        D3D9ShaderValidatorMessage::InstructionEndOfShader,
        "IDirect3DShaderValidator9::Instruction called out of order. After end token there should be no more instructions. Call ::End next.");
    } else if (unlikely(m_state == D3D9ShaderValidatorState::Error)) {
      return E_FAIL;
    }

    if (m_state == D3D9ShaderValidatorState::ValidatingHeader)
      return ValidateHeader(pFile, Line, pdwInst, cdw);

    DxsoCodeIter pdwInstIter{ reinterpret_cast<const uint32_t*>(pdwInst) };
    bool isEndToken = !m_ctx->decodeInstruction(pdwInstIter);
    const DxsoInstructionContext instContext = m_ctx->getInstructionContext();

    if (isEndToken)
      return ValidateEndToken(pFile, Line, pdwInst, cdw);

    // TODO: DxsoDecodeContext::decodeInstructionLength() does not currently appear
    // to return the correct token length in many cases, and as such dwordLength
    // will not be equal to cdw in many situations that are expected to pass validation
    //
    /*Logger::debug(str::format("IDirect3DShaderValidator9::Instruction: opcode ", instContext.instruction.opcode));
    // + 1 to account for the opcode...
    uint32_t dwordLength = instContext.instruction.tokenLength + 1;
    Logger::debug(str::format("IDirect3DShaderValidator9::Instruction: cdw ", cdw));
    Logger::debug(str::format("IDirect3DShaderValidator9::Instruction: dwordLength ", dwordLength));
    if (unlikely(cdw != dwordLength)) {
      return ErrorCallback(pFile, Line, 0x2,
        D3D9ShaderValidatorMessage::BadInstructionLength,
        str::format("Instruction length specified for instruction (", cdw, ") does not match the token count encountered (", dwordLength, "). Aborting validation."));
    }*/

    // a maximum of 10 inputs are supported with PS 3.0 (validation required by The Void)
    if (m_isPixelShader && m_majorVersion == 3) {
      switch (instContext.instruction.opcode) {
        case DxsoOpcode::Comment:
        case DxsoOpcode::Def:
        case DxsoOpcode::DefB:
        case DxsoOpcode::DefI:
          break;

        default:
          // Iterate over register tokens. Bit 31 of register tokens is always 1.
          for (uint32_t instNum = 1; instNum < cdw && (pdwInst[instNum] >> 31); instNum++) {
            DWORD regType  = ((pdwInst[instNum] & D3DSP_REGTYPE_MASK)  >> D3DSP_REGTYPE_SHIFT)
                            | ((pdwInst[instNum] & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2);
            DWORD regIndex = pdwInst[instNum] & D3DSP_REGNUM_MASK;

            if (unlikely(regType == static_cast<DWORD>(DxsoRegisterType::Input) && regIndex >= 10)) {
              return ErrorCallback(pFile, Line, 0x2, pdwInst, cdw,
                instContext.instruction.opcode == DxsoOpcode::Dcl ?
                  D3D9ShaderValidatorMessage::BadInputRegisterDeclaration :
                  D3D9ShaderValidatorMessage::BadInputRegister,
                str::format("IDirect3DShaderValidator9::Instruction: PS input registers index #", regIndex, " not valid for operand ", instNum, "."));
            }
          }
      }
    }

    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9ShaderValidator::End() {
    if (unlikely(m_state == D3D9ShaderValidatorState::Error)) {
      return E_FAIL;
    } else if (unlikely(m_state == D3D9ShaderValidatorState::Begin)) {
      return ErrorCallback(nullptr, 0, 0, nullptr, 0,
        D3D9ShaderValidatorMessage::EndOutOfOrder,
        "IDirect3DShaderValidator9::End called out of order. Call to ::Begin, followed by calls to ::Instruction must occur first.");
    } else if (unlikely(m_state != D3D9ShaderValidatorState::EndOfShader)) {
      return ErrorCallback(nullptr, 0, 0, nullptr, 0,
        D3D9ShaderValidatorMessage::MissingEndToken,
        "IDirect3DShaderValidator9::End: Shader missing end token.");
    }

    m_state    = D3D9ShaderValidatorState::Begin;
    m_isPixelShader = false;
    m_majorVersion  = 0;
    m_minorVersion  = 0;
    m_callback = nullptr;
    m_userData = nullptr;
    m_ctx      = nullptr;

    return D3D_OK;
  }


  HRESULT D3D9ShaderValidator::ValidateHeader(const char* pFile, UINT Line, const DWORD* pdwInst, DWORD cdw) {
    if (unlikely(cdw != 1)) {
      return ErrorCallback(pFile, Line, 0x6, pdwInst, cdw,
        D3D9ShaderValidatorMessage::BadVersionTokenLength,
        "IDirect3DShaderValidator9::Instruction: Bad version token. DWORD count > 1 given. Expected DWORD count to be 1 for version token.");
    }

    DxsoReader reader = { reinterpret_cast<const char*>(pdwInst) };
    uint32_t headerToken = reader.readu32();
    uint32_t shaderType  = headerToken & 0xffff0000;
    DxsoProgramType programType;

    if (shaderType == 0xffff0000) { // Pixel Shader
      programType = DxsoProgramTypes::PixelShader;
      m_isPixelShader = true;
    } else if (shaderType == 0xfffe0000) { // Vertex Shader
      programType = DxsoProgramTypes::VertexShader;
      m_isPixelShader = false;
    } else {
      return ErrorCallback(pFile, Line, 0x6, pdwInst, cdw,
        D3D9ShaderValidatorMessage::BadVersionTokenType,
        "IDirect3DShaderValidator9::Instruction: Bad version token. It indicates neither a pixel shader nor a vertex shader.");
    }

    m_majorVersion = D3DSHADER_VERSION_MAJOR(headerToken);
    m_minorVersion = D3DSHADER_VERSION_MINOR(headerToken);
    m_ctx   = std::make_unique<DxsoDecodeContext>(DxsoProgramInfo{ programType, m_minorVersion, m_majorVersion });
    m_state = D3D9ShaderValidatorState::ValidatingInstructions;

    const char* shaderTypeOutput = m_isPixelShader ? "PS" : "VS";
    Logger::debug(str::format("IDirect3DShaderValidator9::Instruction: Validating ",
                              shaderTypeOutput, " version ", m_majorVersion, ".", m_minorVersion));

    return D3D_OK;
  }


  HRESULT D3D9ShaderValidator::ValidateEndToken(const char* pFile, UINT Line, const DWORD* pdwInst, DWORD cdw) {
    // Reached the end token.
    if (unlikely(cdw != 1)) {
      return ErrorCallback(pFile, Line, 0x6, pdwInst, cdw,
        D3D9ShaderValidatorMessage::BadEndToken,
        "IDirect3DShaderValidator9::Instruction: Bad end token. DWORD count > 1 given. Expected DWORD count to be 1 for end token.");
    }

    m_state = D3D9ShaderValidatorState::EndOfShader;

    return D3D_OK;
  }


  HRESULT D3D9ShaderValidator::ErrorCallback(
      const char*                      pFile,
            UINT                       Line,
            DWORD                      Unknown,
      const DWORD*                     pInstr,
            DWORD                      InstrLength,
            D3D9ShaderValidatorMessage MessageID,
      const std::string&               Message) {
    if (m_callback)
      m_callback(pFile, Line, Unknown, MessageID, Message.c_str(), m_userData);

    // TODO: Consider switching this to debug, once we're
    // confident the implementation doesn't cause any issues
    Logger::warn(Message);

    // Log instruction that caused the error as raw bytecode
    if (Logger::logLevel() <= LogLevel::Debug && pInstr && InstrLength) {
      std::stringstream instMsg;

      for (uint32_t i = 0; i < InstrLength; i++) {
        instMsg << (i ? "," : " [");
        instMsg << std::hex << std::setfill('0') << std::setw(8) << pInstr[i];
        instMsg << (i + 1 == InstrLength ? "]" : "");
      }

      Logger::debug(instMsg.str());
    }

    m_state = D3D9ShaderValidatorState::Error;
    return E_FAIL;
  }

}