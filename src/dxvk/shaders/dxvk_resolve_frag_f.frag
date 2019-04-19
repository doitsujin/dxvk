#version 450

layout(constant_id = 0) const int c_samples = 1;

layout(binding = 0) uniform sampler2DMSArray s_image;

layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform u_info_t {
  ivec2 offset;
} u_info;

void main() {
  ivec3 coord = ivec3(gl_FragCoord.xy + u_info.offset, gl_Layer);
  vec4 color = vec4(0.0f);
  for (int i = 0; i < c_samples; i++)
    color += texelFetch(s_image, coord, i);
  o_color = color / float(c_samples);
}