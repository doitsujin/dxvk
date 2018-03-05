spirv_src = files([
  'spirv_code_buffer.cpp',
  'spirv_module.cpp',
])

spirv_lib = static_library('spirv', spirv_src,
  include_directories : [ dxvk_include_path ],
  dependencies        : [ lib_spirvtools, lib_spirvtools_opt ],
  override_options    : ['cpp_std='+dxvk_cpp_std])
