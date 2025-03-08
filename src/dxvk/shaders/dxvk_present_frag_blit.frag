#version 450

#extension GL_GOOGLE_include_directive : enable

#include "dxvk_present_common.glsl"

layout(location = 0) in  vec2 i_coord;
layout(location = 0) out vec4 o_color;

void main() {
  vec2 coord = vec2(src_offset) + vec2(src_extent) * i_coord;
  vec2 delta = vec2(dFdx(coord.x), dFdy(coord.y));

  ivec2 i_coord = ivec2(coord);
  vec2  f_coord = fract(coord);

  if (all(lessThan(delta, vec2(1.0f)))) {
    // Map pixel rectangle to source image. If it is entirely contained within one
    // source pixel, just sample that pixel, otherwise do a linear interpolation.
    // For even scaling factors, this is essentially integer scaling.
    vec2 lo = max(coord - 0.5f * delta, vec2(src_offset));
    vec2 hi = min(coord + 0.5f * delta, vec2(src_offset + src_extent - 1));

    i_coord = ivec2(lo);
    f_coord = mix((hi - floor(hi)) / delta, vec2(0.0), equal(floor(lo), floor(hi)));
  }

  // Manually interpolate in the correct color space
  o_color = mix(mix(input_to_sc_rgb(texelFetch(s_image, i_coord + ivec2(0, 0), 0)),
                    input_to_sc_rgb(texelFetch(s_image, i_coord + ivec2(1, 0), 0)), f_coord.x),
                mix(input_to_sc_rgb(texelFetch(s_image, i_coord + ivec2(0, 1), 0)),
                    input_to_sc_rgb(texelFetch(s_image, i_coord + ivec2(1, 1), 0)), f_coord.x),
                f_coord.y);

  o_color = composite_image(o_color);
  o_color = sc_rgb_to_output(o_color);
}
