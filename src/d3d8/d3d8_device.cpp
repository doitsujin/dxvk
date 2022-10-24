#include "d3d8_device.h"

#include "d3d8_interface.h"
#include "d3d8_shader.h"

#ifdef MSC_VER
#pragma fenv_access (on)
#endif

#define DXVK_D3D8_SHADER_BIT 0x80000000

namespace dxvk {

  struct D3D8VertexShaderInfo {
    // Vertex Shader
    d3d9::IDirect3DVertexDeclaration9*  pVertexDecl   = nullptr;
    d3d9::IDirect3DVertexShader9*       pVertexShader = nullptr;
    std::vector<DWORD>                  declaration;
    std::vector<DWORD>                  function;
  };

  D3D8DeviceEx::D3D8DeviceEx(
    D3D8InterfaceEx*              pParent,
    Com<d3d9::IDirect3DDevice9>&& pDevice,
    //D3D8Adapter*                  pAdapter,
    D3DDEVTYPE                    DeviceType,
    HWND                          hFocusWindow,
    DWORD                         BehaviorFlags,
    D3DPRESENT_PARAMETERS*        pParams)
    : D3D8DeviceBase(std::move(pDevice))
    , m_parent(pParent)
    , m_presentParams(*pParams)
    , m_deviceType(DeviceType)
    , m_window(hFocusWindow)
    , m_behaviorFlags(BehaviorFlags) {

    // Get the bridge interface to D3D9.
    if (FAILED(GetD3D9()->QueryInterface(__uuidof(IDxvkD3D8Bridge), (void**)&m_bridge))) {
      throw DxvkError("D3D8DeviceEx: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_bridge->SetAPIName("D3D8");

    m_textures.fill(nullptr);
    m_streams.fill(D3D8VBO());

    ResetState();
  }


  D3D8DeviceEx::~D3D8DeviceEx() {
  }


  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::QueryInterface(REFIID riid, void** ppvObject) {

    return S_OK;
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)) {
    //  || riid == IID_IDirect3DDevice8) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D8DeviceEx::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::TestCooperativeLevel() {
    // Equivelant of D3D11/DXGI present tests. We can always present.
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::GetDirect3D(IDirect3D8** ppD3D8) {
    if (ppD3D8 == nullptr)
      return D3DERR_INVALIDCALL;

    *ppD3D8 = m_parent.ref();
    return D3D_OK;
  }

  // Vertex Shaders //

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::CreateVertexShader(
        const DWORD* pDeclaration,
        const DWORD* pFunction,
              DWORD* pHandle,
              DWORD Usage ) {


    D3D8VertexShaderInfo& info = m_vertexShaders.emplace_back();

    // Store D3D8 bytecodes in the shader info
    if (pDeclaration != nullptr)
      for (UINT i = 0; pDeclaration[i+1] != D3DVSD_END(); i++)
        info.declaration.push_back(pDeclaration[i]);

    if (pFunction != nullptr)
      for (UINT i = 0; pFunction[i+1] != D3DVS_END(); i++)
        info.function.push_back(pFunction[i]);
    
    D3D9VertexShaderCode result = translateVertexShader8(pDeclaration, pFunction);

    // Create vertex declaration
    HRESULT res = GetD3D9()->CreateVertexDeclaration(result.declaration, &(info.pVertexDecl));
    if (FAILED(res))
      return res;

    if (pFunction != nullptr) {
      res = GetD3D9()->CreateVertexShader(result.function.data(), &(info.pVertexShader));
    } else {
      // pFunction is NULL: fixed function pipeline
      info.pVertexShader = nullptr;
    }

    // Set bit to indicate this is not an FVF
    *pHandle = DWORD(m_vertexShaders.size() - 1) | DXVK_D3D8_SHADER_BIT;

    return res;
  }

  inline D3D8VertexShaderInfo* getVertexShaderInfo(D3D8DeviceEx* device, DWORD Handle) {
    
    // Check for extra bit that indicates this is not an FVF
    if ((Handle & DXVK_D3D8_SHADER_BIT ) != 0) {
      // Remove that bit
      Handle &= ~DXVK_D3D8_SHADER_BIT;
    }

    if (unlikely( Handle >= device->m_vertexShaders.size())) {
      Logger::err(str::format("getVertexShaderInfo: Invalid vertex shader index ", Handle));
      return nullptr;
    }

    D3D8VertexShaderInfo& info = device->m_vertexShaders[Handle];

    if (unlikely(!info.pVertexDecl && !info.pVertexShader)) {
      Logger::err(str::format("getVertexShaderInfo: Application provided deleted vertex shader ", Handle));
      return nullptr;
    }

    return &info;
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::SetVertexShader( DWORD Handle ) {

    if (unlikely(ShouldRecord())) {
      return m_recorder->SetVertexShader(Handle);
    }

    // Check for extra bit that indicates this is not an FVF
    if ( (Handle & DXVK_D3D8_SHADER_BIT ) != 0 ) {

      D3D8VertexShaderInfo* info = getVertexShaderInfo(this, Handle);

      if (!info)
        return D3DERR_INVALIDCALL;

      // Cache current shader
      m_currentVertexShader = Handle | DXVK_D3D8_SHADER_BIT;
      
      GetD3D9()->SetVertexDeclaration(info->pVertexDecl);
      return GetD3D9()->SetVertexShader(info->pVertexShader);

    } else {

      // Cache current FVF
      m_currentVertexShader = Handle;

      //GetD3D9()->SetVertexDeclaration(nullptr);
      GetD3D9()->SetVertexShader(nullptr);
      return GetD3D9()->SetFVF( Handle );
    }
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::GetVertexShader(DWORD* pHandle) {

    // Return cached shader
    *pHandle = m_currentVertexShader;

    return D3D_OK;

    /*
    // Slow path. Use to debug cached shader validation. //
    
    d3d9::IDirect3DVertexShader9* pVertexShader;
    HRESULT res = GetD3D9()->GetVertexShader(&pVertexShader);

    if (FAILED(res) || pVertexShader == nullptr) {
      return GetD3D9()->GetFVF(pHandle);
    }

    for (unsigned int i = 0; i < m_vertexShaders.size(); i++) {
      D3D8VertexShaderInfo& info = m_vertexShaders[i];

      if (info.pVertexShader == pVertexShader) {
        *pHandle = DWORD(i) | DXVK_D3D8_SHADER_BIT;
        return res;
      }
    }

    return res;
    */
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::DeleteVertexShader(DWORD Handle) {

    if ((Handle & DXVK_D3D8_SHADER_BIT) != 0) {

      D3D8VertexShaderInfo* info = getVertexShaderInfo(this, Handle);

      if (!info)
        return D3DERR_INVALIDCALL;

      SAFE_RELEASE(info->pVertexDecl);
      SAFE_RELEASE(info->pVertexShader);

      info->declaration.clear();
      info->function.clear();
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::GetVertexShaderDeclaration(DWORD Handle, void* pData, DWORD* pSizeOfData) {
    D3D8VertexShaderInfo* pInfo = getVertexShaderInfo(this, Handle);

    if (unlikely(!pInfo))
      return D3DERR_INVALIDCALL;
    
    UINT SizeOfData = *pSizeOfData;
    
    // Get actual size
    UINT ActualSize = pInfo->declaration.size() * sizeof(DWORD);
    
    if (pData == nullptr) {
      *pSizeOfData = ActualSize;
      return D3D_OK;
    }

    // D3D8-specific behavior
    if (SizeOfData < ActualSize) {
      *pSizeOfData = ActualSize;
      return D3DERR_MOREDATA;
    }

    memcpy(pData, pInfo->declaration.data(), ActualSize);
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::GetVertexShaderFunction(DWORD Handle, void* pData, DWORD* pSizeOfData) {
    D3D8VertexShaderInfo* pInfo = getVertexShaderInfo(this, Handle);

    if (unlikely(!pInfo))
      return D3DERR_INVALIDCALL;
    
    UINT SizeOfData = *pSizeOfData;
    
    // Get actual size
    UINT ActualSize = pInfo->function.size() * sizeof(DWORD);
    
    if (pData == nullptr) {
      *pSizeOfData = ActualSize;
      return D3D_OK;
    }

    // D3D8-specific behavior
    if (SizeOfData < ActualSize) {
      *pSizeOfData = ActualSize;
      return D3DERR_MOREDATA;
    }

    memcpy(pData, pInfo->function.data(), ActualSize);
    return D3D_OK;

  }

  // Pixel Shaders //

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::CreatePixelShader(
        const DWORD* pFunction,
              DWORD* pHandle) {

    d3d9::IDirect3DPixelShader9* pPixelShader;
    
    HRESULT res = GetD3D9()->CreatePixelShader(pFunction, &pPixelShader);

    m_pixelShaders.push_back(pPixelShader);

    // Still set the shader bit so that SetVertexShader can recognize and reject a pixel shader,
    // and to prevent conflicts with NULL.
    *pHandle = DWORD(m_pixelShaders.size() - 1) | DXVK_D3D8_SHADER_BIT;

    return res;
  }

  inline d3d9::IDirect3DPixelShader9* getPixelShaderPtr(D3D8DeviceEx* device, DWORD Handle) {

    if ( (Handle & DXVK_D3D8_SHADER_BIT) != 0 ) {
      Handle &= ~DXVK_D3D8_SHADER_BIT; // We don't care
    }

    if (unlikely(Handle >= device->m_pixelShaders.size())) {
      Logger::err(str::format("GetPixelShaderPtr: Invalid pixel shader index ", Handle));
      return nullptr;
    }

    d3d9::IDirect3DPixelShader9* pPixelShader = device->m_pixelShaders[Handle];

    if (unlikely(pPixelShader == nullptr)) {
      Logger::err(str::format("SetPixelShader: Application provided deleted pixel shader ", Handle));
      return nullptr;
    }

    return pPixelShader;
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::SetPixelShader(DWORD Handle) {

    if (unlikely(ShouldRecord())) {
      return m_recorder->SetPixelShader(Handle);
    }

    if (Handle == DWORD(NULL)) {
      return GetD3D9()->SetPixelShader(nullptr);
    }

    d3d9::IDirect3DPixelShader9* pPixelShader = getPixelShaderPtr(this, Handle);

    if (unlikely(!pPixelShader)) {
      return D3DERR_INVALIDCALL;
    }

    // Cache current pixel shader
    m_currentPixelShader = Handle | DXVK_D3D8_SHADER_BIT;

    return GetD3D9()->SetPixelShader(pPixelShader);
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::GetPixelShader(DWORD* pHandle) {
    // Return cached shader
    *pHandle = m_currentPixelShader;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::DeletePixelShader(DWORD Handle) {

    d3d9::IDirect3DPixelShader9* pPixelShader = getPixelShaderPtr(this, Handle);

    if (unlikely(!pPixelShader)) {
      return D3DERR_INVALIDCALL;
    }

    SAFE_RELEASE(pPixelShader);

    m_pixelShaders[Handle] = nullptr;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::GetPixelShaderFunction(DWORD Handle, void* pData, DWORD* pSizeOfData) {

    d3d9::IDirect3DPixelShader9* pPixelShader = getPixelShaderPtr(this, Handle);

    if (unlikely(!pPixelShader))
      return D3DERR_INVALIDCALL;

    UINT SizeOfData = *pSizeOfData;
    
    // Get actual size
    UINT ActualSize = 0;
    pPixelShader->GetFunction(nullptr, &ActualSize);
    
    if (pData == nullptr) {
      *pSizeOfData = ActualSize;
      return D3D_OK;
    }

    // D3D8-specific behavior
    if (SizeOfData < ActualSize) {
      *pSizeOfData = ActualSize;
      return D3DERR_MOREDATA;
    }

    return pPixelShader->GetFunction(pData, &SizeOfData);
  }

} // namespace dxvk