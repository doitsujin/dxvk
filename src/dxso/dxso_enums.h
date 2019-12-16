#pragma once

#include "dxso_include.h"

#include <cstdint>

namespace dxvk {

  /**
   * \brief Instruction code listing
   */
  enum class DxsoOpcode : uint32_t {
    Nop          = 0,
    Mov          ,
    Add          ,
    Sub          ,
    Mad          ,
    Mul          ,
    Rcp          ,
    Rsq          ,
    Dp3          ,
    Dp4          ,
    Min          ,
    Max          ,
    Slt          ,
    Sge          ,
    Exp          ,
    Log          ,
    Lit          ,
    Dst          ,
    Lrp          ,
    Frc          ,
    M4x4         ,
    M4x3         ,
    M3x4         ,
    M3x3         ,
    M3x2         ,
    Call         ,
    CallNz       ,
    Loop         ,
    Ret          ,
    EndLoop      ,
    Label        ,
    Dcl          ,
    Pow          ,
    Crs          ,
    Sgn          ,
    Abs          ,
    Nrm          ,
    SinCos       ,
    Rep          ,
    EndRep       ,
    If           ,
    Ifc          ,
    Else         ,
    EndIf        ,
    Break        ,
    BreakC       ,
    Mova         ,
    DefB         ,
    DefI         ,

    TexCoord     = 64,
    TexKill      ,
    Tex          ,
    TexBem       ,
    TexBemL      ,
    TexReg2Ar    ,
    TexReg2Gb    ,
    TexM3x2Pad   ,
    TexM3x2Tex   ,
    TexM3x3Pad   ,
    TexM3x3Tex   ,
    Reserved0    ,
    TexM3x3Spec  ,
    TexM3x3VSpec ,
    ExpP         ,
    LogP         ,
    Cnd          ,
    Def          ,
    TexReg2Rgb   ,
    TexDp3Tex    ,
    TexM3x2Depth ,
    TexDp3       ,
    TexM3x3      ,
    TexDepth     ,
    Cmp          ,
    Bem          ,
    Dp2Add       ,
    DsX          ,
    DsY          ,
    TexLdd       ,
    SetP         ,
    TexLdl       ,
    BreakP       ,

    Phase        = 0xfffd,
    Comment      = 0xfffe,
    End          = 0xffff
  };

  std::ostream& operator << (std::ostream& os, DxsoOpcode opcode);

  enum class DxsoRegisterType : uint32_t {
    Temp           =  0, // Temporary Register File
    Input          =  1, // Input Register File
    Const          =  2, // Constant Register File
    Addr           =  3, // Address Register (VS)
    Texture        =  3, // Texture Register File (PS)
    RasterizerOut  =  4, // Rasterizer Register File
    AttributeOut   =  5, // Attribute Output Register File
    TexcoordOut    =  6, // Texture Coordinate Output Register File
    Output         =  6, // Output register file for VS3.0+
    ConstInt       =  7, // Constant Integer Vector Register File
    ColorOut       =  8, // Color Output Register File
    DepthOut       =  9, // Depth Output Register File
    Sampler        = 10, // Sampler State Register File
    Const2         = 11, // Constant Register File  2048 - 4095
    Const3         = 12, // Constant Register File  4096 - 6143
    Const4         = 13, // Constant Register File  6144 - 8191
    ConstBool      = 14, // Constant Boolean register file
    Loop           = 15, // Loop counter register file
    TempFloat16    = 16, // 16-bit float temp register file
    MiscType       = 17, // Miscellaneous (single) registers.
    Label          = 18, // Label
    Predicate      = 19, // Predicate register
    PixelTexcoord  = 20
  };

  enum class DxsoUsage : uint32_t {
    Position        = 0,
    BlendWeight,   // 1
    BlendIndices,  // 2
    Normal,        // 3
    PointSize,     // 4
    Texcoord,      // 5
    Tangent,       // 6
    Binormal,      // 7
    TessFactor,    // 8
    PositionT,     // 9
    Color,         // 10
    Fog,           // 11
    Depth,         // 12
    Sample,        // 13
  };

  enum class DxsoTextureType : uint32_t {
    Texture2D   = 2,
    TextureCube = 3,
    Texture3D   = 4
  };

  enum DxsoReasterizerOutIndices : uint32_t {
    RasterOutPosition  = 0,
    RasterOutFog       = 1,
    RasterOutPointSize = 2
  };

  enum DxsoMiscTypeIndices : uint32_t {
    MiscTypePosition,
    MiscTypeFace,
  };

}