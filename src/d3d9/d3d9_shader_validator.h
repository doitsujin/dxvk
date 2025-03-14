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
    BadInputRegisterDeclaration = 0x12c,
    BadInputRegister = 0x167,
    BadInstructionLength = 0x21e,
  };

  enum class D3D9ShaderValidatorState {
    Begin,
    ValidatingHeader,
    ValidatingInstructions,
    EndOfShader,
    Error,
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
      const char*  pFile,
            UINT   Line,
      const DWORD* pdwInst,
            DWORD  cdw) = 0;

    virtual HRESULT STDMETHODCALLTYPE End() = 0;

  };

  class D3D9ShaderValidator final : public ComObjectClamp<IDirect3DShaderValidator9> {

  public:

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Begin(
        D3D9ShaderValidatorCallback pCallback,
        void*                       pUserData,
        DWORD                       Unknown);

    HRESULT STDMETHODCALLTYPE Instruction(
        const char*  pFile,
              UINT   Line,
        const DWORD* pdwInst,
              DWORD  cdw);

    HRESULT STDMETHODCALLTYPE End();

  private:

    HRESULT ValidateHeader(const char* pFile, UINT Line, const DWORD* pdwInst, DWORD cdw);

    HRESULT ValidateEndToken(const char* pFile, UINT Line, const DWORD* pdwInst, DWORD cdw);

    HRESULT ErrorCallback(
        const char*                      pFile,
              UINT                       Line,
              DWORD                      Unknown,
        const DWORD*                     pInstr,
              DWORD                      InstrLength,
              D3D9ShaderValidatorMessage MessageID,
        const std::string&               Message);

    bool                        m_isPixelShader = false;
    uint32_t                    m_majorVersion  = 0;
    uint32_t                    m_minorVersion  = 0;

    D3D9ShaderValidatorState    m_state         = D3D9ShaderValidatorState::Begin;
    D3D9ShaderValidatorCallback m_callback      = nullptr;
    void*                       m_userData      = nullptr;

    std::unique_ptr<DxsoDecodeContext> m_ctx;
  };

}