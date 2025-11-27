#version 460

#extension GL_GOOGLE_include_directive : enable

#include "dxvk_formats.glsl"

layout(binding = 0) uniform samplerBuffer u_data;

layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_data_t {
  uvec3 image_offset;
  uint buffer_offset;
  uvec3 image_extent;
  uint buffer_image_width;
  uint buffer_image_height;
  uint stencil_bit_index;
};

void main() {
  uvec2 location = uvec2(gl_FragCoord.xy) - image_offset.xy;

  int offset = int(buffer_offset + location.x +
    buffer_image_width * (location.y + buffer_image_height * gl_Layer));

  o_color = texelFetch(u_data, offset);
}
