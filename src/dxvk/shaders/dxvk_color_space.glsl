#define VK_COLOR_SPACE_SRGB_NONLINEAR_KHR (0)
#define VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT (1000104002)
#define VK_COLOR_SPACE_HDR10_ST2084_EXT (1000104008)
#define VK_COLOR_SPACE_PASS_THROUGH_EXT (1000104013)

#define SDR_NITS (203.0f)

const mat3 rec709_to_xyz = mat3(
   0.4123908,  0.2126390,  0.0193308,
   0.3575843,  0.7151687,  0.1191948,
   0.1804808,  0.0721923,  0.9505322);

const mat3 xyz_to_rec2020 = mat3(
   1.7166512, -0.6666844,  0.0176399,
  -0.3556708,  1.6164812, -0.0427706,
  -0.2533663,  0.0157685,  0.9421031);

mat3 rec709_to_rec2020 = xyz_to_rec2020 * rec709_to_xyz;
mat3 rec2020_to_rec709 = inverse(rec709_to_rec2020);


// sRGB functions
vec3 linear_to_srgb(vec3 linear) {
  bvec3 isLo = lessThanEqual(linear, vec3(0.0031308f));

  vec3 loPart = linear * 12.92f;
  vec3 hiPart = pow(linear, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;
  return mix(hiPart, loPart, isLo);
}

vec3 srgb_to_linear(vec3 srgb) {
  bvec3 isLo = lessThanEqual(srgb, vec3(0.04045f));

  vec3 loPart = srgb / 12.92f;
  vec3 hiPart = pow((srgb + 0.055f) / 1.055f, vec3(12.0f / 5.0f));
  return mix(hiPart, loPart, isLo);
}


// Perceptual quantizer conversion
vec3 nits_to_pq(vec3 nits) {
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

vec3 pq_to_nits(vec3 pq) {
  const float c1 = 0.8359375f;
  const float c2 = 18.8515625f;
  const float c3 = 18.6875f;
  const float m1 = 0.1593017578125f;
  const float m2 = 78.84375f;

  vec3 pq_m2 = pow(pq, vec3(1.0f / m2));

  vec3 num = max(pq_m2 - c1, 0.0f);
  vec3 den = c2 - c3 * pq_m2;

  vec3 y = pow(num / den, vec3(1.0f / m1));
  return 10000.0f * y;  
}


// scRGB conversion
vec3 nits_to_sc_rgb(vec3 nits) {
  return nits / 80.0f;
}

vec3 sc_rgb_to_nits(vec3 sc_rgb) {
  return sc_rgb * 80.0f;
}
