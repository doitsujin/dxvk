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
    d3d9::IDirect3DVertexDeclaration9*  pVertexDecl = nullptr;
    d3d9::IDirect3DVertexShader9*       pVertexShader = nullptr;
  };

  D3D8DeviceEx::D3D8DeviceEx(
    D3D8InterfaceEx*              pParent,
    Com<d3d9::IDirect3DDevice9>&& pDevice,
    //D3D8Adapter*                  pAdapter,
    D3DDEVTYPE                    DeviceType,
    HWND                          hFocusWindow,
    DWORD                         BehaviorFlags)
    : D3D8DeviceBase(std::move(pDevice))
    , m_parent(pParent)
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

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::SetVertexShader( DWORD Handle ) {

    if (unlikely(ShouldRecord())) {
      return m_recorder->SetVertexShader(Handle);
    }

    // Check for extra bit that indicates this is not an FVF
    if ( (Handle & DXVK_D3D8_SHADER_BIT ) != 0 ) {
      // Remove that bit
      Handle &= ~DXVK_D3D8_SHADER_BIT;

      if ( unlikely( Handle >= m_vertexShaders.size() ) ) {
        Logger::err(str::format("SetVertexShader: Invalid vertex shader index ", Handle));
        return D3DERR_INVALIDCALL;
      }

      D3D8VertexShaderInfo& info = m_vertexShaders[Handle];

      if ( info.pVertexDecl == nullptr && info.pVertexShader == nullptr ) {
        Logger::err(str::format("SetVertexShader: Application provided deleted vertex shader ", Handle));
        return D3DERR_INVALIDCALL;
      }

      // Cache current shader
      m_currentVertexShader = Handle | DXVK_D3D8_SHADER_BIT;
      
      GetD3D9()->SetVertexDeclaration(info.pVertexDecl);
      return GetD3D9()->SetVertexShader(info.pVertexShader);

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

      Handle &= ~DXVK_D3D8_SHADER_BIT;

      if ( Handle >= m_vertexShaders.size() ) {
        Logger::err(str::format("DeleteVertexShader: Invalid vertex shader index ", Handle));
        return D3DERR_INVALIDCALL;
      }

      D3D8VertexShaderInfo& info = m_vertexShaders[Handle];

      if (info.pVertexDecl == nullptr && info.pVertexShader == nullptr) {
        Logger::err(str::format("DeleteVertexShader: Application provided already deleted vertex shader ", Handle));
        return D3DERR_INVALIDCALL;
      }

      SAFE_RELEASE(info.pVertexDecl);
      SAFE_RELEASE(info.pVertexShader);
    }

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

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::SetPixelShader(DWORD Handle) {

    if (unlikely(ShouldRecord())) {
      return m_recorder->SetPixelShader(Handle);
    }

    if (Handle == NULL) {
      return GetD3D9()->SetPixelShader(nullptr);
    }

    if ( (Handle & DXVK_D3D8_SHADER_BIT) != 0 ) {
      Handle &= ~DXVK_D3D8_SHADER_BIT; // We don't care
    }

    if (unlikely(Handle >= m_pixelShaders.size())) {
      Logger::err(str::format("SetPixelShader: Invalid pixel shader index ", Handle));
      return D3DERR_INVALIDCALL;
    }

    d3d9::IDirect3DPixelShader9* pPixelShader = m_pixelShaders[Handle];

    if (unlikely(pPixelShader == nullptr)) {
      Logger::err(str::format("SetPixelShader: Application provided deleted pixel shader ", Handle));
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

    if ( (Handle & DXVK_D3D8_SHADER_BIT) != 0 ) {
      Handle &= ~DXVK_D3D8_SHADER_BIT; // We don't care
    }

    if (Handle >= m_pixelShaders.size()) {
      Logger::err(str::format("DeletePixelShader: Invalid pixel shader index ", Handle));
      return D3DERR_INVALIDCALL;
    }

    d3d9::IDirect3DPixelShader9* pPixelShader = m_pixelShaders[Handle];

    if (unlikely(pPixelShader == nullptr)) {
      Logger::err(str::format("SetPixelShader: Application provided already deleted pixel shader ", Handle));
      return D3DERR_INVALIDCALL;
    }

    SAFE_RELEASE(pPixelShader);

    m_pixelShaders[Handle] = nullptr;

    return D3D_OK;
  }

} // namespace dxvk