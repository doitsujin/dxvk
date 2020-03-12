#include "d3d10_reflection.h"

namespace dxvk {

  D3D10ShaderReflectionType::D3D10ShaderReflectionType(
          ID3D11ShaderReflectionType*     d3d11)
  : m_d3d11(d3d11) {

  }

  
  D3D10ShaderReflectionType::~D3D10ShaderReflectionType() {

  }


  HRESULT STDMETHODCALLTYPE D3D10ShaderReflectionType::GetDesc(
          D3D10_SHADER_TYPE_DESC*         pDesc) {
    D3D11_SHADER_TYPE_DESC d3d11Desc;
    HRESULT hr = m_d3d11->GetDesc(&d3d11Desc);

    if (FAILED(hr))
      return hr;
    
    pDesc->Class    = D3D10_SHADER_VARIABLE_CLASS(d3d11Desc.Class);
    pDesc->Type     = D3D10_SHADER_VARIABLE_TYPE (d3d11Desc.Type);
    pDesc->Rows     = d3d11Desc.Rows;
    pDesc->Columns  = d3d11Desc.Columns;
    pDesc->Elements = d3d11Desc.Elements;
    pDesc->Members  = d3d11Desc.Members;
    pDesc->Offset   = d3d11Desc.Offset;
    return S_OK;
  }

  
  ID3D10ShaderReflectionType* STDMETHODCALLTYPE D3D10ShaderReflectionType::GetMemberTypeByIndex(
          UINT                            Index) {
    return FindMemberType(m_d3d11->GetMemberTypeByIndex(Index));
  }

  
  ID3D10ShaderReflectionType* STDMETHODCALLTYPE D3D10ShaderReflectionType::GetMemberTypeByName(
    const char*                           Name) {
    return FindMemberType(m_d3d11->GetMemberTypeByName(Name));
  }

  
  const char* STDMETHODCALLTYPE D3D10ShaderReflectionType::GetMemberTypeName(
          UINT                            Index) {
    return m_d3d11->GetMemberTypeName(Index);
  }


  ID3D10ShaderReflectionType* D3D10ShaderReflectionType::FindMemberType(
          ID3D11ShaderReflectionType*     pMemberType) {
    if (!pMemberType)
      return nullptr;

    auto entry = m_members.find(pMemberType);

    if (entry == m_members.end()) {
      entry = m_members.insert({ pMemberType,
        std::make_unique<D3D10ShaderReflectionType>(pMemberType) }).first;
    }

    return entry->second.get();
  }

  
  D3D10ShaderReflectionVariable::D3D10ShaderReflectionVariable(ID3D11ShaderReflectionVariable* d3d11)
  : m_d3d11(d3d11), m_type(m_d3d11->GetType()) {

  }


  D3D10ShaderReflectionVariable::~D3D10ShaderReflectionVariable() {

  }


  HRESULT STDMETHODCALLTYPE D3D10ShaderReflectionVariable::GetDesc(
          D3D10_SHADER_VARIABLE_DESC*     pDesc) {
    D3D11_SHADER_VARIABLE_DESC d3d11Desc;
    HRESULT hr = m_d3d11->GetDesc(&d3d11Desc);

    if (FAILED(hr))
      return hr;

    pDesc->Name         = d3d11Desc.Name;
    pDesc->StartOffset  = d3d11Desc.StartOffset;
    pDesc->Size         = d3d11Desc.Size;
    pDesc->uFlags       = d3d11Desc.uFlags;
    pDesc->DefaultValue = d3d11Desc.DefaultValue;
    return S_OK;
  }

  
  ID3D10ShaderReflectionType* STDMETHODCALLTYPE D3D10ShaderReflectionVariable::GetType() {
    return &m_type;
  }


  D3D10ShaderReflectionConstantBuffer::D3D10ShaderReflectionConstantBuffer(
          ID3D11ShaderReflectionConstantBuffer* d3d11)
  : m_d3d11(d3d11) {

  }

  
  D3D10ShaderReflectionConstantBuffer::~D3D10ShaderReflectionConstantBuffer() {

  }


  HRESULT STDMETHODCALLTYPE D3D10ShaderReflectionConstantBuffer::GetDesc(
          D3D10_SHADER_BUFFER_DESC*       pDesc) {
    D3D11_SHADER_BUFFER_DESC d3d11Desc;
    HRESULT hr = m_d3d11->GetDesc(&d3d11Desc);

    if (FAILED(hr))
      return hr;
    
    pDesc->Name       = d3d11Desc.Name;
    pDesc->Type       = D3D10_CBUFFER_TYPE(d3d11Desc.Type);
    pDesc->Variables  = d3d11Desc.Variables;
    pDesc->Size       = d3d11Desc.Size;
    pDesc->uFlags     = d3d11Desc.uFlags;
    return S_OK;
  }


