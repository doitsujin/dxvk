#version 450

layout(set = 0, binding = 0)
uniform sampler2DMSArray s_image;

layout(location = 0) out vec4 o_color;

void main() {
  int sampleCount = textureSamples(s_image);

  o_color = vec4(0.0f);
  for (int i = 0; i < sampleCount; i++)
    o_color += texelFetch(s_image, ivec3(gl_FragCoord.xy, gl_Layer), i);
  o_color /= float(sampleCount);
}