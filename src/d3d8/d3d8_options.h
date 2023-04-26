#pragma once

#include "d3d8_include.h"
#include "../d3d9/d3d9_bridge.h"
#include "../util/config/config.h"

namespace dxvk {
  struct D3D8Options {

    /// Remap DEFAULT pool vertex and index buffers above this size to the MANAGED pool
    /// to improve performance by avoiding waiting for games that frequently lock (large) buffers.
    ///
    /// This implicitly disables direct buffer mapping. Some applications may need this option
    /// disabled for certain (smaller) buffers to keep from overwriting in-use buffer regions.
    uint32_t managedBufferPlacement = 0;

    /// Some games rely on undefined behavior by using undeclared vertex shader inputs.
    /// The simplest way to fix them is to simply modify their vertex shader decl.
    /// 
    /// This option takes a comma-separated list of colon-separated number pairs, where
    /// the first number is a D3DVSDE_REGISTER value, the second is a D3DVSDT_TYPE value.
    ///   e.g. "0:2,3:2,7:1" for float3 position : v0, float3 normal : v3, float2 uv : v7
    std::vector<std::pair<D3DVSDE_REGISTER, D3DVSDT_TYPE>> forceVsDecl;

    D3D8Options() {}
    D3D8Options(const Config& config) {
      useShadowBuffers        = config.getOption<bool>       ("d3d8.useShadowBuffers",       useShadowBuffers);
      int32_t minManagedSize  = config.getOption<int32_t>    ("d3d8.managedBufferPlacement", managedBufferPlacement);
      managedBufferPlacement  = config.getOption<bool>       ("d3d8.managedBufferPlacement", true) ? minManagedSize : UINT32_MAX;
      auto forceVsDeclStr     = config.getOption<std::string>("d3d8.forceVsDecl",  "");

      parseVsDecl(forceVsDeclStr);
    }

    void parseVsDecl(const std::string& decl);
  };
}