  ID3D10ShaderReflectionVariable* STDMETHODCALLTYPE D3D10ShaderReflectionConstantBuffer::GetVariableByIndex(
          UINT                            Index) {
    return FindVariable(m_d3d11->GetVariableByIndex(Index));
  }

  
  ID3D10ShaderReflectionVariable* STDMETHODCALLTYPE D3D10ShaderReflectionConstantBuffer::GetVariableByName(
          LPCSTR                          Name) {
    return FindVariable(m_d3d11->GetVariableByName(Name));
  }


  ID3D10ShaderReflectionVariable* D3D10ShaderReflectionConstantBuffer::FindVariable(
          ID3D11ShaderReflectionVariable* pVariable) {
    if (!pVariable)
      return nullptr;

    auto entry = m_variables.find(pVariable);

    if (entry == m_variables.end()) {
      entry = m_variables.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(pVariable),
        std::forward_as_tuple(pVariable)).first;
    }

    return &entry->second;
  }


  D3D10ShaderReflection::D3D10ShaderReflection(ID3D11ShaderReflection* d3d11)
  : m_d3d11(d3d11) {

  }


  D3D10ShaderReflection::~D3D10ShaderReflection() {

  }


  HRESULT STDMETHODCALLTYPE D3D10ShaderReflection::QueryInterface(
          REFIID              riid,
          void**              ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    static const GUID IID_ID3D10ShaderReflection
      = {0xd40e20b6,0xf8f7,0x42ad,{0xab,0x20,0x4b,0xaf,0x8f,0x15,0xdf,0xaa}};

    if (riid == __uuidof(IUnknown)
     || riid == IID_ID3D10ShaderReflection) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  
  HRESULT STDMETHODCALLTYPE D3D10ShaderReflection::GetDesc(
          D3D10_SHADER_DESC*              pDesc) {
    D3D11_SHADER_DESC d3d11Desc;
    HRESULT hr = m_d3d11->GetDesc(&d3d11Desc);

    if (FAILED(hr))
      return hr;
    
    pDesc->Version                     = d3d11Desc.Version;
    pDesc->Creator                     = d3d11Desc.Creator;
    pDesc->Flags                       = d3d11Desc.Flags;
    pDesc->ConstantBuffers             = d3d11Desc.ConstantBuffers;
    pDesc->BoundResources              = d3d11Desc.BoundResources;
    pDesc->InputParameters             = d3d11Desc.InputParameters;
    pDesc->OutputParameters            = d3d11Desc.OutputParameters;
    pDesc->InstructionCount            = d3d11Desc.InstructionCount;
    pDesc->TempRegisterCount           = d3d11Desc.TempRegisterCount;
    pDesc->TempArrayCount              = d3d11Desc.TempArrayCount;
    pDesc->DefCount                    = d3d11Desc.DefCount;
    pDesc->DclCount                    = d3d11Desc.DclCount;
    pDesc->TextureNormalInstructions   = d3d11Desc.TextureNormalInstructions;
    pDesc->TextureLoadInstructions     = d3d11Desc.TextureLoadInstructions;
    pDesc->TextureCompInstructions     = d3d11Desc.TextureCompInstructions;
    pDesc->TextureBiasInstructions     = d3d11Desc.TextureBiasInstructions;
    pDesc->TextureGradientInstructions = d3d11Desc.TextureGradientInstructions;
    pDesc->FloatInstructionCount       = d3d11Desc.FloatInstructionCount;
    pDesc->IntInstructionCount         = d3d11Desc.IntInstructionCount;
    pDesc->UintInstructionCount        = d3d11Desc.UintInstructionCount;
    pDesc->StaticFlowControlCount      = d3d11Desc.StaticFlowControlCount;
    pDesc->DynamicFlowControlCount     = d3d11Desc.DynamicFlowControlCount;
    pDesc->MacroInstructionCount       = d3d11Desc.MacroInstructionCount;
    pDesc->ArrayInstructionCount       = d3d11Desc.ArrayInstructionCount;
    pDesc->CutInstructionCount         = d3d11Desc.CutInstructionCount;
    pDesc->EmitInstructionCount        = d3d11Desc.EmitInstructionCount;
    pDesc->GSOutputTopology            = D3D10_PRIMITIVE_TOPOLOGY(d3d11Desc.GSOutputTopology);
    pDesc->GSMaxOutputVertexCount      = d3d11Desc.GSMaxOutputVertexCount;
    return S_OK;
  }

  
  ID3D10ShaderReflectionConstantBuffer* STDMETHODCALLTYPE
  D3D10ShaderReflection::GetConstantBufferByIndex(
          UINT                            Index) {
    return FindConstantBuffer(m_d3d11->GetConstantBufferByIndex(Index));
  }


  ID3D10ShaderReflectionConstantBuffer* STDMETHODCALLTYPE
  D3D10ShaderReflection::GetConstantBufferByName(
          LPCSTR                          Name) {
    return FindConstantBuffer(m_d3d11->GetConstantBufferByName(Name));
  }


  HRESULT STDMETHODCALLTYPE D3D10ShaderReflection::GetInputParameterDesc(
          UINT                            ParameterIndex,
          D3D10_SIGNATURE_PARAMETER_DESC* pDesc) {
    D3D11_SIGNATURE_PARAMETER_DESC d3d11Desc;
    HRESULT hr = m_d3d11->GetInputParameterDesc(ParameterIndex, &d3d11Desc);

    if (FAILED(hr))
      return hr;
    
    ConvertSignatureParameterDesc(&d3d11Desc, pDesc);
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10ShaderReflection::GetOutputParameterDesc(
          UINT                            ParameterIndex,
          D3D10_SIGNATURE_PARAMETER_DESC* pDesc) {
    D3D11_SIGNATURE_PARAMETER_DESC d3d11Desc;
    HRESULT hr = m_d3d11->GetOutputParameterDesc(ParameterIndex, &d3d11Desc);

    if (FAILED(hr))
      return hr;
    
    ConvertSignatureParameterDesc(&d3d11Desc, pDesc);
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D10ShaderReflection::GetResourceBindingDesc(
          UINT                            ResourceIndex,
          D3D10_SHADER_INPUT_BIND_DESC*   pDesc) {
    D3D11_SHADER_INPUT_BIND_DESC d3d11Desc;
    HRESULT hr = m_d3d11->GetResourceBindingDesc(
      ResourceIndex, &d3d11Desc);
    
    if (FAILED(hr))
      return hr;

    pDesc->Name         = d3d11Desc.Name;
    pDesc->Type         = D3D10_SHADER_INPUT_TYPE(d3d11Desc.Type);
    pDesc->BindPoint    = d3d11Desc.BindPoint;
    pDesc->BindCount    = d3d11Desc.BindCount;
    pDesc->uFlags       = d3d11Desc.uFlags;
    pDesc->ReturnType   = D3D10_RESOURCE_RETURN_TYPE(d3d11Desc.ReturnType);
    pDesc->Dimension    = D3D10_SRV_DIMENSION       (d3d11Desc.Dimension);
    pDesc->NumSamples   = d3d11Desc.NumSamples;
    return S_OK;
  }


  ID3D10ShaderReflectionConstantBuffer* D3D10ShaderReflection::FindConstantBuffer(
          ID3D11ShaderReflectionConstantBuffer* pConstantBuffer) {
    if (!pConstantBuffer)
      return nullptr;

    auto entry = m_constantBuffers.find(pConstantBuffer);

    if (entry == m_constantBuffers.end()) {
      entry = m_constantBuffers.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(pConstantBuffer),
        std::forward_as_tuple(pConstantBuffer)).first;
    }

    return &entry->second;
  }


  void D3D10ShaderReflection::ConvertSignatureParameterDesc(
    const D3D11_SIGNATURE_PARAMETER_DESC* pSrcDesc,
          D3D10_SIGNATURE_PARAMETER_DESC* pDstDesc) {
    pDstDesc->SemanticName        = pSrcDesc->SemanticName;
    pDstDesc->SemanticIndex       = pSrcDesc->SemanticIndex;
    pDstDesc->Register            = pSrcDesc->Register;
    pDstDesc->SystemValueType     = D3D10_NAME(pSrcDesc->SystemValueType);
    pDstDesc->ComponentType       = D3D10_REGISTER_COMPONENT_TYPE(pSrcDesc->ComponentType);
    pDstDesc->Mask                = pSrcDesc->Mask;
    pDstDesc->ReadWriteMask       = pSrcDesc->ReadWriteMask;
  }

}