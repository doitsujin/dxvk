float unormalize(uint value, int bits) {
  const int range = (1 << bits) - 1;

  return float(value) / float(range);
}

float snormalize(int value, int bits) {
  const int range = (1 << (bits - 1)) - 1;

  // Min because, -32 and -31 map to -1.0f, and we
  // divide by 31.
  return max(float(value) / float(range), -1.0);
}

float unpackUnorm(uint p) {
  return float(p) / 255.0;
}

vec2 unpackUnorm2x8(uint p) {
  uvec2 value = uvec2(p & 0xFF, p >> 8);
  return vec2(unpackUnorm(value.x), unpackUnorm(value.y));
}

mat3x4 g_yuv_to_rgb = {
  { 298 / 256,  0,          409 / 256, 0.5 },
  { 298 / 256, -100 / 256, -208 / 256, 0.5 },
  { 298 / 256,  516 / 256,  0,         0.5 }
};

vec4 convertYUV(vec3 yuv) {
  vec3 value = vec4(yuv, 1 / 255.0) * g_yuv_to_rgb;

  return vec4(clamp(value, 0, 1), 1);
}

mat3x3 g_bt709_to_rgb = {
  { 1.164,  0,          1.793    },
  { 1.164, -0.213,     -0.533    },
  { 1.164,  2.112,      0        }
};

vec4 convertBT_709(vec3 cde) {
  return vec4(clamp(cde * g_bt709_to_rgb, 0, 1), 1);
}
