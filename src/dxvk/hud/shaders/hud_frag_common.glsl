#define VK_COLOR_SPACE_SRGB_NONLINEAR_KHR (0)
#define VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT (1000104002)
#define VK_COLOR_SPACE_HDR10_ST2084_EXT (1000104008)
#define VK_COLOR_SPACE_PASS_THROUGH_EXT (1000104013)

#define HUD_NITS (203.0f)

const mat3 rec709_to_xyz = mat3(
   0.4123908,  0.2126390,  0.0193308,
   0.3575843,  0.7151687,  0.1191948,
   0.1804808,  0.0721923,  0.9505322);

const mat3 xyz_to_rec2020 = mat3(
   1.7166512, -0.6666844,  0.0176399,
  -0.3556708,  1.6164812, -0.0427706,
  -0.2533663,  0.0157685,  0.9421031);

const mat3 rec709_to_rec2020 = xyz_to_rec2020 * rec709_to_xyz;

// Spec constants must always default to
// zero for DXVK to handle them properly
layout(constant_id = 0) const uint hud_color_space = 0;

vec3 encodeSrgb(vec3 linear) {
  bvec3 isLo = lessThanEqual(linear, vec3(0.0031308f));

  vec3 loPart = linear * 12.92f;
  vec3 hiPart = pow(linear, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;
  return mix(hiPart, loPart, isLo);
}

vec3 encodePq(vec3 nits) {
  const float c1 = 0.8359375f;
  const float c2 = 18.8515625f;
  const float c3 = 18.6875f;
  const float m1 = 0.1593017578125f;
  const float m2 = 78.84375f;

  vec3 y = clamp(nits / 10000.0f, vec3(0.0f), vec3(1.0f));
  vec3 y_m1 = pow(y, vec3(m1));

  vec3 num = c1 + c2 * y_m1;
  vec3 den = 1.0f + c3 * y_m1;

  return pow(num / den, vec3(m2));
}

vec3 encodeScRgb(vec3 nits) {
  return nits / 80.0f;
}

vec3 encodeOutput(vec3 linear) {
  switch (hud_color_space) {
    default:
      return linear;

    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
      return encodeSrgb(linear);

    case VK_COLOR_SPACE_HDR10_ST2084_EXT: {
      vec3 rec2020 = rec709_to_rec2020 * linear;
      return encodePq(rec2020 * HUD_NITS);
    }

    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
      return encodeScRgb(linear * HUD_NITS);
  }
}
