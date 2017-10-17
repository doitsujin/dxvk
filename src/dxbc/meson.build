dxbc_src = files([
  'dxbc_annotation.cpp',
  'dxbc_capability.cpp',
  'dxbc_chunk_shex.cpp',
  'dxbc_common.cpp',
  'dxbc_compiler.cpp',
  'dxbc_entrypoint.cpp',
  'dxbc_header.cpp',
  'dxbc_module.cpp',
  'dxbc_reader.cpp',
  'dxbc_typeinfo.cpp',
])

dxbc_lib = static_library('dxbc', dxbc_src,
  include_directories : [ dxvk_include_path ])

dxbc_dep = declare_dependency(
  link_with           : [ dxbc_lib ],
  include_directories : [ dxvk_include_path, include_directories('.') ])
