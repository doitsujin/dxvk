#pragma once

namespace dxvk {
  
  enum DxvkLimits : size_t {
    MaxNumRenderTargets  =   8,
    MaxNumUniformBuffers =  16,
    MaxNumSampledImages  =  16,
    MaxNumStorageBuffers = 128,
    MaxNumStorageImages  = 128,
    MaxNumVertexBuffers  =  32,
    MaxNumOutputStreams  =   4,
  };
  
}