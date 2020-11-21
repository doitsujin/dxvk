#pragma once

#include "d3d11_include.h"

namespace dxvk {

  /**
   * \brief D3D11 command type
   * 
   * Used to identify the type of command
   * data most recently added to a CS chunk.
   */
  enum class D3D11CmdType {
    DrawIndirect,
    DrawIndirectIndexed,
  };


  /**
   * \brief Command data header
   * 
   * Stores the command type. All command
   * data structs must inherit this struct.
   */
  struct D3D11CmdData {
    D3D11CmdType        type;
  };


  /**
   * \brief Indirect draw command data
   * 
   * Stores the offset into the draw buffer for
   * the first draw, as well as the number of
   * draws to execute.
   */
  struct D3D11CmdDrawIndirectData : public D3D11CmdData {
    uint32_t            offset;
    uint32_t            count;
    uint32_t            stride;
  };

}