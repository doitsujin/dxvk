#include "d3d8_device.h"

#include "d3d8_interface.h"

#ifdef MSC_VER
#pragma fenv_access (on)
#endif

#define DXVK_D3D8_SHADER_BIT 0x80000000

namespace dxvk {

  // v0-v16, used by fixed function vs
  static constexpr int D3D8_NUM_VERTEX_INPUT_REGISTERS = 17;

  // standard mapping of vertx input v0-v16 to d3d9 usages and usage indices
  // (See D3DVSDE_ values in d3d8types.h or DirectX 8 docs for vertex shader input registers vn)
  static const BYTE D3D8_VERTEX_INPUT_REGISTERS[D3D8_NUM_VERTEX_INPUT_REGISTERS][2] = {
    {d3d9::D3DDECLUSAGE_POSITION, 0},      // dcl_position     v0
    {d3d9::D3DDECLUSAGE_BLENDWEIGHT, 0},   // dcl_blendweight  v1
    {d3d9::D3DDECLUSAGE_BLENDINDICES, 0},  // dcl_blendindices v2
    {d3d9::D3DDECLUSAGE_NORMAL, 0},        // dcl_normal       v3
    {d3d9::D3DDECLUSAGE_PSIZE, 0},         // dcl_psize        v4
    {d3d9::D3DDECLUSAGE_COLOR, 0},         // dcl_color        v5 ; diffuse
    {d3d9::D3DDECLUSAGE_COLOR, 1},         // dcl_color1       v6 ; specular
    {d3d9::D3DDECLUSAGE_TEXCOORD, 0},      // dcl_texcoord0    v7
    {d3d9::D3DDECLUSAGE_TEXCOORD, 1},      // dcl_texcoord1    v8
    {d3d9::D3DDECLUSAGE_TEXCOORD, 2},      // dcl_texcoord2    v9
    {d3d9::D3DDECLUSAGE_TEXCOORD, 3},      // dcl_texcoord3    v10
    {d3d9::D3DDECLUSAGE_TEXCOORD, 4},      // dcl_texcoord4    v11
    {d3d9::D3DDECLUSAGE_TEXCOORD, 5},      // dcl_texcoord5    v12
    {d3d9::D3DDECLUSAGE_TEXCOORD, 6},      // dcl_texcoord6    v13
    {d3d9::D3DDECLUSAGE_TEXCOORD, 7},      // dcl_texcoord7    v14
    {d3d9::D3DDECLUSAGE_POSITION, 1},      // dcl_position1    v15 ; position 2
    {d3d9::D3DDECLUSAGE_NORMAL, 1},        // dcl_normal1      v16 ; normal 2
  };
  
  // width in bytes of each D3DDECLTYPE (dx9) or D3DVSDT (dx8)
  static const BYTE D3D9_DECL_TYPE_SIZES[d3d9::MAXD3DDECLTYPE + 1] = {
    4,  // FLOAT1
    8,  // FLOAT2
    12, // FLOAT3
    16, // FLOAT4
    4,  // D3DCOLOR

    4,  // UBYTE4
    4,  // SHORT2
    8,  // SHORT4

    // The following are for vs2.0+ //
    4,  // UBYTE4N
    4,  // SHORT2N
    8,  // SHORT4N
    4,  // USHORT2N
    8,  // USHORT4N
    6,  // UDEC3
    6,  // DEC3N
    8,  // FLOAT16_2
    16, // FLOAT16_4

    0   // UNUSED
  };

  struct D3D8VertexShaderInfo {
    // Vertex Shader
    d3d9::IDirect3DVertexDeclaration9*  pVertexDecl;
    d3d9::IDirect3DVertexShader9*       pVertexShader;
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

