#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  enum DxvkLimits : size_t {
    MaxNumRenderTargets         =     8,
    MaxNumVertexAttributes      =    32,
    MaxNumVertexBindings        =    32,
    MaxNumXfbBuffers            =     4,
    MaxNumXfbStreams            =     4,
    MaxNumViewports             =    16,
    MaxNumUniformBufferSlots    =   128,
    MaxNumSamplerSlots          =   128,
    MaxNumResourceSlots         =  1024,
    MaxNumQueuedCommandBuffers  =    32,
    MaxNumQueryCountPerPool     =   128,
    MaxNumSpecConstants         =    12,
    MaxUniformBufferSize        = 65536,
    MaxVertexBindingStride      =  2048,
    MaxPushConstantSize         =    64,
  };
  
}
