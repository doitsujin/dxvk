#version 450

#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_spirv_intrinsics : enable

// GL_AMD_shader_fragment_mask was never updated to support
// sampler-less functions, so we have to define these manually
spirv_instruction(extensions = ["SPV_AMD_shader_fragment_mask"], capabilities = [5010], id = 5011)
uint fragment_mask_fetch(texture2DMSArray tex, ivec3 coord);

spirv_instruction(extensions = ["SPV_AMD_shader_fragment_mask"], capabilities = [5010], id = 5012)
vec4 fragment_fetch(texture2DMSArray tex, ivec3 coord, uint index);

layout(constant_id = 0) const int c_samples = 1;

layout(set = 0, binding = 0)
uniform texture2DMSArray s_image;

layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform u_info_t {
  ivec2 offset;
} u_info;

void main() {
  ivec3 coord = ivec3(gl_FragCoord.xy + u_info.offset, gl_Layer);

  // get a four-bit fragment index for each sample
  uint fragMask = fragment_mask_fetch(s_image, coord);

  // count number of occurences of each fragment
  // index in one four-bit counter for each sample
  uint fragCount = 0u;

  for (int i = 0; i < 4 * c_samples; i += 4) {
    uint fragIndex = bitfieldExtract(fragMask, i, 4);
    fragCount += 1u << (fragIndex << 2);
  }

  // perform necessary texture lookups to compute
  // final fragment color
  o_color = vec4(0.0f);
  
  while (fragCount != 0) {
    int fragIndex = findLSB(fragCount) >> 2;
    int fragShift = fragIndex << 2;

    o_color += fragment_fetch(s_image, coord, fragIndex)
      * float(bitfieldExtract(fragCount, fragShift, 4));

    fragCount = bitfieldInsert(fragCount, 0, fragShift, 4);
  }

  o_color /= float(c_samples);
}
