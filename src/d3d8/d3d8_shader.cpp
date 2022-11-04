
#include "d3d8_shader.h"

#define VSD_SHIFT_MASK(token, field) ( (token & field ## MASK) >> field ## SHIFT )

// Magic number from D3DVSD_REG()
#define VSD_SKIP_FLAG 0x10000000

#define VS_BIT_PARAM 0x80000000

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
   * \param [in]  regType  DxsoRegisterType
   * \cite https://learn.microsoft.com/en-us/windows-hardware/drivers/display/dcl-instruction
   */
  constexpr DWORD encodeDeclaration(d3d9::D3DDECLUSAGE usage) {
    DWORD token = 0;
    token |= usage & 0x1F;  // bits 0:4   DxsoUsage
    token |= 0 << 16;       // bits 16:19 usageIndex (TODO: should this change?)
    token |= 1 << 31;       // bit 31     always 1
    return token;
  }

  D3D9VertexShaderCode translateVertexShader8(const DWORD* pDeclaration, const DWORD* pFunction) {
    using d3d9::D3DDECLTYPE;
    D3D9VertexShaderCode result;

    std::vector<DWORD>& tokens = result.function;
    std::vector<DWORD> defs; // Constant definitions

    // shaderInputRegisters:
    // set bit N to enable input register vN
    DWORD shaderInputRegisters = 0;
    
    d3d9::D3DVERTEXELEMENT9* vertexElements = result.declaration;
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

          currentStream = streamNum;
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
            currentOffset += skipCount * sizeof(DWORD);
            break;
          }

          // D3DVSD_REG
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
          // TODO: D3DVSD_TOKEN_EXT
          break;
        }
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


    if (pFunction != nullptr) {
      // copy first token
      // TODO: ensure first token is always only one dword
      tokens.push_back(pFunction[0]);

      // insert dcl instructions 
      for (int vn = 0; vn < D3D8_NUM_VERTEX_INPUT_REGISTERS; vn++) {

        // if bit N is set then we need to dcl register vN
        if ((shaderInputRegisters & (1 << vn)) != 0) {

          Logger::debug(str::format("\tShader Input Regsiter: v", vn));

          DWORD usage = D3D8_VERTEX_INPUT_REGISTERS[vn][0];
          DWORD index = D3D8_VERTEX_INPUT_REGISTERS[vn][1];

          DWORD dclUsage  = (usage << D3DSP_DCL_USAGE_SHIFT) & D3DSP_DCL_USAGE_MASK;           // usage
          dclUsage       |= (index << D3DSP_DCL_USAGEINDEX_SHIFT) & D3DSP_DCL_USAGEINDEX_MASK; // usage index

          tokens.push_back(encodeInstruction(d3d9::D3DSIO_DCL));              // dcl opcode
          tokens.push_back(encodeDeclaration(d3d9::D3DDECLUSAGE(dclUsage)));  // usage token
          tokens.push_back(encodeDestRegister(d3d9::D3DSPR_INPUT, vn));       // dest register num
        }
      }

      // copy constant defs
      for (DWORD def : defs) {
        tokens.push_back(def);
      }

      // copy shader tokens from input,
      // skip first token (we already copied it)
      i = 1;
      do {
        token = pFunction[i++];

        DWORD opcode = token & D3DSI_OPCODE_MASK;

        // Instructions
        if ((token & VS_BIT_PARAM) == 0) {
          // RSQ uses the w component in D3D8
          if (opcode == D3DSIO_RSQ) {
            tokens.push_back(token);                  // instr
            tokens.push_back(token = pFunction[i++]); // dest
            token = pFunction[i++];                   // src0
            token |= 0b1111 << D3DVS_SWIZZLE_SHIFT;   // swizzle: .wwww
          }
        }
        tokens.push_back(token);

        //Logger::debug(str::format(std::hex, token));
      } while (token != D3DVS_END());
    }

    return result;
  }

}