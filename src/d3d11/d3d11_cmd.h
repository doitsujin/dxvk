#pragma once

#include "d3d11_include.h"

namespace dxvk {

  /**
   * \brief D3D11 command type
   * 
   * Used to identify the type of command
   * data most recently added to a CS chunk.
   */
  enum class D3D11CmdType : uint32_t {
    None,
    DrawIndirect,
    DrawIndirectIndexed,
    Draw,
    DrawIndexed,
  };


  /**
   * \brief Indirect draw command data
   * 
   * Stores the offset into the draw buffer for
   * the first draw, as well as the number of
   * draws to execute.
   */
  struct D3D11CmdDrawIndirectData {
    uint32_t            offset;
    uint32_t            count;
    uint32_t            stride;
  };

}
