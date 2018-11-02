dxbc_src = files([
  'dxbc_analysis.cpp',
  'dxbc_chunk_isgn.cpp',
  'dxbc_chunk_shex.cpp',
  'dxbc_common.cpp',
  'dxbc_compiler.cpp',
  'dxbc_defs.cpp',
  'dxbc_decoder.cpp',
  'dxbc_header.cpp',
  'dxbc_module.cpp',
  'dxbc_names.cpp',
  'dxbc_options.cpp',
  'dxbc_reader.cpp',
  'dxbc_util.cpp',
])

dxbc_lib = static_library('dxbc', dxbc_src,
  include_directories : [ dxvk_include_path ],
  override_options    : ['cpp_std='+dxvk_cpp_std])

dxbc_dep = declare_dependency(
  link_with           : [ dxbc_lib ],
  include_directories : [ dxvk_include_path ])
