#version 450

#extension GL_GOOGLE_include_directive : require

#include "hud_frag_common.glsl"

layout(location = 0) in  vec2 v_coord;

layout(location = 1, component = 0) flat in uint v_color;
layout(location = 1, component = 1) flat in uint v_mask_index;
layout(location = 1, component = 2) flat in uint v_page_count;
layout(location = 1, component = 3) flat in uint v_active;

layout(location = 0) out vec4 o_color;

layout(binding = 1, std430)
readonly buffer mask_data_t {
  uint masks[];
};

layout(push_constant)
uniform push_data_t {
  uvec2 surface_size;
  float opacity;
  float scale;
};

void main() {
  float dx = dFdx(v_coord.x * float(v_page_count)) / 2.0f - 0.5f;

  uvec2 pageRange = uvec2(clamp(
    (v_coord.xx * float(v_page_count)) + vec2(-dx, dx),
    vec2(0.0), vec2(float(v_page_count - 1u))));

  uint bitsTotal = max(pageRange.y - pageRange.x, 1u);
  uint bitsSet = 0u;

  uint index = pageRange.x / 32u;
  uint shift = pageRange.x % 32u;

  if (shift + bitsTotal <= 32u) {
    bitsSet = bitCount(bitfieldExtract(
      masks[v_mask_index + index], int(shift), int(bitsTotal)));
  } else {
    bitsSet = bitCount(masks[v_mask_index + (index++)] >> shift);
    uint bitsCounted = 32u - shift;

    while (bitsCounted + 32u <= bitsTotal) {
      bitsSet += bitCount(masks[v_mask_index + (index++)]);
      bitsCounted += 32u;
    }

    if (bitsCounted < bitsTotal) {
      bitsSet += bitCount(bitfieldExtract(
        masks[v_mask_index + (index++)], 0, int(bitsTotal - bitsCounted)));
    }
  }

  if (bitsSet == 0u)
    discard;

  vec4 color = unpackUnorm4x8(v_color);

  float blendFactor = 0.5f * float(bitsSet) / max(float(bitsTotal), 1.0f);
  o_color = vec4(mix(color.rgb, vec3(1.0f), blendFactor), color.a * opacity);
  o_color.rgb *= o_color.a;
  o_color = linear_to_output(o_color);
}
