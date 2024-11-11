#version 450

#extension GL_GOOGLE_include_directive : require

#include "hud_frag_common.glsl"

layout(location = 0) in  vec2 v_coord;
layout(location = 0) out vec4 o_color;

#define NUM_FRAME_TIME_STAMPS (420)

layout(binding = 0, std430)
readonly buffer timestamp_buffer_t {
  uvec2 frame_timestamps_raw[2u];
  float frame_interval_ms[NUM_FRAME_TIME_STAMPS];

  float frame_time_avg_ms;
  float frame_time_min_ms;
  float frame_time_max_ms;
};

layout(push_constant)
uniform push_data_t {
  uvec2 surface_size;
  float opacity;
  float scale;
  uint  packed_xy;
  uint  packed_wh;
  uint  frame_index;
};

int max_index = NUM_FRAME_TIME_STAMPS - 1;

int compute_real_index(int local_index) {
  int real_index = int(frame_index) - local_index;

  if (real_index < 0)
    real_index += NUM_FRAME_TIME_STAMPS;

  return real_index;
}

float sample_at(float position) {
  int local_index = int(position);

  int lo_index = compute_real_index(clamp(local_index, 0, max_index));
  int hi_index = compute_real_index(clamp(local_index + 1, 0, max_index));

  float lo_value = frame_interval_ms[lo_index];
  float hi_value = frame_interval_ms[hi_index];

  return mix(lo_value, hi_value, fract(position));
}

void main() {
  float x_pos = (1.0f - v_coord.x) * float(NUM_FRAME_TIME_STAMPS);

  float x_delta = abs(dFdx(x_pos)) / 2.0f;
  float y_delta = abs(dFdy(v_coord.y));

  float ms_l = sample_at(x_pos - x_delta);
  float ms_r = sample_at(x_pos + x_delta);

  float ms_lo = min(ms_l, ms_r);
  float ms_hi = max(ms_l, ms_r);

  float ms_max = clamp(max(frame_time_max_ms, frame_time_avg_ms * 2.0f), 20.0f, 200.0f);

  float val_lo = min(ms_lo / ms_max, 1.0f) - y_delta;
  float val_hi = min(ms_hi / ms_max, 1.0f) - y_delta;

  float diff_lo = min(v_coord.y - val_lo, 0.0f);
  float diff_hi = min(val_hi - v_coord.y, 0.0f);

  float ms_y = ms_max * v_coord.y;

  vec4 line_color = vec4(0.776f, 0.812f, 0.882f, 1.0f);
  vec4 bg_color = vec4(0.0f, 0.0f, 0.0f, 0.75f);

  // Try to draw a somewhat defined line
  float diff = (diff_lo + diff_hi) + y_delta;
  o_color = mix(bg_color, line_color, clamp(diff / y_delta, 0.0f, 1.0f));

  o_color.a *= opacity;
  o_color.rgb *= o_color.a;

  o_color = linear_to_output(o_color);
}
