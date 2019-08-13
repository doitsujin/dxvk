#version 450

#extension GL_ARB_shader_stencil_export : enable

#define VK_RESOLVE_MODE_NONE_KHR            (0)
#define VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR (1 << 0)
#define VK_RESOLVE_MODE_AVERAGE_BIT_KHR     (1 << 1)
#define VK_RESOLVE_MODE_MIN_BIT_KHR         (1 << 2)
#define VK_RESOLVE_MODE_MAX_BIT_KHR         (1 << 3)

layout(constant_id = 0) const int c_samples = 1;
layout(constant_id = 1) const int c_mode_d  = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR;
layout(constant_id = 2) const int c_mode_s  = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR;

layout(binding = 0) uniform  sampler2DMSArray s_depth;
layout(binding = 1) uniform usampler2DMSArray s_stencil;

layout(push_constant)
uniform u_info_t {
  ivec2 offset;
} u_info;

float resolve_depth(ivec3 coord) {
  float depth = 0.0f;

  switch (c_mode_d) {
    case VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR:
      depth = texelFetch(s_depth, coord, 0).r;
      break;
    
    case VK_RESOLVE_MODE_AVERAGE_BIT_KHR:
      depth = texelFetch(s_depth, coord, 0).r;
      for (int i = 1; i < c_samples; i++)
        depth += texelFetch(s_depth, coord, i).r;
      depth /= float(c_samples);
      break;
    
    case VK_RESOLVE_MODE_MIN_BIT_KHR:
      depth = texelFetch(s_depth, coord, 0).r;
      for (int i = 1; i < c_samples; i++)
        depth = min(depth, texelFetch(s_depth, coord, i).r);
      break;
    
    case VK_RESOLVE_MODE_MAX_BIT_KHR:
      depth = texelFetch(s_depth, coord, 0).r;
      for (int i = 1; i < c_samples; i++)
        depth = max(depth, texelFetch(s_depth, coord, i).r);
      break;
  }

  return depth;
}

int resolve_stencil(ivec3 coord) {
  uint stencil = 0u;

  switch (c_mode_s) {
    case VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR:
      stencil = texelFetch(s_stencil, coord, 0).r;
      break;
    
    case VK_RESOLVE_MODE_MIN_BIT_KHR:
      stencil = texelFetch(s_stencil, coord, 0).r;
      for (int i = 1; i < c_samples; i++)
        stencil = min(stencil, texelFetch(s_stencil, coord, i).r);
      break;
    
    case VK_RESOLVE_MODE_MAX_BIT_KHR:
      stencil = texelFetch(s_stencil, coord, 0).r;
      for (int i = 1; i < c_samples; i++)
        stencil = max(stencil, texelFetch(s_stencil, coord, i).r);
      break;
  }

  return int(stencil);
}

void main() {
  ivec3 coord = ivec3(gl_FragCoord.xy + u_info.offset, gl_Layer);

  gl_FragDepth         = resolve_depth(coord);
  gl_FragStencilRefARB = resolve_stencil(coord);
}