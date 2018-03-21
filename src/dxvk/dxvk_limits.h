#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  enum DxvkLimits : size_t {
    MaxNumRenderTargets         =    8,
    MaxNumVertexAttributes      =   32,
    MaxNumVertexBindings        =   32,
    MaxNumOutputStreams         =    4,
    MaxNumViewports             =   16,
    MaxNumResourceSlots         = 1208,
    MaxNumActiveBindings        =  128,
    MaxNumQueuedCommandBuffers  =    8,
    MaxNumQueryCountPerPool     =  128,
    MaxVertexBindingStride      = 2048,
    MaxPushConstantSize         =  128,
  };
  
}