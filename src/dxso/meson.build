dxso_src = files([
  'dxso_common.cpp',
  'dxso_options.cpp',
  'dxso_module.cpp',
  'dxso_reader.cpp',
  'dxso_header.cpp',
  'dxso_ctab.cpp',
  'dxso_util.cpp',
  'dxso_code.cpp',
  'dxso_tables.cpp',
  'dxso_decoder.cpp',
  'dxso_analysis.cpp',
  'dxso_compiler.cpp',
  'dxso_enums.cpp'
])

dxso_lib = static_library('dxso', dxso_src,
  include_directories : [ dxvk_include_path ],
  override_options    : ['cpp_std='+dxvk_cpp_std])

dxso_dep = declare_dependency(
  link_with           : [ dxso_lib ],
  include_directories : [ dxvk_include_path ])
