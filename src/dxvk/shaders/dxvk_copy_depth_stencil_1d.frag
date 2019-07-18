#version 450

#extension GL_ARB_shader_stencil_export : enable

layout(set = 0, binding = 0)
uniform sampler1DArray s_depth;

layout(set = 0, binding = 1)
uniform usampler1DArray s_stencil;

layout(push_constant)
uniform u_info_t {
  ivec2 offset;
} u_info;

void main() {
  ivec2 coord = ivec2(gl_FragCoord.x + u_info.offset.x, gl_Layer);
  gl_FragDepth         = texelFetch(s_depth,   coord, 0).r;
  gl_FragStencilRefARB = int(texelFetch(s_stencil, coord, 0).r);
}