#include "dxvk_color_space.glsl"

layout(constant_id = 0) const uint c_samples = 0u;
layout(constant_id = 1) const bool c_gamma = false;

layout(constant_id = 2) const uint c_src_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
layout(constant_id = 3) const bool c_src_is_srgb = true;
layout(constant_id = 4) const uint c_dst_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
layout(constant_id = 5) const bool c_dst_is_srgb = true;

layout(set = 0, binding = 0) uniform sampler2D s_image;
layout(set = 0, binding = 0) uniform sampler2DMS s_image_ms;
layout(set = 0, binding = 1) uniform sampler1D s_gamma;

layout(push_constant)
uniform present_info_t {
  ivec2 src_offset;
  ivec2 src_extent;
  ivec2 dst_offset;
};

vec4 input_to_sc_rgb(vec4 color) {
  switch (c_src_color_space) {
    default:
    case VK_COLOR_SPACE_PASS_THROUGH_EXT:
      return color;

    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: {
      if (!c_src_is_srgb)
        color.rgb = srgb_to_linear(color.rgb);

      color.rgb = nits_to_sc_rgb(color.rgb * SDR_NITS);
      return color;
    }

    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
      return color;

    case VK_COLOR_SPACE_HDR10_ST2084_EXT:
      color.rgb = nits_to_sc_rgb(pq_to_nits(color.rgb));
      color.rgb = rec2020_to_rec709 * color.rgb;
      return color;
  }
}


vec4 sc_rgb_to_output(vec4 color) {
  if (c_gamma) {
    // If we need to apply a gamma curve, convert to sRGB, perform
    // the lookup, and then convert back to scRGB as necessary
    if (c_src_color_space != VK_COLOR_SPACE_PASS_THROUGH_EXT) {
      color.rgb = sc_rgb_to_nits(color.rgb) / SDR_NITS;
      color.rgb = linear_to_srgb(color.rgb);
    } else if (c_src_is_srgb) {
      color.rgb = linear_to_srgb(color.rgb);
    }

    color.rgb = vec3(
      texture(s_gamma, color.r).r,
      texture(s_gamma, color.g).g,
      texture(s_gamma, color.b).b);

    if (c_dst_color_space != VK_COLOR_SPACE_PASS_THROUGH_EXT) {
      color.rgb = srgb_to_linear(color.rgb);
      color.rgb = nits_to_sc_rgb(color.rgb * SDR_NITS);
    } else if (c_dst_is_srgb) {
      color.rgb = srgb_to_linear(color.rgb);
    }
  }

  switch (c_dst_color_space) {
    default:
    case VK_COLOR_SPACE_PASS_THROUGH_EXT:
      // If we applied a gamma curve, the output is already correct
      if (!c_gamma && c_src_is_srgb != c_dst_is_srgb) {
        color.rgb = c_src_is_srgb
          ? linear_to_srgb(color.rgb)
          : srgb_to_linear(color.rgb);
      }

      return color;

    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: {
      color.rgb = sc_rgb_to_nits(color.rgb) / SDR_NITS;

      if (!c_dst_is_srgb)
        color.rgb = linear_to_srgb(color.rgb);

      return color;
    }

    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
      return color;

    case VK_COLOR_SPACE_HDR10_ST2084_EXT:
      color.rgb = rec709_to_rec2020 * color.rgb;
      color.rgb = nits_to_pq(sc_rgb_to_nits(color.rgb));
      return color;
  }
}
