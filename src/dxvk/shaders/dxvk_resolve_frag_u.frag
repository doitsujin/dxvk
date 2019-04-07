#version 450

layout(binding = 0) uniform usampler2DMSArray s_image;

layout(location = 0) out uvec4 o_color;

void main() {
  ivec3 coord = ivec3(gl_FragCoord.xy, gl_Layer);
  o_color = texelFetch(s_image, coord, 0);
}