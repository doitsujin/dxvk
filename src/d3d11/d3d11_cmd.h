#pragma once

#include "d3d11_include.h"

namespace dxvk {

  enum class D3D11CmdType {
    DrawIndirect,
    DrawIndirectIndexed,
  };

  struct D3D11CmdData {
    D3D11CmdType        type;
  };

  struct D3D11CmdDrawIndirectData : public D3D11CmdData {
    uint32_t            offset;
    uint32_t            count;
  };

}