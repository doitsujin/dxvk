#version 460

#extension GL_GOOGLE_include_directive : require

#include "dxvk_color_space.glsl"

layout(constant_id = 0) const uint s_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
layout(constant_id = 1) const bool s_srgb = false;

layout(set = 0, binding = 0) uniform sampler2D s_texture;

layout(location = 0) in vec2 i_texcoord;
layout(location = 0) out vec4 o_color;


vec4 linear_to_output(vec4 color) {
  switch (s_color_space) {
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: {
      if (!s_srgb)
        color.rgb = linear_to_srgb(color.rgb);

      return color;
    }

    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
      color.rgb = nits_to_sc_rgb(color.rgb * SDR_NITS);
      return color;

    case VK_COLOR_SPACE_HDR10_ST2084_EXT:
      color.rgb = rec709_to_rec2020 * color.rgb;
      color.rgb = nits_to_pq(color.rgb * SDR_NITS);
      return color;

    default: /* pass through */
      return color;
  }
}


void main() {
  o_color = linear_to_output(texture(s_texture, i_texcoord));
  o_color.rgb *= o_color.a;
}
