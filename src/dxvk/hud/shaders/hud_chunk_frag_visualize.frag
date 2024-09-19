#version 450

#extension GL_GOOGLE_include_directive : require

#include "hud_frag_common.glsl"

layout(location = 0) in  vec2 v_coord;
layout(location = 0) out vec4 o_color;

layout(binding = 0, std430)
readonly buffer mask_data_t {
  uint masks[];
};

layout(push_constant)
uniform push_data_t {
  vec2 pos;
  vec2 size;
  vec2 scale;
  float opacity;
  uint color;
  uint maskIndex;
  uint pageCount;
};

void main() {
  vec4 rgba = unpackUnorm4x8(color);

  float dx = dFdx(v_coord.x * float(pageCount)) / 2.0f - 0.5f;

  uvec2 pageRange = uvec2(clamp(
    (v_coord.xx * float(pageCount)) + vec2(-dx, dx),
    vec2(0.0), vec2(float(pageCount - 1u))));

  uint bitsTotal = max(pageRange.y - pageRange.x, 1u);
  uint bitsSet = 0u;

  uint index = pageRange.x / 32u;
  uint shift = pageRange.x % 32u;

  if (shift + bitsTotal <= 32u) {
    bitsSet = bitCount(bitfieldExtract(
      masks[maskIndex + index], int(shift), int(bitsTotal)));
  } else {
    bitsSet = bitCount(masks[maskIndex + (index++)] >> shift);
    uint bitsCounted = 32u - shift;

    while (bitsCounted + 32u <= bitsTotal) {
      bitsSet += bitCount(masks[maskIndex + (index++)]);
      bitsCounted += 32u;
    }

    if (bitsCounted < bitsTotal) {
      bitsSet += bitCount(bitfieldExtract(
        masks[maskIndex + (index++)], 0, int(bitsTotal - bitsCounted)));
    }
  }

  if (bitsSet == 0u)
    discard;

  float blendFactor = 0.5f * float(bitsSet) / max(float(bitsTotal), 1.0f);
  o_color = vec4(mix(rgba.rgb, vec3(1.0f), blendFactor), rgba.a * opacity);
  o_color.rgb = encodeOutput(o_color.rgb);
}
