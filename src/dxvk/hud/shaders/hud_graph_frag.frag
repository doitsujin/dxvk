#version 450

#extension GL_GOOGLE_include_directive : require

#include "hud_frag_common.glsl"

layout(location = 0) in  vec2 v_coord;
layout(location = 0) out vec4 o_color;

struct line_point_t {
  float value;
  uint  color;
};

layout(binding = 0, std430)
readonly buffer line_data_t {
  line_point_t points[];
};

layout(push_constant)
uniform push_data_t {
  uint offset;
  uint count;
  vec2 pos;
  vec2 size;
  vec2 scale;
  float opacity;
};

void main() {
  float cx = v_coord.x * float(count);
  float fx = fract(cx);

  // We need to roll our own linear interpolation here
  uint p0_idx = min(uint(floor(cx)), count - 1) + offset;
  uint p1_idx = min(uint(ceil(cx)),  count - 1) + offset;

  line_point_t p0 = points[p0_idx];
  line_point_t p1 = points[p1_idx];

  float value = mix(p0.value, p1.value, fx);
  float alpha = value + v_coord.y - 1.0f;
  alpha = min(alpha / dFdy(v_coord.y), 1.0f);

  if (alpha <= 0.0f)
    discard;

  o_color = mix(
    unpackUnorm4x8(p0.color),
    unpackUnorm4x8(p1.color), fx);
  o_color *= alpha * opacity;

  o_color.rgb = encodeOutput(o_color.rgb);
}
