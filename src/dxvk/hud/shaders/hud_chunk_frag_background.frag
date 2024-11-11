#version 450

#extension GL_GOOGLE_include_directive : require

#include "hud_frag_common.glsl"

layout(push_constant)
uniform push_data_t {
  uvec2 surface_size;
  float opacity;
  float scale;
};

layout(location = 0) flat in uint v_active;

layout(location = 0) out vec4 o_color;

void main() {
  o_color = vec4(0.0f, 0.0f, 0.0f, 0.75f);

  if (v_active == 0u) {
    uvec2 pos = uvec2(gl_FragCoord.xy);

    if (((pos.x + pos.y) & 7u) < 2u)
      o_color.xyz = vec3(0.25f);
  }

  o_color.a *= opacity;
  o_color.rgb *= o_color.a;
  o_color = linear_to_output(o_color);
}
