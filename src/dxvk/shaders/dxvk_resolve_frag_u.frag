#version 450

#extension GL_EXT_samplerless_texture_functions : enable

layout(binding = 0) uniform utexture2DMSArray s_image;

layout(location = 0) out uvec4 o_color;

layout(push_constant)
uniform u_info_t {
  ivec2 offset;
} u_info;

void main() {
  ivec3 coord = ivec3(gl_FragCoord.xy + u_info.offset, gl_Layer);
  o_color = texelFetch(s_image, coord, 0);
}