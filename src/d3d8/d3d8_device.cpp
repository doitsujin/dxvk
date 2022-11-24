#include "d3d8_device.h"

#include "d3d8_interface.h"
#include "d3d8_shader.h"

#ifdef MSC_VER
#pragma fenv_access (on)
#endif

namespace dxvk {

  constexpr DWORD isFVF(DWORD Handle) {
    return (Handle & D3DFVF_RESERVED0) == 0;
  }

  constexpr DWORD getShaderHandle(DWORD Index) {
    return (Index << 1) | D3DFVF_RESERVED0;
  }

  constexpr DWORD getShaderIndex(DWORD Handle) {
    if ((Handle & D3DFVF_RESERVED0) != 0) {
      return (Handle & ~(D3DFVF_RESERVED0)) >> 1;
    } else {
      return Handle;
    }
  }

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

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::GetInfo(DWORD DevInfoID, void* pDevInfoStruct, DWORD DevInfoStructSize) {
    Logger::debug(str::format("D3D8DeviceEx::GetInfo: ", DevInfoID));

    if (unlikely(pDevInfoStruct == nullptr || DevInfoStructSize == 0))
      return D3DERR_INVALIDCALL;

    HRESULT res;
    d3d9::IDirect3DQuery9* pQuery = nullptr;
    
    switch (DevInfoID) {
      // pre-DX8 queries
      case 0:
      case D3DDEVINFOID_TEXTUREMANAGER:
      case D3DDEVINFOID_D3DTEXTUREMANAGER:
      case D3DDEVINFOID_TEXTURING:
        return E_FAIL;
      
      case D3DDEVINFOID_VCACHE:
        // Docs say response should be S_FALSE, but we'll let D9VK
        // decide based on the value of supportVCache. D3D8Ex calls this.
        res = GetD3D9()->CreateQuery(d3d9::D3DQUERYTYPE_VCACHE, &pQuery);
        break;
      case D3DDEVINFOID_RESOURCEMANAGER:
        // May not be implemented by D9VK.
        res = GetD3D9()->CreateQuery(d3d9::D3DQUERYTYPE_RESOURCEMANAGER, &pQuery);
        break;
      case D3DDEVINFOID_VERTEXSTATS:
        res = GetD3D9()->CreateQuery(d3d9::D3DQUERYTYPE_VERTEXSTATS, &pQuery);
        break;

      default:
        Logger::warn(str::format("D3D8DeviceEx::GetInfo: Unsupported device info ID: ", DevInfoID));
        return E_FAIL;
    }

    if (unlikely(FAILED(res)))
      goto done;
    
    // Immediately issue the query.
    // D3D9 will begin it automatically before ending.
    res = pQuery->Issue(D3DISSUE_END);
    if (unlikely(FAILED(res))) {
      goto done;
    }

    // TODO: Will immediately issuing the query without doing any API calls
    // actually yield meaingful results? And should we flush or let it mellow?
    res = pQuery->GetData(pDevInfoStruct, DevInfoStructSize, D3DGETDATA_FLUSH);

  done:
    if (pQuery != nullptr)
      pQuery->Release();
    
    if (unlikely(FAILED(res))) {
      if (res == D3DERR_NOTAVAILABLE) // unsupported
        return E_FAIL;
      else // any unknown error
        return S_FALSE;
    }
    return res;
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

  // Render States //

  static constexpr float ZBIAS_SCALE     = -0.000005f;
  static constexpr float ZBIAS_SCALE_INV = 1 / ZBIAS_SCALE;

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    d3d9::D3DRENDERSTATETYPE State9 = (d3d9::D3DRENDERSTATETYPE)State;

    if (unlikely(State == D3DRS_ZBIAS)) {
      State9 = d3d9::D3DRS_DEPTHBIAS;
      Value  = bit::cast<DWORD>(float(Value) * ZBIAS_SCALE);
    }

    return GetD3D9()->SetRenderState(State9, Value);
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {

    if (unlikely(State == D3DRS_ZBIAS)) {
      float bias  = 0;
      HRESULT res = GetD3D9()->GetRenderState(d3d9::D3DRS_DEPTHBIAS, (DWORD*)&bias);
      *pValue     = bit::cast<DWORD>(bias * ZBIAS_SCALE_INV);
      return res;
    }

    return GetD3D9()->GetRenderState((d3d9::D3DRENDERSTATETYPE)State, pValue);
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
    *pHandle = getShaderHandle(m_vertexShaders.size() - 1);

    return res;
  }

  inline D3D8VertexShaderInfo* getVertexShaderInfo(D3D8DeviceEx* device, DWORD Handle) {
    
    Handle = getShaderIndex(Handle);

    if (unlikely(Handle >= device->m_vertexShaders.size())) {
      Logger::err(str::format("getVertexShaderInfo: Invalid vertex shader index ", std::hex, Handle));
      return nullptr;
    }

    D3D8VertexShaderInfo& info = device->m_vertexShaders[Handle];

    if (unlikely(!info.pVertexDecl && !info.pVertexShader)) {
      Logger::err(str::format("getVertexShaderInfo: Application provided deleted vertex shader ", std::hex, Handle));
      return nullptr;
    }

    return &info;
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::SetVertexShader( DWORD Handle ) {

    if (unlikely(ShouldRecord())) {
      return m_recorder->SetVertexShader(Handle);
    }

    // Check for extra bit that indicates this is not an FVF
    if (!isFVF(Handle)) {

      D3D8VertexShaderInfo* info = getVertexShaderInfo(this, Handle);

      if (!info)
        return D3DERR_INVALIDCALL;

      // Cache current shader
      m_currentVertexShader = Handle;
      
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
        *pHandle = getShaderHandle(DWORD(i));
        return res;
      }
    }

    return res;
    */
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::DeleteVertexShader(DWORD Handle) {

    if (!isFVF(Handle)) {

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

    // Still set the shader bit, to prevent conflicts with NULL.
    *pHandle = getShaderHandle(m_pixelShaders.size() - 1);

    return res;
  }

  inline d3d9::IDirect3DPixelShader9* getPixelShaderPtr(D3D8DeviceEx* device, DWORD Handle) {

    Handle = getShaderIndex(Handle);

    if (unlikely(Handle >= device->m_pixelShaders.size())) {
      Logger::err(str::format("getPixelShaderPtr: Invalid pixel shader index ", std::hex, Handle));
      return nullptr;
    }

    d3d9::IDirect3DPixelShader9* pPixelShader = device->m_pixelShaders[Handle];

    if (unlikely(pPixelShader == nullptr)) {
      Logger::err(str::format("getPixelShaderPtr: Application provided deleted pixel shader ", std::hex, Handle));
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
    m_currentPixelShader = Handle;

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

    m_pixelShaders[getShaderIndex(Handle)] = nullptr;

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