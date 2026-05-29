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

    dxbc_spv::util::ByteReader reader(pdwInst, sizeof(*pdwInst) * cdw);
    dxbc_spv::sm3::Instruction op(reader, m_header);

    if (!op) {
      return ErrorCallback(pFile, Line, 0x2, pdwInst, cdw,
        D3D9ShaderValidatorMessage::MissingEndToken,
        str::format("IDirect3DShaderValidator9::Instruction: Failed to parse instruction."));
    }

    if (op.getOpCode() == dxbc_spv::sm3::OpCode::eEnd)
      return ValidateEndToken(pFile, Line, pdwInst, cdw);

    // The Void relies on validating that there are at most 10 PS inputs
    if (m_header.getType() == dxbc_spv::sm3::ShaderType::ePixel
     && m_header.getVersion().first == 3u) {
      switch (op.getOpCode()) {
        case dxbc_spv::sm3::OpCode::eComment:
        case dxbc_spv::sm3::OpCode::eDef:
        case dxbc_spv::sm3::OpCode::eDefB:
        case dxbc_spv::sm3::OpCode::eDefI:
          break;

        default: {
          for (uint32_t i = 0u; i < op.getSrcCount(); i++) {
            const auto& operand = op.getSrc(i);

            if (operand.getRegisterType() == dxbc_spv::sm3::RegisterType::eInput && operand.getIndex() >= 10u) {
              return ErrorCallback(pFile, Line, 0x2, pdwInst, cdw,
                op.getOpCode() == dxbc_spv::sm3::OpCode::eDcl ?
                  D3D9ShaderValidatorMessage::BadInputRegisterDeclaration :
                  D3D9ShaderValidatorMessage::BadInputRegister,
                str::format("IDirect3DShaderValidator9::Instruction: PS input registers index #", operand.getIndex(), " not valid for source operand #", i, "."));
            }
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
    m_header   = dxbc_spv::sm3::ShaderInfo();
    m_callback = nullptr;
    m_userData = nullptr;
    return D3D_OK;
  }


  HRESULT D3D9ShaderValidator::ValidateHeader(const char* pFile, UINT Line, const DWORD* pdwInst, DWORD cdw) {
    if (unlikely(cdw != 1)) {
      return ErrorCallback(pFile, Line, 0x6, pdwInst, cdw,
        D3D9ShaderValidatorMessage::BadVersionTokenLength,
        "IDirect3DShaderValidator9::Instruction: Bad version token. DWORD count > 1 given. Expected DWORD count to be 1 for version token.");
    }

    dxbc_spv::util::ByteReader reader(pdwInst, sizeof(*pdwInst) * cdw);

    if (!(m_header = dxbc_spv::sm3::ShaderInfo(reader))) {
      return ErrorCallback(pFile, Line, 0x6, pdwInst, cdw,
        D3D9ShaderValidatorMessage::BadVersionTokenType,
        "IDirect3DShaderValidator9::Instruction: Bad version token. It indicates neither a pixel shader nor a vertex shader.");
    }

    m_state = D3D9ShaderValidatorState::ValidatingInstructions;

    Logger::debug(str::format("IDirect3DShaderValidator9::Instruction: Validating ",
      m_header.getType(), " version ", m_header.getVersion().first, ".", m_header.getVersion().second));

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
