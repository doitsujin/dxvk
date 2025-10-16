#include "d3d7/d3d7_device.h"
#include "d3d6/d3d6_device.h"
#include "d3d5/d3d5_device.h"
#include "d3d3/d3d3_device.h"

namespace dxvk {

  D3DMultithread::D3DMultithread(
          BOOL                  Protected)
    : m_protected( Protected ) { }

}