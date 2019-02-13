dx9asm_src = files([
  'dx9asm_decoder.cpp',
])

dx9asm_lib = static_library('dx9asm', dx9asm_src,
  include_directories : [ dxvk_include_path ],
  override_options    : ['cpp_std='+dxvk_cpp_std])

dx9asm_dep = declare_dependency(
  link_with           : [ dx9asm_lib ],
  include_directories : [ dxvk_include_path ])
