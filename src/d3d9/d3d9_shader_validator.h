#pragma once

#include "d3d9_include.h"

#include "../dxso/dxso_header.h"
#include "../dxso/dxso_decoder.h"

namespace dxvk {

  enum class D3D9ShaderValidatorMessage : uint32_t {
    BeginOutOfOrder = 0xeb,
    InstructionOutOfOrder = 0xec,
    InstructionEndOfShader = 0xed,
    InstructionNullArgs = 0xee,
    BadVersionTokenLength = 0xef,
    BadVersionTokenType = 0xf0,
    BadEndToken = 0xf1,
    EndOutOfOrder = 0xf2,
    MissingEndToken = 0xf3,

    BadInstructionLength = 0x21e,
  };

  using D3D9ShaderValidatorCallback = HRESULT(STDMETHODCALLTYPE *)(
    const char*                      pFile,
          UINT                       Line,
          DWORD                      Unknown,
          D3D9ShaderValidatorMessage MessageID,
    const char*                      pMessage,
          void*                      pUserData);

  class IDirect3DShaderValidator9 : public IUnknown {

  public:

    virtual HRESULT STDMETHODCALLTYPE Begin(
            D3D9ShaderValidatorCallback pCallback,
            void*                       pUserParam,
            DWORD                       Unknown) = 0;

    virtual HRESULT STDMETHODCALLTYPE Instruction(
      const char*  pUnknown1,
            UINT   Unknown2,
      const DWORD* pdwInst,
            DWORD  cdw) = 0;

    virtual HRESULT STDMETHODCALLTYPE End() = 0;

  };

  enum class D3D9ShaderValidatorState {
    Begin,
    ValidatingHeader,
    ValidatingInstructions,
    EndOfShader,
    Error,
  };

  class D3D9ShaderValidator final : public ComObjectClamp<IDirect3DShaderValidator9> {

  public:

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      if (ppvObject == nullptr)
        return E_POINTER;

      *ppvObject = ref(this);
      return S_OK;
    }


    HRESULT STDMETHODCALLTYPE Begin(
            D3D9ShaderValidatorCallback pCallback,
            void*                       pUserData,
            DWORD                       Unknown) {
      if (m_state != D3D9ShaderValidatorState::Begin) {
        return ErrorCallback(nullptr, -1, 0,
          D3D9ShaderValidatorMessage::BeginOutOfOrder,
          "IDirect3DShaderValidator9::Begin called out of order. ::End must be called first.");
      }

      m_callback = pCallback;
      m_userData = pUserData;
      m_state    = D3D9ShaderValidatorState::ValidatingHeader;

      return D3D_OK;
    }


    HRESULT STDMETHODCALLTYPE Instruction(
      const char*  pFile,
            UINT   Line,
      const DWORD* pdwInst,
            DWORD  cdw) {
      if (m_state == D3D9ShaderValidatorState::Begin) {
        return ErrorCallback(pFile, Line, 0,
          D3D9ShaderValidatorMessage::InstructionOutOfOrder,
          "IDirect3DShaderValidator9::Instruction called out of order. ::Begin must be called first.");
      } else if (m_state == D3D9ShaderValidatorState::EndOfShader) {
        return ErrorCallback(pFile, Line, 0,
          D3D9ShaderValidatorMessage::InstructionEndOfShader,
          "IDirect3DShaderValidator9::Instruction called out of order. After end token there should be no more instructions.  Call ::End next.");
      } else if (m_state == D3D9ShaderValidatorState::Error) {
        return E_FAIL;
      }

      if (pdwInst == nullptr || !cdw) {
        return ErrorCallback(pFile, Line, 0,
          D3D9ShaderValidatorMessage::InstructionNullArgs,
          "IDirect3DShaderValidator9::Instruction called with NULL == pdwInst or 0 == cdw.");
      }

      DxsoReader reader = { reinterpret_cast<const char*>(pdwInst) };

      if (m_state == D3D9ShaderValidatorState::ValidatingHeader)
        return ValidateHeader(pFile, Line, pdwInst, cdw);

      if (!m_ctx->decodeInstruction(DxsoCodeIter{ reinterpret_cast<const uint32_t*>(pdwInst) }))
        return ValidateEndToken(pFile, Line, pdwInst, cdw);

      // + 1 to account for the opcode...
      uint32_t dwordLength = m_ctx->getInstructionContext().instruction.tokenLength + 1;
      if (cdw != dwordLength) {
        return ErrorCallback(pFile, Line, 0x2,
          D3D9ShaderValidatorMessage::BadInstructionLength,
          str::format("Instruction length specified for instruction (", cdw, ") does not match the token count encountered (", dwordLength, "). Aborting validation."));
      }

      return D3D_OK;
    }


