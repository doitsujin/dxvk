#pragma once

#include <unordered_map>
#include <vector>

#include "d3d10_include.h"

#include <d3d10shader.h>
#include <d3d11shader.h>

namespace dxvk {

  class D3D10ShaderReflectionType : public ID3D10ShaderReflectionType {

  public:

    D3D10ShaderReflectionType(
            ID3D11ShaderReflectionType*     d3d11);
    
    ~D3D10ShaderReflectionType();

    HRESULT STDMETHODCALLTYPE GetDesc(
            D3D10_SHADER_TYPE_DESC*         pDesc);
    
    ID3D10ShaderReflectionType* STDMETHODCALLTYPE GetMemberTypeByIndex(
            UINT                            Index);
    
    ID3D10ShaderReflectionType* STDMETHODCALLTYPE GetMemberTypeByName(
      const char*                           Name);
    
    const char* STDMETHODCALLTYPE GetMemberTypeName(
            UINT                            Index);
    
    ID3D11ShaderReflectionType* GetD3D11Iface() {
      return m_d3d11;
    }
  
  private:

    ID3D11ShaderReflectionType* m_d3d11;

    std::unordered_map<
      ID3D11ShaderReflectionType*,
      std::unique_ptr<D3D10ShaderReflectionType>> m_members;

    ID3D10ShaderReflectionType* FindMemberType(
            ID3D11ShaderReflectionType*     pMemberType);

  };


  class D3D10ShaderReflectionVariable : public ID3D10ShaderReflectionVariable {

  public:

    D3D10ShaderReflectionVariable(
            ID3D11ShaderReflectionVariable* d3d11);
    
    ~D3D10ShaderReflectionVariable();

    HRESULT STDMETHODCALLTYPE GetDesc(
            D3D10_SHADER_VARIABLE_DESC*     pDesc);
    
    ID3D10ShaderReflectionType* STDMETHODCALLTYPE GetType();

    ID3D11ShaderReflectionVariable* STDMETHODCALLTYPE GetD3D11Iface() {
      return m_d3d11;
    }

  private:

    ID3D11ShaderReflectionVariable* m_d3d11;
    D3D10ShaderReflectionType       m_type;

  };


  class D3D10ShaderReflectionConstantBuffer : public ID3D10ShaderReflectionConstantBuffer {

  public:

    D3D10ShaderReflectionConstantBuffer(
            ID3D11ShaderReflectionConstantBuffer* d3d11);
    
    ~D3D10ShaderReflectionConstantBuffer();

    HRESULT STDMETHODCALLTYPE GetDesc(
            D3D10_SHADER_BUFFER_DESC*       pDesc);

    ID3D10ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByIndex(
            UINT                            Index);
    
    ID3D10ShaderReflectionVariable* STDMETHODCALLTYPE GetVariableByName(
            LPCSTR                          Name);
    
    ID3D11ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetD3D11Iface() {
      return m_d3d11;
    }
    
  private:

    ID3D11ShaderReflectionConstantBuffer* m_d3d11;

    std::unordered_map<
      ID3D11ShaderReflectionVariable*,
      D3D10ShaderReflectionVariable> m_variables;

    ID3D10ShaderReflectionVariable* FindVariable(
            ID3D11ShaderReflectionVariable* pVariable);

  };


  class D3D10ShaderReflection : public ComObject<ID3D10ShaderReflection> {

  public:

    D3D10ShaderReflection(ID3D11ShaderReflection* d3d11);
    ~D3D10ShaderReflection();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                          riid,
            void**                          ppvObject);
    
    HRESULT STDMETHODCALLTYPE GetDesc(
            D3D10_SHADER_DESC*              pDesc);
    
    ID3D10ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByIndex(
            UINT                            Index);

    ID3D10ShaderReflectionConstantBuffer* STDMETHODCALLTYPE GetConstantBufferByName(
            LPCSTR                          Name);

    HRESULT STDMETHODCALLTYPE GetInputParameterDesc(
            UINT                            ParameterIndex,
            D3D10_SIGNATURE_PARAMETER_DESC* pDesc);

    HRESULT STDMETHODCALLTYPE GetOutputParameterDesc(
            UINT                            ParameterIndex,
            D3D10_SIGNATURE_PARAMETER_DESC* pDesc);

    HRESULT STDMETHODCALLTYPE GetResourceBindingDesc(
            UINT                            ResourceIndex,
            D3D10_SHADER_INPUT_BIND_DESC*   pDesc);
    
  private:

    Com<ID3D11ShaderReflection> m_d3d11;

    std::unordered_map<
      ID3D11ShaderReflectionConstantBuffer*,
      D3D10ShaderReflectionConstantBuffer> m_constantBuffers;
    
    ID3D10ShaderReflectionConstantBuffer* FindConstantBuffer(
            ID3D11ShaderReflectionConstantBuffer* pConstantBuffer);
    
    void ConvertSignatureParameterDesc(
      const D3D11_SIGNATURE_PARAMETER_DESC* pSrcDesc,
            D3D10_SIGNATURE_PARAMETER_DESC* pDstDesc);

  };

}