#include "dxso_header.h"

namespace dxvk {

  DxsoHeader::DxsoHeader(DxsoReader& reader) {
    uint32_t headerToken = reader.readu32();

    uint32_t headerTypeMask = headerToken & 0xffff0000;

    DxsoProgramType programType;
    if (headerTypeMask == 0xffff0000)
      programType = DxsoProgramTypes::PixelShader;
    else if (headerTypeMask == 0xfffe0000)
      programType = DxsoProgramTypes::VertexShader;
    else
      throw DxvkError("DxsoHeader: invalid header - invalid version");

    const uint32_t majorVersion = (headerToken >> 8) & 0xff;
    const uint32_t minorVersion = headerToken & 0xff;

    m_info = DxsoProgramInfo{ programType, minorVersion, majorVersion };
  }

}