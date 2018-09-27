#version 450

layout(set = 0, binding = 0)
uniform sampler1DArray s_image;

layout(push_constant)
uniform u_info_t {
  ivec2 offset;
} u_info;

void main() {
  gl_FragDepth = texelFetch(s_image,
    ivec2(gl_FragCoord.x + u_info.offset.x, gl_Layer), 0).r;
}