    m_textures.fill(nullptr);

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

#define VSD_SHIFT_MASK(token, field) ( (token & field ## MASK) >> field ## SHIFT )

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::CreateVertexShader(
        const DWORD* pDeclaration,
        const DWORD* pFunction,
              DWORD* pHandle,
              DWORD Usage ) {

    using d3d9::D3DVERTEXELEMENT9;
    using d3d9::D3DDECLTYPE;

    D3D8VertexShaderInfo& info = m_vertexShaders.emplace_back();

    std::vector<DWORD> tokens;

    // shaderInputRegisters:
    // set bit N to enable input register vN
    DWORD shaderInputRegisters = 0;
    
    D3DVERTEXELEMENT9 vertexElements[MAXD3DDECLLENGTH + 1];
    unsigned int elementIdx = 0;

    // these are used for pDeclaration and pFunction
    int i = 0;
    DWORD token;

    std::stringstream dbg;
    dbg << "Vertex Declaration Tokens:\n\t";

    WORD currentStream = 0;
    WORD currentOffset = 0;

    // remap d3d8 tokens to d3d9 vertex elements
    // and enable specific shaderInputRegisters for each
    do {
      token = pDeclaration[i++];

      D3DVSD_TOKENTYPE tokenType = D3DVSD_TOKENTYPE(VSD_SHIFT_MASK(token, D3DVSD_TOKENTYPE));

      switch ( tokenType ) {
        case D3DVSD_TOKEN_NOP:
          dbg << "NOP";
          break;
        case D3DVSD_TOKEN_STREAM: {
          dbg << "STREAM ";

          // TODO: D3DVSD_STREAMTESSS

          DWORD streamNum = VSD_SHIFT_MASK(token, D3DVSD_STREAMNUMBER);

          currentStream = streamNum;
          currentOffset = 0; // reset offset

          dbg << streamNum;
          break;
        }
        case D3DVSD_TOKEN_STREAMDATA: {

          dbg << "STREAMDATA ";

          DWORD dataLoadType = VSD_SHIFT_MASK(token, D3DVSD_DATALOADTYPE);

          if ( dataLoadType == 0 ) { // vertex

            vertexElements[elementIdx].Stream = currentStream;
            vertexElements[elementIdx].Offset = currentOffset;

            // Read and set data type
            D3DDECLTYPE dataType  = D3DDECLTYPE(VSD_SHIFT_MASK(token, D3DVSD_DATATYPE));
            vertexElements[elementIdx].Type = dataType; // (D3DVSDT values map directly to D3DDECLTYPE)

            // Increase stream offset
            currentOffset += D3D9_DECL_TYPE_SIZES[dataType];

            vertexElements[elementIdx].Method = d3d9::D3DDECLMETHOD_DEFAULT;

            DWORD dataReg = VSD_SHIFT_MASK(token, D3DVSD_VERTEXREG);

            // Map D3DVSDE register num to Usage and UsageIndex
            vertexElements[elementIdx].Usage = D3D8_VERTEX_INPUT_REGISTERS[dataReg][0];
            vertexElements[elementIdx].UsageIndex = D3D8_VERTEX_INPUT_REGISTERS[dataReg][1];

            // Enable register vn
            shaderInputRegisters |= 1 << dataReg;

            // Finished with this element
            elementIdx++;
            
            dbg << "type=" << dataType << ", register=" << dataReg;
          }
          break;
        }
        case D3DVSD_TOKEN_TESSELLATOR:
          dbg << "TESSELLATOR";
          // TODO: D3DVSD_TOKEN_TESSELLATOR
          break;
        case D3DVSD_TOKEN_CONSTMEM:
          dbg << "CONSTMEM";
          // TODO: D3DVSD_TOKEN_CONSTMEM
          break;
        case D3DVSD_TOKEN_EXT:
          dbg << "EXT";
          // TODO: D3DVSD_TOKEN_EXT
          break;
        case D3DVSD_TOKEN_END: {
          using d3d9::D3DDECLTYPE_UNUSED;

          vertexElements[elementIdx] = D3DDECL_END();

          // Finished with this element
          elementIdx++;

          dbg << "END";
          break;
        }
        default:
          dbg << "UNKNOWN TYPE";
          break;
      }
      dbg << "\n\t";

      //dbg << std::hex << token << " ";

    } while (token != D3DVSD_END());
    Logger::debug(dbg.str());

    // copy first token
    // TODO: ensure first token is always only one dword
    tokens.push_back(pFunction[0]);

    // insert dcl instructions 

    for ( int vn = 0; vn < D3D8_NUM_VERTEX_INPUT_REGISTERS; vn++ ) {

      // if bit N is set then we need to dcl register vN
      if ( ( shaderInputRegisters & ( 1 << vn ) ) != 0 ) {

        Logger::debug(str::format("\tShader Input Regsiter: v", vn));

        DWORD usage = D3D8_VERTEX_INPUT_REGISTERS[vn][0];
        DWORD index = D3D8_VERTEX_INPUT_REGISTERS[vn][1];

        DWORD dclUsage  = (usage << D3DSP_DCL_USAGE_SHIFT) & D3DSP_DCL_USAGE_MASK;           // usage
        dclUsage       |= (index << D3DSP_DCL_USAGEINDEX_SHIFT) & D3DSP_DCL_USAGEINDEX_MASK; // usage index

        tokens.push_back(d3d9::D3DSIO_DCL);   // dcl opcode
        tokens.push_back(0x80000000 | dclUsage); // usage token
        tokens.push_back(0x900F0000 | vn);    // register num
      }
    }

    // copy shader tokens from input

    i = 1; // skip first token (we already copied it)
    do {
      token = pFunction[i++];
      tokens.push_back(token);
      //Logger::debug(str::format(std::hex, token));
    } while ( token != D3DVS_END() );

    GetD3D9()->CreateVertexDeclaration(vertexElements, &(info.pVertexDecl));
    HRESULT res = GetD3D9()->CreateVertexShader(tokens.data(), &(info.pVertexShader));

    // Set bit to indicate this is not a fixed function FVF
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

    return D3D_OK;
  }

} // namespace dxvk