    HRESULT STDMETHODCALLTYPE End() {
      if (m_state == D3D9ShaderValidatorState::Error) {
        return E_FAIL;
      } else if (m_state == D3D9ShaderValidatorState::Begin) {
        return ErrorCallback(nullptr, 0, 0,
          D3D9ShaderValidatorMessage::EndOutOfOrder,
          "IDirect3DShaderValidator9::End called out of order. Call to ::Begin, followed by calls to ::Instruction must occur first.");
      } else if (m_state != D3D9ShaderValidatorState::EndOfShader) {
        return ErrorCallback(nullptr, 0, 0,
          D3D9ShaderValidatorMessage::MissingEndToken,
          "Shader missing end token.");
      }

      m_state    = D3D9ShaderValidatorState::Begin;
      m_callback = nullptr;
      m_userData = nullptr;
      m_ctx      = nullptr;

      return D3D_OK;
    }

  private:

    HRESULT ValidateHeader(const char* pFile, UINT Line, const DWORD* pdwInst, DWORD cdw) {
      if (cdw != 1) {
        return ErrorCallback(pFile, Line, 0x6,
          D3D9ShaderValidatorMessage::BadVersionTokenLength,
          "Bad version token.  DWORD count > 1 given. Expected DWORD count to be 1 for version token.");
      }

      DxsoReader reader = { reinterpret_cast<const char*>(pdwInst) };

      uint32_t headerToken = reader.readu32();
      uint32_t shaderType  = headerToken & 0xffff0000;

      DxsoProgramType programType;
      if (shaderType == 0xffff0000) // Pixel Shader
        programType = DxsoProgramTypes::PixelShader;
      else if (shaderType == 0xfffe0000) // Vertex Shader
        programType = DxsoProgramTypes::VertexShader;
      else {
        return ErrorCallback(pFile, Line, 0x6,
          D3D9ShaderValidatorMessage::BadVersionTokenType,
          "Bad version token.  It indicates neither a pixel shader nor a vertex shader.");
      }

      const uint32_t majorVersion = (headerToken >> 8) & 0xff;
      const uint32_t minorVersion = headerToken & 0xff;

      m_ctx   = std::make_unique<DxsoDecodeContext>(DxsoProgramInfo{ programType, minorVersion, majorVersion });
      m_state = D3D9ShaderValidatorState::ValidatingInstructions;
      return D3D_OK;
    }

    HRESULT ValidateEndToken(const char* pFile, UINT Line, const DWORD* pdwInst, DWORD cdw) {
      // Reached the end token.
      if (cdw != 1) {
        return ErrorCallback(pFile, Line, 0x6,
          D3D9ShaderValidatorMessage::BadEndToken,
          "Bad end token.  DWORD count > 1 given. Expected DWORD count to be 1 for end token.");
      }

      m_state = D3D9ShaderValidatorState::EndOfShader;
      return D3D_OK;
    }

    HRESULT ErrorCallback(
      const char*                      pFile,
            UINT                       Line,
            DWORD_PTR                  Unknown,
            D3D9ShaderValidatorMessage MessageID,
      const std::string&               Message) {
      if (m_callback)
        m_callback(pFile, Line, Unknown, MessageID, Message.c_str(), m_userData);

      m_state = D3D9ShaderValidatorState::Error;
      return E_FAIL;
    }

    D3D9ShaderValidatorState    m_state    = D3D9ShaderValidatorState::Begin;
    D3D9ShaderValidatorCallback m_callback = nullptr;
    void*                       m_userData = nullptr;

    std::unique_ptr<DxsoDecodeContext> m_ctx;

  };

}