#version 450

layout(constant_id = 3) const bool c_planar = true;

// Can't use matrix types here since even a two-row
// matrix will be padded to 16 bytes per column for
// absolutely no reason
layout(std140, set = 0, binding = 0)
uniform ubo_t {
  vec4 color_matrix_r1;
  vec4 color_matrix_r2;
  vec4 color_matrix_r3;
  vec2 coord_matrix_c1;
  vec2 coord_matrix_c2;
  vec2 coord_matrix_c3;
  float y_min;
  float y_max;
};

layout(location = 0) in vec2 i_texcoord;
layout(location = 0) out vec4 o_color;

layout(set = 0, binding = 1) uniform sampler s_sampler;
layout(set = 0, binding = 2) uniform texture2D s_inputY;
layout(set = 0, binding = 3) uniform texture2D s_inputCbCr;

void main() {
  // Transform input texture coordinates to
  // account for rotation and source rectangle
  mat3x2 coord_matrix = mat3x2(
    coord_matrix_c1,
    coord_matrix_c2,
    coord_matrix_c3);

  vec2 coord = coord_matrix * vec3(i_texcoord, 1.0f);

  // Fetch source image color
  vec4 color = vec4(0.0f, 0.0f, 0.0f, 1.0f);

  if (c_planar) {
    color.g  = texture(sampler2D(s_inputY,    s_sampler), coord).r;
    color.rb = texture(sampler2D(s_inputCbCr, s_sampler), coord).gr;
    color.g  = clamp((color.g - y_min) / (y_max - y_min), 0.0f, 1.0f);
  } else {
    color = texture(sampler2D(s_inputY, s_sampler), coord);
  }

  // Color space transformation
  mat3x4 color_matrix = mat3x4(
    color_matrix_r1,
    color_matrix_r2,
    color_matrix_r3);

  o_color.rgb = vec4(color.rgb, 1.0f) * color_matrix;
  o_color.a = color.a;
}
