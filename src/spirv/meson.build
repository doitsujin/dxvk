spirv_src = files([
  'spirv_code_buffer.cpp',
  
  'gen/spirv_gen_capability.cpp',
  'gen/spirv_gen_constant.cpp',
  'gen/spirv_gen_debuginfo.cpp',
  'gen/spirv_gen_decoration.cpp',
  'gen/spirv_gen_entrypoint.cpp',
  'gen/spirv_gen_typeinfo.cpp',
  'gen/spirv_gen_variable.cpp',
])

spirv_lib = static_library('spirv', spirv_src,
  include_directories : [ dxvk_include_path ])
