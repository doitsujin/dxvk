#include "d3d8_device.h"

#include "d3d8_interface.h"

#ifdef MSC_VER
#pragma fenv_access (on)
#endif

#define DXVK_D3D8_SHADER_BIT 0x80000000

namespace dxvk {

  struct D3D8ShaderInfo {

    d3d9::IDirect3DVertexDeclaration9*  pDecl;
    d3d9::IDirect3DVertexShader9*       pShader;
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

#define VSD_SHIFT_MASK(token, field) ( (token & field ## MASK) >> field ## SHIFT )

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::CreateVertexShader(
        const DWORD* pDeclaration,
        const DWORD* pFunction,
              DWORD* pHandle,
              DWORD Usage ) {

    using d3d9::D3DVERTEXELEMENT9;
    using d3d9::D3DDECLTYPE;

    D3D8ShaderInfo& info = m_shaders.emplace_back();
    
    D3DVERTEXELEMENT9 vertexElements[MAXD3DDECLLENGTH + 1];
    unsigned int elementIdx = 0;

    int i = 0;
    DWORD token;
    std::stringstream dbg;
    dbg << "Vertex Declaration Tokens:\n\t";

    WORD currentStream = 0;
    WORD currentOffset = 0;

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
            vertexElements[elementIdx].Type = dataType;

            vertexElements[elementIdx].Method = d3d9::D3DDECLMETHOD_DEFAULT;

            // default usage index
            vertexElements[elementIdx].UsageIndex = 0;

            DWORD dataReg   = VSD_SHIFT_MASK(token, D3DVSD_VERTEXREG);
            switch ( dataReg ) {
              case D3DVSDE_POSITION:
                vertexElements[elementIdx].Usage = d3d9::D3DDECLUSAGE_POSITION;
                break;
              case D3DVSDE_BLENDWEIGHT:
                vertexElements[elementIdx].Usage = d3d9::D3DDECLUSAGE_BLENDWEIGHT;
                break;
              case D3DVSDE_BLENDINDICES:
                vertexElements[elementIdx].Usage = d3d9::D3DDECLUSAGE_BLENDINDICES;
                break;
              case D3DVSDE_NORMAL:
                vertexElements[elementIdx].Usage = d3d9::D3DDECLUSAGE_NORMAL;
                break;
              case D3DVSDE_PSIZE:
                vertexElements[elementIdx].Usage = d3d9::D3DDECLUSAGE_PSIZE;
                break;

              case D3DVSDE_DIFFUSE:   // Color 0
                vertexElements[elementIdx].Usage = d3d9::D3DDECLUSAGE_COLOR;
                break;
              case D3DVSDE_SPECULAR:  // Color 1
                vertexElements[elementIdx].Usage = d3d9::D3DDECLUSAGE_COLOR;
                vertexElements[elementIdx].UsageIndex = 1;
                break;

              // Texcoords
              case D3DVSDE_TEXCOORD0:
              case D3DVSDE_TEXCOORD1:
              case D3DVSDE_TEXCOORD2:
              case D3DVSDE_TEXCOORD3:
              case D3DVSDE_TEXCOORD4:
              case D3DVSDE_TEXCOORD5:
              case D3DVSDE_TEXCOORD6:
              case D3DVSDE_TEXCOORD7:
                vertexElements[elementIdx].Usage = d3d9::D3DDECLUSAGE_TEXCOORD;
                vertexElements[elementIdx].UsageIndex = BYTE(dataReg - D3DVSDE_TEXCOORD0); // 0 to 7
                break;

              case D3DVSDE_POSITION2:
                vertexElements[elementIdx].Usage = d3d9::D3DDECLUSAGE_POSITION;
                vertexElements[elementIdx].UsageIndex = 1;
                break;
              case D3DVSDE_NORMAL2:
                vertexElements[elementIdx].Usage = d3d9::D3DDECLUSAGE_NORMAL;
                vertexElements[elementIdx].UsageIndex = 1;
                break;
            }

            // Finished with this element
            elementIdx++;
            
            dbg << "type=" << dataType << ", register=" << dataReg;
            break;
          }
        }
        case D3DVSD_TOKEN_TESSELLATOR:
          dbg << "TESSELLATOR";
          break;
        case D3DVSD_TOKEN_CONSTMEM:
          dbg << "CONSTMEM";
          break;
        case D3DVSD_TOKEN_EXT:
          dbg << "EXT";
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
    Logger::info(dbg.str());

    GetD3D9()->CreateVertexDeclaration(vertexElements, &(info.pDecl));
    HRESULT res = GetD3D9()->CreateVertexShader(pFunction, &(info.pShader));

    // Set bit to indicate this is not a fixed function FVF
    *pHandle = DWORD(m_shaders.size() - 1) | DXVK_D3D8_SHADER_BIT;

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::SetVertexShader( DWORD Handle ) {
    // TODO: determine if Handle is an FVF or a shader ptr
    // (may need to set a bit on ptrs)

    // Check for extra bit that indicates this is not an FVF
    if ( (Handle & DXVK_D3D8_SHADER_BIT ) != 0 ) {
      // Remove that bit
      Handle &= ~DXVK_D3D8_SHADER_BIT;

      if ( Handle >= m_shaders.size() ) {
        Logger::err(str::format("Invalid vertex shader index ", Handle));
        return D3DERR_INVALIDCALL;
      }

      D3D8ShaderInfo& info = m_shaders[Handle];
      
      GetD3D9()->SetVertexDeclaration(info.pDecl);
      return GetD3D9()->SetVertexShader(info.pShader);
    }
    else {
      //GetD3D9()->SetVertexDeclaration(nullptr);
      GetD3D9()->SetVertexShader(nullptr);
      return GetD3D9()->SetFVF( Handle );
    }
  }

} // namespace dxvk