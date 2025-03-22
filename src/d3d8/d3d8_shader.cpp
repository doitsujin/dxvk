#include "d3d8_shader.h"

#define VSD_SHIFT_MASK(token, field) ((token & field ## MASK) >> field ## SHIFT)
#define VSD_ENCODE(token, field) ((token << field ## _SHIFT) & field ## _MASK)

// Magic number from D3DVSD_SKIP(...)
#define VSD_SKIP_FLAG 0x10000000

// This bit is set on all parameter (non-instruction) tokens.
#define VS_BIT_PARAM 0x80000000

namespace dxvk {

  static constexpr int D3D8_NUM_VERTEX_INPUT_REGISTERS = 17;

  /**
   * Standard mapping of vertex input registers v0-v16 to D3D9 usages and usage indices
   * (See D3DVSDE_REGISTER values in d3d8types.h or DirectX 8 docs for vertex shader input registers vN)
   *
   * \cite https://learn.microsoft.com/en-us/windows/win32/direct3d9/mapping-between-a-directx-9-declaration-and-directx-8
  */
  static constexpr BYTE D3D8_VERTEX_INPUT_REGISTERS[D3D8_NUM_VERTEX_INPUT_REGISTERS][2] = {
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

  /** Width in bytes of each d3d9::D3DDECLTYPE or d3d8 D3DVSDT_TYPE */
  static constexpr BYTE D3D9_DECL_TYPE_SIZES[d3d9::MAXD3DDECLTYPE + 1] = {
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

  /**
   * Encodes a \ref DxsoShaderInstruction
   *
   * \param [in]  opcode  DxsoOpcode
   * \cite https://learn.microsoft.com/en-us/windows-hardware/drivers/display/instruction-token
   */
  constexpr DWORD encodeInstruction(d3d9::D3DSHADER_INSTRUCTION_OPCODE_TYPE opcode) {
    DWORD token   = 0;
    token        |= opcode & 0xFFFF;  // bits 0:15
    return token;
  }

  /**
   * Encodes a \ref DxsoRegister
   *
   * \param [in]  regType  DxsoRegisterType
   * \cite https://learn.microsoft.com/en-us/windows-hardware/drivers/display/destination-parameter-token
   */
  constexpr DWORD encodeDestRegister(d3d9::D3DSHADER_PARAM_REGISTER_TYPE type, UINT reg) {
    DWORD token = 0;
    token    |= reg & 0x7FF;                  // bits 0:10   num
    token    |= ((type & 0x07) << 28);        // bits 28:30  type[0:2]
    token    |= ((type & 0x18) >>  3) << 11;  // bits 11:12  type[3:4]
    // UINT addrMode : 1;                     // bit  13     hasRelative
    token    |= 0b1111 << 16;                 // bits 16:19  DxsoRegMask
    // UINT resultModifier : 3;               // bits 20:23
    // UINT resultShift : 3;                  // bits 24:27
    token    |= 1 << 31;                      // bit  31     always 1
    return token;
  }

  /**
   * Encodes a \ref DxsoDeclaration
   *
   * \cite https://learn.microsoft.com/en-us/windows-hardware/drivers/display/dcl-instruction
   */
  constexpr DWORD encodeDeclaration(d3d9::D3DDECLUSAGE usage, DWORD index) {
    DWORD token = 0;
    token |= VSD_ENCODE(usage, D3DSP_DCL_USAGE);       // bits 0:4   DxsoUsage (TODO: missing MSB)
    token |= VSD_ENCODE(index, D3DSP_DCL_USAGEINDEX);  // bits 16:19 usageIndex
    token |= 1 << 31;                                  // bit 31     always 1
    return token;
  }

  /**
   * Validates and converts a D3D8 vertex shader
   * + declaration to a D3D9 vertex shader + declaration.
  */
  HRESULT TranslateVertexShader8(
      const DWORD*          pDeclaration,
      const DWORD*          pFunction,
      const D3D8Options&    options,
      D3D9VertexShaderCode& pTranslatedVS) {
    using d3d9::D3DDECLTYPE;
    using d3d9::D3DDECLTYPE_UNUSED;

    HRESULT res = D3D_OK;

    std::vector<DWORD>& tokens = pTranslatedVS.function;
    std::vector<DWORD> defs; // Constant definitions

    // shaderInputRegisters:
    // set bit N to enable input register vN
    DWORD shaderInputRegisters = 0;

    d3d9::D3DVERTEXELEMENT9* vertexElements = pTranslatedVS.declaration;
    unsigned int elementIdx = 0;

    // These are used for pDeclaration and pFunction
    int i = 0;
    DWORD token;

    std::stringstream dbg;
    dbg << "D3D8: Vertex Declaration Tokens:\n\t";

    WORD currentStream = 0;
    WORD currentOffset = 0;

    auto addVertexElement = [&] (D3DVSDE_REGISTER reg, D3DVSDT_TYPE type) {
      vertexElements[elementIdx].Stream     = currentStream;
      vertexElements[elementIdx].Offset     = currentOffset;
      vertexElements[elementIdx].Method     = d3d9::D3DDECLMETHOD_DEFAULT;
      vertexElements[elementIdx].Type       = D3DDECLTYPE(type); // (D3DVSDT_TYPE values map directly to D3DDECLTYPE)
      vertexElements[elementIdx].Usage      = D3D8_VERTEX_INPUT_REGISTERS[reg][0];
      vertexElements[elementIdx].UsageIndex = D3D8_VERTEX_INPUT_REGISTERS[reg][1];

      // Increase stream offset
      currentOffset += D3D9_DECL_TYPE_SIZES[type];

      // Enable register vn
      shaderInputRegisters |= 1 << reg;

      // Finished with this element
      elementIdx++;
    };

    // Remap d3d8 decl tokens to d3d9 vertex elements,
    // and enable bits on shaderInputRegisters for each.
    if (options.forceVsDecl.size() == 0) do {
      token = pDeclaration[i++];

      D3DVSD_TOKENTYPE tokenType = D3DVSD_TOKENTYPE(VSD_SHIFT_MASK(token, D3DVSD_TOKENTYPE));

      switch (tokenType) {
        case D3DVSD_TOKEN_NOP:
          dbg << "NOP";
          break;
        case D3DVSD_TOKEN_STREAM: {
          dbg << "STREAM ";

          // TODO: D3DVSD_STREAM_TESS
          if (token & D3DVSD_STREAMTESSMASK) {
            dbg << "TESS";
          }

          DWORD streamNum = VSD_SHIFT_MASK(token, D3DVSD_STREAMNUMBER);

          currentStream = WORD(streamNum);
          currentOffset = 0; // reset offset

          dbg << ", num=" << streamNum;
          break;
        }
        case D3DVSD_TOKEN_STREAMDATA: {

          dbg << "STREAMDATA ";

          // D3DVSD_SKIP
          if (token & VSD_SKIP_FLAG) {
            auto skipCount = VSD_SHIFT_MASK(token, D3DVSD_SKIPCOUNT);
            dbg << "SKIP " << " count=" << skipCount;
            currentOffset += WORD(skipCount) * sizeof(DWORD);
            break;
          }

          // D3DVSD_REG
          DWORD dataLoadType = VSD_SHIFT_MASK(token, D3DVSD_DATALOADTYPE);

          if ( dataLoadType == 0 ) { // vertex
            D3DVSDT_TYPE     type = D3DVSDT_TYPE(VSD_SHIFT_MASK(token, D3DVSD_DATATYPE));
            D3DVSDE_REGISTER reg  = D3DVSDE_REGISTER(VSD_SHIFT_MASK(token, D3DVSD_VERTEXREG));

            // FVF normals are expected to only have 3 components
            if (unlikely(pFunction == nullptr && reg == D3DVSDE_NORMAL && type != D3DVSDT_FLOAT3)) {
              Logger::err("D3D8Device::CreateVertexShader: Invalid FVF declaration: D3DVSDE_NORMAL must use D3DVSDT_FLOAT3");
              return D3DERR_INVALIDCALL;
            }

            addVertexElement(reg, type);

            dbg << "type=" << type << ", register=" << reg;
          } else {
            // TODO: When would this bit be 1?
            dbg << "D3DVSD_DATALOADTYPE " << dataLoadType;
          }
          break;
        }
        case D3DVSD_TOKEN_TESSELLATOR:
          dbg << "TESSELLATOR " << std::hex << token;
          // TODO: D3DVSD_TOKEN_TESSELLATOR
          break;
        case D3DVSD_TOKEN_CONSTMEM: {
          dbg << "CONSTMEM ";
          DWORD count     = VSD_SHIFT_MASK(token, D3DVSD_CONSTCOUNT);
          DWORD regCount  = count * 4;
          DWORD addr      = VSD_SHIFT_MASK(token, D3DVSD_CONSTADDRESS);
          DWORD rs        = VSD_SHIFT_MASK(token, D3DVSD_CONSTRS);

          dbg << "count=" << count << ", addr=" << addr << ", rs=" << rs;

          // Add a DEF instruction for each constant
          for (DWORD j = 0; j < regCount; j += 4) {
            defs.push_back(encodeInstruction(d3d9::D3DSIO_DEF));
            defs.push_back(encodeDestRegister(d3d9::D3DSPR_CONST2, addr));
            defs.push_back(pDeclaration[i+j+0]);
            defs.push_back(pDeclaration[i+j+1]);
            defs.push_back(pDeclaration[i+j+2]);
            defs.push_back(pDeclaration[i+j+3]);
            addr++;
          }
          i += regCount;
          break;
        }
        case D3DVSD_TOKEN_EXT: {
          dbg << "EXT " << std::hex << token << " ";
          DWORD extInfo = VSD_SHIFT_MASK(token, D3DVSD_EXTINFO);
          DWORD extCount = VSD_SHIFT_MASK(token, D3DVSD_EXTCOUNT);
          dbg << "info=" << extInfo << ", count=" << extCount;
          break;
        }
        case D3DVSD_TOKEN_END: {
          vertexElements[elementIdx++] = D3DDECL_END();
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

    // If forceVsDecl is set, use that decl instead.
    if (options.forceVsDecl.size() > 0) {
      for (auto [reg, type] : options.forceVsDecl) {
        addVertexElement(reg, type);
      }
      vertexElements[elementIdx++] = D3DDECL_END();
    }

    if (pFunction != nullptr) {
      // Copy first token (version)
      tokens.push_back(pFunction[0]);

      DWORD vsMajor = D3DSHADER_VERSION_MAJOR(pFunction[0]);
      DWORD vsMinor = D3DSHADER_VERSION_MINOR(pFunction[0]);
      Logger::debug(str::format("VS version: ", vsMajor, ".", vsMinor));

      // Insert dcl instructions
      for (int vn = 0; vn < D3D8_NUM_VERTEX_INPUT_REGISTERS; vn++) {

        // If bit N is set then we need to dcl register vN
        if ((shaderInputRegisters & (1 << vn)) != 0) {

          Logger::debug(str::format("\tShader Input Regsiter: v", vn));

          DWORD usage = D3D8_VERTEX_INPUT_REGISTERS[vn][0];
          DWORD index = D3D8_VERTEX_INPUT_REGISTERS[vn][1];

          tokens.push_back(encodeInstruction(d3d9::D3DSIO_DCL));                  // dcl opcode
          tokens.push_back(encodeDeclaration(d3d9::D3DDECLUSAGE(usage), index));  // usage token
          tokens.push_back(encodeDestRegister(d3d9::D3DSPR_INPUT, vn));           // dest register num
        }
      }

      // Copy constant defs
      for (DWORD def : defs) {
        tokens.push_back(def);
      }

      // Copy shader tokens from input,
      // skip first token (we already copied it)
      i = 1;
      do {
        token = pFunction[i++];

        DWORD opcode = token & D3DSI_OPCODE_MASK;

        // Instructions
        if ((token & VS_BIT_PARAM) == 0) {

          // Swizzle fixup for opcodes that require explicit use of a replicate swizzle.
          if (opcode == D3DSIO_RSQ  || opcode == D3DSIO_RCP
           || opcode == D3DSIO_EXP  || opcode == D3DSIO_LOG
           || opcode == D3DSIO_EXPP || opcode == D3DSIO_LOGP) {
            tokens.push_back(token);                            // instr
            tokens.push_back(token = pFunction[i++]);           // dest
            token = pFunction[i++];                             // src0

            // If no swizzling is done, then use the w-component.
            // See d8vk#43 for more information as this may need to change in some cases.
            if (((token & D3DVS_NOSWIZZLE) == D3DVS_NOSWIZZLE)) {
              token &= ~D3DVS_SWIZZLE_MASK;
              token |= (D3DVS_X_W | D3DVS_Y_W | D3DVS_Z_W | D3DVS_W_W);
            }
          }
        }
        tokens.push_back(token);
      } while (token != D3DVS_END());
    }

    return res;
  }

}
