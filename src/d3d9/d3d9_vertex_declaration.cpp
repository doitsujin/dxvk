#include "d3d9_vertex_declaration.h"
#include "d3d9_util.h"

#include <algorithm>
#include <cstring>

namespace dxvk {

  D3D9VertexDecl::D3D9VertexDecl(
          D3D9DeviceEx*      pDevice,
          DWORD              FVF)
    : D3D9VertexDeclBase(pDevice) {
    this->SetFVF(FVF);
    this->Classify();
  }


  D3D9VertexDecl::D3D9VertexDecl(
          D3D9DeviceEx*      pDevice,
    const D3DVERTEXELEMENT9* pVertexElements,
          uint32_t           DeclCount)
    : D3D9VertexDeclBase( pDevice )
    , m_elements        ( pVertexElements, pVertexElements + DeclCount )
    , m_fvf             ( this->MapD3D9VertexElementsToFvf() ) {
    this->Classify();
  }


  HRESULT STDMETHODCALLTYPE D3D9VertexDecl::QueryInterface(
          REFIID  riid,
          void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DVertexDeclaration9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDirect3DVertexDeclaration9), riid)) {
      Logger::warn("D3D9VertexDecl::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D9VertexDecl::GetDeclaration(
          D3DVERTEXELEMENT9* pElement,
          UINT*              pNumElements) {
    if (pNumElements == nullptr)
      return D3DERR_INVALIDCALL;

    *pNumElements = UINT(m_elements.size()) + 1u; // Account for D3DDECL_END

    if (pElement == nullptr)
      return D3D_OK;

    // The native runtime ignores pNumElements here...
    std::copy(m_elements.begin(), m_elements.end(), pElement);
    pElement[m_elements.size()] = D3DDECL_END();

    return D3D_OK;
  }


  void D3D9VertexDecl::SetFVF(DWORD FVF) {
    m_fvf = FVF;

    std::array<D3DVERTEXELEMENT9, 16> elements;
    uint32_t elemCount = 0;
    uint32_t texCount = 0;

    uint32_t betas = 0;
    uint8_t betaIdx = 0xFF;

    switch (FVF & D3DFVF_POSITION_MASK) {
      case D3DFVF_XYZ:
      case D3DFVF_XYZB1:
      case D3DFVF_XYZB2:
      case D3DFVF_XYZB3:
      case D3DFVF_XYZB4:
      case D3DFVF_XYZB5:
        elements[elemCount].Type = D3DDECLTYPE_FLOAT3;
        elements[elemCount].Usage = D3DDECLUSAGE_POSITION;
        elements[elemCount].UsageIndex = 0;
        elemCount++;

        if ((FVF & D3DFVF_POSITION_MASK) == D3DFVF_XYZ)
          break;

        betas = (((FVF & D3DFVF_XYZB5) - D3DFVF_XYZB1) >> 1) + 1;
        if (FVF & D3DFVF_LASTBETA_D3DCOLOR)
          betaIdx = D3DDECLTYPE_D3DCOLOR;
        else if (FVF & D3DFVF_LASTBETA_UBYTE4)
          betaIdx = D3DDECLTYPE_UBYTE4;
        else if ((FVF & D3DFVF_XYZB5) == D3DFVF_XYZB5)
          betaIdx = D3DDECLTYPE_FLOAT1;

        if (betaIdx != 0xFF)
          betas--;

        if (betas > 0) {
          switch (betas) {
            case 1: elements[elemCount].Type = D3DDECLTYPE_FLOAT1; break;
            case 2: elements[elemCount].Type = D3DDECLTYPE_FLOAT2; break;
            case 3: elements[elemCount].Type = D3DDECLTYPE_FLOAT3; break;
            case 4: elements[elemCount].Type = D3DDECLTYPE_FLOAT4; break;
            default: break;
          }
          elements[elemCount].Usage = D3DDECLUSAGE_BLENDWEIGHT;
          elements[elemCount].UsageIndex = 0;
          elemCount++;
        }

        if (betaIdx != 0xFF) {
          elements[elemCount].Type = betaIdx;
          elements[elemCount].Usage = D3DDECLUSAGE_BLENDINDICES;
          elements[elemCount].UsageIndex = 0;
          elemCount++;
        }
        break;

      case D3DFVF_XYZW:
      case D3DFVF_XYZRHW:
        elements[elemCount].Type = D3DDECLTYPE_FLOAT4;
        elements[elemCount].Usage =
          ((FVF & D3DFVF_POSITION_MASK) == D3DFVF_XYZW)
          ? D3DDECLUSAGE_POSITION
          : D3DDECLUSAGE_POSITIONT;
        elements[elemCount].UsageIndex = 0;
        elemCount++;
        break;

      default:
        break;
    }

    if (FVF & D3DFVF_NORMAL) {
      elements[elemCount].Type = D3DDECLTYPE_FLOAT3;
      elements[elemCount].Usage = D3DDECLUSAGE_NORMAL;
      elements[elemCount].UsageIndex = 0;
      elemCount++;
    }
    if (FVF & D3DFVF_PSIZE) {
      elements[elemCount].Type = D3DDECLTYPE_FLOAT1;
      elements[elemCount].Usage = D3DDECLUSAGE_PSIZE;
      elements[elemCount].UsageIndex = 0;
      elemCount++;
    }
    if (FVF & D3DFVF_DIFFUSE) {
      elements[elemCount].Type = D3DDECLTYPE_D3DCOLOR;
      elements[elemCount].Usage = D3DDECLUSAGE_COLOR;
      elements[elemCount].UsageIndex = 0;
      elemCount++;
    }
    if (FVF & D3DFVF_SPECULAR) {
      elements[elemCount].Type = D3DDECLTYPE_D3DCOLOR;
      elements[elemCount].Usage = D3DDECLUSAGE_COLOR;
      elements[elemCount].UsageIndex = 1;
      elemCount++;
    }

    texCount = (FVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    texCount = std::min(texCount, 8u);

    for (uint32_t i = 0; i < texCount; i++) {
      switch ((FVF >> (16 + i * 2)) & 0x3) {
        case D3DFVF_TEXTUREFORMAT1:
          elements[elemCount].Type = D3DDECLTYPE_FLOAT1;
          break;

        case D3DFVF_TEXTUREFORMAT2:
          elements[elemCount].Type = D3DDECLTYPE_FLOAT2;
          break;

        case D3DFVF_TEXTUREFORMAT3:
          elements[elemCount].Type = D3DDECLTYPE_FLOAT3;
          break;

        case D3DFVF_TEXTUREFORMAT4:
          elements[elemCount].Type = D3DDECLTYPE_FLOAT4;
          break;

        default:
          break;
      }
      elements[elemCount].Usage = D3DDECLUSAGE_TEXCOORD;
      elements[elemCount].UsageIndex = i;
      elemCount++;
    }

    for (uint32_t i = 0; i < elemCount; i++) {
      elements[i].Stream = 0;
      elements[i].Offset = (i == 0)
        ? 0
        : (elements[i - 1].Offset + GetDecltypeSize(D3DDECLTYPE(elements[i - 1].Type)));

      elements[i].Method = D3DDECLMETHOD_DEFAULT;
    }

    m_elements.resize(elemCount);
    std::copy(elements.begin(), elements.begin() + elemCount, m_elements.data());
  }

  bool D3D9VertexDecl::MapD3DDeclToFvf(
    const D3DVERTEXELEMENT9& element,
          DWORD fvf,
          DWORD& outFvf,
          DWORD& texCountPostUpdate) {

    // Mapping between a Direct3D Declaration and FVF Codes (Direct3D 9)
    // This table maps members of a D3DVERTEXELEMENT9 declaration to a FVF code.
    //
    // Data type              Usage                       Usage index     FVF
    // ----------------------------------------------------------------------------------------
    // D3DDECLTYPE_FLOAT3     D3DDECLUSAGE_POSITION       0               D3DFVF_XYZ
    // D3DDECLTYPE_FLOAT4     D3DDECLUSAGE_POSITIONT      0               D3DFVF_XYZRHW
    // D3DDECLTYPE_FLOATn     D3DDECLUSAGE_BLENDWEIGHT    0               D3DFVF_XYZBn
    // D3DDECLTYPE_UBYTE4     D3DDECLUSAGE_BLENDINDICES   0               D3DFVF_XYZB(nWeights + 1)
    // D3DDECLTYPE_FLOAT3     D3DDECLUSAGE_NORMAL         0               D3DFVF_NORMAL
    // D3DDECLTYPE_FLOAT1     D3DDECLUSAGE_PSIZE          0               D3DFVF_PSIZE
    // D3DDECLTYPE_D3DCOLOR   D3DDECLUSAGE_COLOR          0               D3DFVF_DIFFUSE
    // D3DDECLTYPE_D3DCOLOR   D3DDECLUSAGE_COLOR          1               D3DFVF_SPECULAR
    // D3DDECLTYPE_FLOATm     D3DDECLUSAGE_TEXCOORD       n               D3DFVF_TEXCOORDSIZEm(n)
    // D3DDECLTYPE_FLOAT3     D3DDECLUSAGE_POSITION       1               N / A
    // D3DDECLTYPE_FLOAT3     D3DDECLUSAGE_NORMAL         1               N / A


    if (element.Usage == D3DDECLUSAGE_POSITION && element.Type == D3DDECLTYPE_FLOAT3 && element.UsageIndex == 0) {
      outFvf = D3DFVF_XYZ;
      return true;
    }

    if (element.Usage == D3DDECLUSAGE_POSITIONT && element.Type == D3DDECLTYPE_FLOAT4 && element.UsageIndex == 0) {
      outFvf = D3DFVF_XYZRHW;
      return true;
    }

    if (element.Usage == D3DDECLUSAGE_BLENDWEIGHT && element.UsageIndex == 0) {
      DWORD fvfRet = MapD3DDeclTypeFloatToFvfXYZBn(element.Type);
      if (likely(fvfRet != 0)) {
        outFvf = fvfRet;
        return true;
      } else {
        return false;
      }
    }

    if (element.Usage == D3DDECLUSAGE_BLENDINDICES && element.Type == D3DDECLTYPE_UBYTE4 && element.UsageIndex == 0) {
      outFvf = D3DFVF_XYZB1;
      return true;
    }

    if (element.Usage == D3DDECLUSAGE_NORMAL && element.Type == D3DDECLTYPE_FLOAT3 && element.UsageIndex == 0) {
      outFvf = D3DFVF_NORMAL;
      return true;
    }

    if (element.Usage == D3DDECLUSAGE_PSIZE && element.Type == D3DDECLTYPE_FLOAT1 && element.UsageIndex == 0) {
      outFvf = D3DFVF_PSIZE;
      return true;
    }

    if (element.Usage == D3DDECLUSAGE_COLOR && element.Type == D3DDECLTYPE_D3DCOLOR) {
      switch (element.UsageIndex) {
        case 0:
          outFvf = D3DFVF_DIFFUSE;
          return true;
        case 1:
          outFvf = D3DFVF_SPECULAR;
          return true;
        default:
          return false;
      }
    }

    if (element.Usage == D3DDECLUSAGE_TEXCOORD && element.UsageIndex < 8) {
      return MapD3DDeclUsageTexCoordToFvfTexCoordSize(element, fvf, outFvf, texCountPostUpdate);
    }

    return false;
  }


  DWORD D3D9VertexDecl::MapD3DDeclTypeFloatToFvfXYZBn(BYTE type) {

    switch (type) {
      case D3DDECLTYPE_FLOAT1: return D3DFVF_XYZB1;
      case D3DDECLTYPE_FLOAT2: return D3DFVF_XYZB2;
      case D3DDECLTYPE_FLOAT3: return D3DFVF_XYZB3;
      case D3DDECLTYPE_FLOAT4: return D3DFVF_XYZB4;
      default:                 return 0;
    }
  }


  bool D3D9VertexDecl::MapD3DDeclUsageTexCoordToFvfTexCoordSize(
      const D3DVERTEXELEMENT9& element,
      DWORD fvf,
      DWORD& outFvf,
      DWORD& texCountPostUpdate) {

    // Check if bits of format for current UsageIndex are free in the fvf
    // It is necessary to skip multiple initializations of the bitfield because
    // returned value is bitwise or-ed to final fvf DWORD.
    // The D3DFVF_TEXCOORDSIZE1 is used below because it covers all formats bits.
    if ((D3DFVF_TEXCOORDSIZE1(element.UsageIndex) & fvf) != 0)
      return false;

    // Update max texture's index in fvf
    DWORD currentTexCount = element.UsageIndex + 1;
    bool retStatus = true;

    if (texCountPostUpdate < currentTexCount)
      texCountPostUpdate = currentTexCount;

    if (element.Type == D3DDECLTYPE_FLOAT1)
      outFvf = D3DFVF_TEXCOORDSIZE1(element.UsageIndex);
    else if (element.Type == D3DDECLTYPE_FLOAT2)
      outFvf = D3DFVF_TEXCOORDSIZE2(element.UsageIndex);
    else if (element.Type == D3DDECLTYPE_FLOAT3)
      outFvf = D3DFVF_TEXCOORDSIZE3(element.UsageIndex);
    else if (element.Type == D3DDECLTYPE_FLOAT4)
      outFvf = D3DFVF_TEXCOORDSIZE4(element.UsageIndex);
    else
      retStatus = false;

    return retStatus;
  }


  DWORD D3D9VertexDecl::MapD3D9VertexElementsToFvf() {
    DWORD fvf = 0;
    DWORD texCountPostUpdate = 0;

    for (const auto& element : m_elements) {
      DWORD elementFvf = 0;
      if (!MapD3DDeclToFvf(element, fvf, elementFvf, texCountPostUpdate)) {
        return 0;
      }
      fvf |= elementFvf;
    }

    fvf |= (texCountPostUpdate << 8);

    return fvf;
  }


  void D3D9VertexDecl::Classify() {
    for (const auto& element : m_elements) {
      if (element.Type != D3DDECLTYPE_UNUSED)
        m_sizes[element.Stream] = std::max(m_sizes[element.Stream], element.Offset + GetDecltypeSize(D3DDECLTYPE(element.Type)));

      if (element.Usage == D3DDECLUSAGE_COLOR && element.UsageIndex == 0)
        m_flags.set(D3D9VertexDeclFlag::HasColor0);
      else if (element.Usage == D3DDECLUSAGE_COLOR && element.UsageIndex == 1)
        m_flags.set(D3D9VertexDeclFlag::HasColor1);
      else if (element.Usage == D3DDECLUSAGE_POSITIONT)
        m_flags.set(D3D9VertexDeclFlag::HasPositionT);
      else if (element.Usage == D3DDECLUSAGE_PSIZE)
        m_flags.set(D3D9VertexDeclFlag::HasPointSize);
      else if (element.Usage == D3DDECLUSAGE_FOG)
        m_flags.set(D3D9VertexDeclFlag::HasFog);
      else if (element.Usage == D3DDECLUSAGE_BLENDWEIGHT)
        m_flags.set(D3D9VertexDeclFlag::HasBlendWeight);
      else if (element.Usage == D3DDECLUSAGE_BLENDINDICES)
        m_flags.set(D3D9VertexDeclFlag::HasBlendIndices);

      if (element.Usage == D3DDECLUSAGE_TEXCOORD)
        m_texcoordMask |= GetDecltypeCount(D3DDECLTYPE(element.Type)) << (element.UsageIndex * 3);

      m_streamMask |= 1 << element.Stream;
    }
  }

}