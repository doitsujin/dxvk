d3d10_core_res = wrc_generator.process('version10_core.rc')

d3d10_core_src = [
  'd3d10_core.cpp',
]

d3d10_core_dll = shared_library('d3d10core'+dll_ext, d3d10_core_src, d3d10_core_res,
  name_prefix         : '',
  dependencies        : [ d3d11_dep ],
  include_directories : dxvk_include_path,
  install             : true,
  vs_module_defs      : 'd3d10core'+def_spec_ext,
)

d3d10_core_dep = declare_dependency(
  link_with           : [ d3d10_core_dll ],
)
