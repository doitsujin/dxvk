#version 450

#extension GL_GOOGLE_include_directive : enable

#include "dxvk_present_common.glsl"

layout(location = 0) out vec4 o_color;

void main() {
  ivec2 coord = ivec2(gl_FragCoord.xy) + src_offset - dst_offset;

  o_color = input_to_sc_rgb(texelFetch(s_image, coord, 0));
  o_color = composite_image(o_color);
  o_color = sc_rgb_to_output(o_color);
}
